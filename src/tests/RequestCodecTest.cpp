#include <gtest/gtest.h>

#include <dsspapiv1/RequestCodec.h>
#include <dssphttp/Uri.h>

namespace dssp::tests {

    namespace v1 = api_v1;
    namespace dto = api_v1::dto;

    Json::Value context(bool multi = true) {
        Json::Value result;
        result["arch"] = "future-architecture";
        result["archExtra"] = Json::Value(Json::arrayValue);
        Json::Value singer;
        singer["id"] = "unknown-resource";
        singer["extra"] = 42;
        if (multi) {
            result["singers"] = Json::Value(Json::arrayValue);
            result["singers"].append(singer);
        } else {
            result["singer"] = singer;
        }
        return result;
    }

    Json::Value pieceRequest() {
        Json::Value request;
        request["context"] = context();
        auto &input = request["input"];
        input["pieceDuration"] = 0.0;
        input["notes"] = Json::Value(Json::arrayValue);
        input["mix"] = Json::Value(Json::arrayValue);
        input["mixSampleRate"] = 100;
        input["parameters"] = Json::Value(Json::objectValue);
        return request;
    }

    TEST(RequestCodec, ContextExtensionsAreOpaqueAndSingerOrderIsRetained) {
        Json::Value json;
        json["context"] = context();
        auto second = json["context"]["singers"][0];
        second["id"] = "second";
        json["context"]["singers"].append(second);
        auto result = v1::fromJson<dto::EnvTagRequest>(json);
        ASSERT_TRUE(result);
        EXPECT_TRUE(result->context.archExtra.isArray());
        EXPECT_EQ(result->context.singers[0].extra.asInt(), 42);
        EXPECT_EQ(result->context.singers[1].id, "second");
    }

    TEST(RequestCodec, MissingAndNullAreDifferentForRequiredFields) {
        Json::Value json;
        json["context"] = context();
        json["context"].removeMember("archExtra");
        auto missing = v1::fromJson<dto::EnvTagRequest>(json);
        ASSERT_FALSE(missing);
        ASSERT_NE(missing.error().get_if<http::ValidationError>(), nullptr);
        EXPECT_EQ(missing.error().get_if<http::ValidationError>()->issues()[0].pointer, "/context/archExtra");
        json["context"]["archExtra"] = Json::Value{};
        EXPECT_TRUE(v1::fromJson<dto::EnvTagRequest>(json));
        json["context"]["singers"] = Json::Value{};
        EXPECT_FALSE(v1::fromJson<dto::EnvTagRequest>(json));
    }

    TEST(RequestCodec, EmptySingerArrayFailsMinItems) {
        Json::Value json;
        json["context"] = context();
        json["context"]["singers"] = Json::Value(Json::arrayValue);
        auto result = v1::fromJson<dto::EnvTagRequest>(json);
        ASSERT_FALSE(result);
        ASSERT_NE(result.error().get_if<http::ValidationError>(), nullptr);
        EXPECT_EQ(result.error().get_if<http::ValidationError>()->issues()[0].errorType, "minItems");
    }

    TEST(RequestCodec, ParameterValuesAreRequiredButAudioValuesCanBeOmitted) {
        auto json = pieceRequest();
        json["input"]["parameters"]["pitch"]["sampleRate"] = 100;
        EXPECT_TRUE(v1::fromJson<dto::AudioRequest>(json));
        auto missing = v1::fromJson<dto::ParameterRequest>(json);
        ASSERT_FALSE(missing);
        ASSERT_NE(missing.error().get_if<http::ValidationError>(), nullptr);
        EXPECT_EQ(missing.error().get_if<http::ValidationError>()->issues()[0].pointer, "/input/parameters/pitch/values");
        json["input"]["parameters"]["pitch"]["values"] = Json::Value(Json::arrayValue);
        EXPECT_TRUE(v1::fromJson<dto::ParameterRequest>(json));
        json["input"]["parameters"]["pitch"]["values"] = Json::Value{};
        EXPECT_FALSE(v1::fromJson<dto::AudioRequest>(json));
    }

