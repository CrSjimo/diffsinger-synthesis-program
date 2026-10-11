#include <srtbeatinfer/Beat/BeatError.h>

namespace srtbeatinfer {
    const std::error_category &beatErrorCategory() noexcept {
        class Category final : public std::error_category {
        public:
            const char *name() const noexcept override { return "srtbeatinfer.beat"; }

            std::string message(int code) const override {
                switch (static_cast<BeatError>(code)) {
                    case BeatError::Cancelled: return "Audio beat was cancelled";
                    case BeatError::NoWorker: return "Cannot start beat worker";
                    case BeatError::Internal: return "Internal beat error";
                    case BeatError::ContractViolation: return "Beat contract violation";
                    case BeatError::ModelFailed: return "Beat model failed";
                    case BeatError::ModelMismatch: return "Beat model tensor mismatch";
                }

                return "Unknown beat error";
            }
        };

        static const Category category;
        return category;
    }
}
