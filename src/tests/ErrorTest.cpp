#include <gtest/gtest.h>

#include <dssperror/ErrorSet.h>
#include <dssphttp/HttpErrors.h>
#include <dsspruntime/SrtErrors.h>

namespace dssp::tests {

    class StageError final : public ErrorType<StageError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "test.stage"; }
    };

    using NarrowErrors = ErrorSet<http::ValidationError>;
    using WideErrors = ErrorSet<http::ValidationError, StageError>;
    static_assert(!std::is_constructible_v<NarrowErrors, StageError>);
    static_assert(!std::is_constructible_v<WideErrors, Error>);
    static_assert(std::is_nothrow_move_constructible_v<WideErrors>);

    template <class To, class From>
    concept CanWiden = requires(From source) { widen_errors<To>(std::move(source)); };
    static_assert(CanWiden<WideErrors, NarrowErrors>);
    static_assert(!CanWiden<NarrowErrors, WideErrors>);

    TEST(Error, CloneRetainsDerivedFieldsAndCause) {
        http::ValidationError input({{"/input/audioUrl", "format", "Expected a URI"}});
        StageError stage("Preparation failed", input.clone());
        const auto cause = stage.cause();
        const auto concrete = dynamic_cast<const http::ValidationError *>(cause.get());
        ASSERT_NE(concrete, nullptr);
        ASSERT_EQ(concrete->issues().size(), 1);
        EXPECT_EQ(concrete->issues()[0].pointer, "/input/audioUrl");
        EXPECT_EQ(stage.formatChain(), "Preparation failed: Request fields do not conform to the API schema");
        EXPECT_EQ(stage.clone()->kind(), "test.stage");
    }

    TEST(Error, WideningRetainsTheConcreteAlternative) {
        NarrowErrors source{http::ValidationError({{"/context", "required", "Missing"}})};
        auto result = widen_errors<WideErrors>(std::move(source));
        ASSERT_NE(result.get_if<http::ValidationError>(), nullptr);
        EXPECT_EQ(result.get_if<http::ValidationError>()->issues()[0].detail, "Missing");
        EXPECT_EQ(result.get_if<StageError>(), nullptr);
    }

    TEST(Error, NativeBoundaryPreservesChainWithoutLeakingNativeErrors) {
        auto native = srt::Error(srt::Error::InvalidFormat, "Invalid model output").withCause(srt::Error(srt::Error::FileNotOpen, "Model file unavailable"));
        auto result = runtime::adaptSrtResult<int, StageError>(srt::Expected<int>(native), [](const srt::Error &error) { return StageError("Acoustic stage failed", runtime::snapshotSrtError(error, "acoustic")); });
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().cause()->message(), "Invalid model output");
        EXPECT_EQ(result.error().cause()->cause()->message(), "Model file unavailable");
        const auto diagnostic = dynamic_cast<const ExternalError *>(result.error().cause().get());
        ASSERT_NE(diagnostic, nullptr);
        EXPECT_EQ(diagnostic->diagnostic().operation, "acoustic");
        EXPECT_EQ(diagnostic->diagnostic().code, srt::Error::InvalidFormat);
    }

    TEST(Error, NativeBoundarySupportsVoidAndSuccessfulValues) {
        const auto map = [](const srt::Error &error) { return StageError("Native call failed", runtime::snapshotSrtError(error, "test")); };
        auto value = runtime::adaptSrtResult<int, StageError>(srt::Expected<int>(42), map);
        ASSERT_TRUE(value);
        EXPECT_EQ(*value, 42);
        EXPECT_TRUE((runtime::adaptSrtResult<void, StageError>(srt::Expected<void>{}, map)));
    }

}