    TEST(RequestCodec, PointerEscapesMapKeysAndAggregatesFailures) {
        auto json = pieceRequest();
        json["input"]["parameters"]["a~/b"]["sampleRate"] = 0;
        auto result = v1::fromJson<dto::ParameterRequest>(json);
        ASSERT_FALSE(result);
        const auto error = result.error().get_if<http::ValidationError>();
        ASSERT_NE(error, nullptr);
        ASSERT_EQ(error->issues().size(), 2);
        EXPECT_EQ(error->issues()[0].pointer, "/input/parameters/a~0~1b/sampleRate");
        EXPECT_EQ(error->issues()[1].errorType, "required");
    }

    TEST(RequestCodec, ServiceRulesAreNotEvaluatedByControllerValidation) {
        auto json = pieceRequest();
        Json::Value note;
        note["position"]["gap"] = 10;
        note["position"]["duration"] = 10;
        note["cent"] = 12800;
        note["pronunciation"] = "";
        note["language"] = "not-a-model-language";
        note["phonemes"] = Json::Value(Json::arrayValue);
        note["kind"] = "slur";
        json["input"]["notes"].append(note);
        Json::Value row(Json::arrayValue);
        row.append(0.9);
        row.append(0.9);
        json["input"]["mix"].append(row);
        auto &parameter = json["input"]["parameters"]["undefined-parameter"];
        parameter["sampleRate"] = 100;
        parameter["values"] = Json::Value(Json::arrayValue);
        parameter["values"].append(2.0);
        parameter["retake"]["position"] = 10000;
        parameter["retake"]["length"] = 10000;
        EXPECT_TRUE(v1::fromJson<dto::DurationRequest>(json));
        // Timed phonemes are empty, so this also has a valid Parameter DTO.
        EXPECT_TRUE(v1::fromJson<dto::ParameterRequest>(json));
        json["input"]["notes"][0]["cent"] = 12801;
        EXPECT_FALSE(v1::fromJson<dto::DurationRequest>(json));
    }

    TEST(RequestCodec, SliceEndpointsAreNullableAndNegativeIntegersAreAllowed) {
        Json::Value json;
        json["context"] = context(false);
        Json::Value note;
        note["pronunciation"] = "hello";
        note["language"] = "eng";
        note["syllableSliceStart"] = -20;
        note["syllableSliceEnd"] = Json::Value{};
        json["input"]["notes"] = Json::Value(Json::arrayValue);
        json["input"]["notes"].append(note);
        EXPECT_TRUE(v1::fromJson<dto::PhonemeRequest>(json));
        json["input"]["notes"][0]["syllableSliceStart"] = 0.5;
        EXPECT_FALSE(v1::fromJson<dto::PhonemeRequest>(json));
    }

    TEST(RequestCodec, UriValidationDoesNotValidateMediaOrSelectSchemes) {
        for (const auto url : {"https://example.com/audio", "file:///tmp/input.wav", "data:audio/flac;base64,broken"}) {
            Json::Value json;
            json["extractor"] = "missing-model";
            json["input"]["audioUrl"] = url;
            EXPECT_TRUE(v1::fromJson<dto::ExtractionRequest>(json));
        }
        for (const auto url : {"relative/path", "http://[invalid]/", "https://example.com/%XY", "data:hello world"}) {
            EXPECT_FALSE(http::validateUri(url));
        }
        EXPECT_TRUE(http::validateUri("https://[::1]:13711/a%20b?x=1#fragment"));
        EXPECT_TRUE(http::validateUri("urn:dssp:resource"));
    }

