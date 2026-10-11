#include "Tempo.h"
#include <atomic>
#include <cmath>
#include <dsspmedia/Wave.h>

namespace dssp::extraction {
    namespace {
        namespace Beat = srtbeatinfer::Api::Beat::L1;
        struct Request {
            Request(std::shared_ptr<const ExtractorCatalog> catalog, const ExtractorRecord &record, std::shared_ptr<resource::TaskExecutor> tasks, std::stop_token externalStop, Completion<TempoOutput> complete)
                : catalog(std::move(catalog)), record(record), tasks(std::move(tasks)), complete(std::move(complete)), cancel(externalStop, [this] { stop.request_stop(); }) {}

            std::shared_ptr<const ExtractorCatalog> catalog;
            const ExtractorRecord &record;
            std::shared_ptr<resource::TaskExecutor> tasks;
            Completion<TempoOutput> complete;
            std::stop_source stop;
            std::stop_callback<std::function<void()>> cancel;
            std::atomic_flag done = ATOMIC_FLAG_INIT;
            void finish(ExtractionResult<TempoOutput> result) {
                if (done.test_and_set(std::memory_order_acq_rel)) return;
                if (stop.stop_requested()) result = fail<ExtractionErrors>(CancelledError("Tempo extraction was cancelled for " + record.id));
                complete(std::move(result));
            }
        };

        drogon::Task<bool> background(std::shared_ptr<Request> request) {
            auto ready = co_await request->tasks->resume(request->stop.get_token());
            if (!ready) {
                if (ready.error().get_if<resource::TaskCancelledError>()) request->finish(fail<ExtractionErrors>(CancelledError("Tempo background task was cancelled")));
                else request->finish(fail<ExtractionErrors>(OperationError("Cannot run tempo background task", ready.error().asError().clone())));
                co_return false;
            }
            if (request->stop.stop_requested()) {
                request->finish(fail<ExtractionErrors>(CancelledError("Tempo extraction was cancelled")));
                co_return false;
            }

            co_return true;
        }
    }

    void extractTempo(std::shared_ptr<const ExtractorCatalog> catalog, const ExtractorRecord &record, std::shared_ptr<resource::TaskExecutor> tasks, Input input, std::stop_token stop, Completion<TempoOutput> complete) {
        const auto request = std::make_shared<Request>(std::move(catalog), record, std::move(tasks), stop, std::move(complete));
        drogon::async_run([request, input = std::move(input)]() mutable -> drogon::Task<> {
            if (!(co_await background(request))) co_return;
            auto audio = media::readWaveDataUrl(input.audioUrl, request->stop.get_token());
            input.audioUrl.clear();
            if (request->stop.stop_requested()) { request->finish(fail<ExtractionErrors>(CancelledError("Tempo audio decoding was cancelled"))); co_return; }
            if (!audio) {
                if (audio.error().get_if<media::InvalidMediaError>()) request->finish(fail<ExtractionErrors>(InvalidInputError("Invalid tempo audio: " + audio.error().asError().message(), audio.error().asError().clone())));
                else request->finish(fail<ExtractionErrors>(OperationError("Cannot decode tempo audio", audio.error().asError().clone())));
                co_return;
            }
            if (audio->sampleRate != request->record.sampleRate) {
                request->finish(fail<ExtractionErrors>(InvalidInputError("Tempo extractor requires " + std::to_string(request->record.sampleRate) + " Hz audio")));
                co_return;
            }

            const auto frames = audio->samples.size() / audio->channelCount;
            const auto duration = static_cast<double>(frames) / audio->sampleRate;
            Beat::BeatStartInput native;
            native.audio = {audio->sampleRate, 1, {}, 0};
            native.audio.samples.resize(frames);
            for (std::size_t frame = 0; frame < frames; ++frame) {
                if ((frame & 65535) == 0 && request->stop.stop_requested()) { request->finish(fail<ExtractionErrors>(CancelledError("Tempo channel mixing was cancelled"))); co_return; }
                double value = 0;
                for (int channel = 0; channel < audio->channelCount; ++channel) value += audio->samples[frame * audio->channelCount + channel];
                native.audio.samples[frame] = static_cast<float>(value / audio->channelCount);
            }

            audio->samples.clear();
            // Beat tracking uses uninterrupted audio; silence slicing would lose measure context.
            auto result = co_await request->record.pool->useCoro<Beat::BeatExecutive>(request->record.resource, request->stop.get_token(), [&native](Beat::BeatExecutive &executive, std::stop_token token) -> inference::ExecutionResult<std::unique_ptr<Beat::BeatResult>> {
                if (token.stop_requested()) return fail<inference::ExecutionErrors>(inference::CancelledError("Tempo inference was cancelled before execution"));
                auto ran = inference::nativeCall("srtbeatinfer BeatExecutive::start", [&] { return executive.start(native); });
                if (executive.state() == srt::ITask::Canceled) return fail<inference::ExecutionErrors>(inference::CancelledError("Tempo inference was cancelled"));
                if (!ran) return fail<inference::ExecutionErrors>(inference::ExecutionError("Tempo inference failed", ran.error().clone()));
                if (!*ran) return fail<inference::ExecutionErrors>(inference::ExecutionError("Beat tracker returned no result"));
                return std::move(*ran);
            });
            if (!(co_await background(request))) co_return;
            if (!result) {
                if (result.error().get_if<inference::CancelledError>()) request->finish(fail<ExtractionErrors>(CancelledError("Tempo inference was cancelled", result.error().asError().clone())));
                else request->finish(fail<ExtractionErrors>(OperationError("Tempo inference failed for " + request->record.id, result.error().asError().clone())));
                co_return;
            }

            TempoOutput output;
            double previous = -1;
            for (const auto &beat : (*result)->beats) {
                if (!std::isfinite(beat.time) || beat.time < 0 || beat.time > duration || beat.time <= previous) {
                    request->finish(fail<ExtractionErrors>(OperationError("Beat tracker returned invalid or unordered timestamps")));
                    co_return;
                }
                output.beats.push_back({beat.time, beat.downbeat});
                previous = beat.time;
            }

            request->finish(std::move(output));
        });
    }
}