    TEST(RequestCodec, UriValidationPreservesEscapesWithoutDecodingOrNormalizingThePayload) {
        for (const auto url : {"https://example.com/a%2Fb%252Fc?source=%23fragment&token=%25%2B#part%2Fone", "data:audio/wav;base64,YQ%2B%2F%3D%3D", "data:application/octet-stream,%00%25%2B%2F%3F%23"}) {
            SCOPED_TRACE(url);
            Json::Value json;
            json["extractor"] = "missing-model";
            json["input"]["audioUrl"] = url;
            const auto decoded = v1::fromJson<dto::ExtractionRequest>(json);
            ASSERT_TRUE(decoded);
            EXPECT_EQ(decoded->input.audioUrl, url);
        }
        for (const auto url : {"https://example.com/%", "https://example.com/%2", "https://example.com/%GG", "data:audio/wav,%2Z"}) {
            SCOPED_TRACE(url);
            EXPECT_FALSE(http::validateUri(url));
        }
    }

    TEST(RequestCodec, AudioFormatsRetainOrderAndRejectParametersAndWildcards) {
        auto json = pieceRequest();
        json["acceptableFormats"] = Json::Value(Json::arrayValue);
        json["acceptableFormats"].append("Audio/X-WAV");
        json["acceptableFormats"].append("application/vnd.example+json");
        auto result = v1::fromJson<dto::AudioRequest>(json);
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->acceptableFormats);
        ASSERT_EQ(result->acceptableFormats->size(), 2U);
        EXPECT_EQ((*result->acceptableFormats)[0], "Audio/X-WAV");
        EXPECT_EQ((*result->acceptableFormats)[1], "application/vnd.example+json");
        for (const auto invalid : {"audio/wav;codecs=pcm", "audio/*", "*/*"}) {
            SCOPED_TRACE(invalid);
            json["acceptableFormats"][0] = invalid;
            auto rejected = v1::fromJson<dto::AudioRequest>(json);
            ASSERT_FALSE(rejected);
            const auto error = rejected.error().get_if<http::ValidationError>();
            ASSERT_NE(error, nullptr);
            ASSERT_EQ(error->issues().size(), 1);
            EXPECT_EQ(error->issues()[0].pointer, "/acceptableFormats/0");
            EXPECT_EQ(error->issues()[0].errorType, "format");
            EXPECT_FALSE(error->issues()[0].detail.empty());
        }
    }

    TEST(RequestCodec, MediaQueryKeepsPriorityAndDoesNotSplitEncodedCommas) {
        auto request = drogon::HttpRequest::newHttpRequest();
        request->setParameter("acceptableFormats", "Audio/x-wav,application/vnd.example+json");
        request->setParameter("acceptableSchemes", "data,https");
        auto result = v1::mediaQuery(*request);
        ASSERT_TRUE(result);
        ASSERT_EQ(result->acceptableFormats.size(), 2);
        EXPECT_EQ(result->acceptableFormats[0], "Audio/x-wav");
        EXPECT_EQ(result->acceptableFormats[1], "application/vnd.example+json");
        ASSERT_EQ(result->acceptableSchemes.size(), 2);
        EXPECT_EQ(result->acceptableSchemes[0], "data");
        EXPECT_EQ(result->acceptableSchemes[1], "https");
        request = drogon::HttpRequest::newHttpRequest();
        request->setQueryParameter("acceptableFormats", "application/vnd.example+json");
        auto encoded = v1::mediaQuery(*request);
        ASSERT_TRUE(encoded);
        ASSERT_EQ(encoded->acceptableFormats.size(), 1);
        EXPECT_EQ(encoded->acceptableFormats[0], "application/vnd.example+json");
        request = drogon::HttpRequest::newHttpRequest();
        request->setQueryParameter("acceptableFormats", "audio/wav,audio/flac");
        auto rejected = v1::mediaQuery(*request);
        ASSERT_FALSE(rejected);
        const auto error = rejected.error().get_if<http::ValidationError>();
        ASSERT_NE(error, nullptr);
        ASSERT_EQ(error->issues().size(), 1);
        EXPECT_EQ(error->issues()[0].pointer, "/acceptableFormats/0");
        EXPECT_EQ(error->issues()[0].errorType, "format");
    }

}
