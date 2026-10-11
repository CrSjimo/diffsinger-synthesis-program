#include "MediaType.h"

#include <exception>
#include <memory>

#include <re2/re2.h>

namespace dssp::http {
    namespace {
        using Pattern = std::expected<std::unique_ptr<re2::RE2>, HttpBoundaryError>;

        Pattern compilePattern(std::string_view source) {
            // Keep RE2 diagnostics in the application's error/logging boundary.
            try {
                re2::RE2::Options options;
                options.set_log_errors(false);
                options.set_encoding(re2::RE2::Options::EncodingLatin1);
                auto pattern = std::make_unique<re2::RE2>(absl::string_view(source.data(), source.size()), options);
                if (!pattern->ok()) {
                    return std::unexpected{HttpBoundaryError("Media type validator initialization failed", std::make_shared<ExternalError>(pattern->error(), NativeDiagnostic{"re2", "compile media type pattern", "re2", pattern->error_code()}))};
                }

                return pattern;
            } catch (const std::exception &error) {
                return std::unexpected{HttpBoundaryError("Media type validator initialization failed", std::make_shared<ExternalError>(error.what(), NativeDiagnostic{"re2", "compile media type pattern", "exception"}))};
            }
        }

        std::expected<bool, HttpBoundaryError> match(std::string_view text, const Pattern &pattern) {
            if (!pattern) {
                return std::unexpected{pattern.error()};
            }

            try {
                return re2::RE2::FullMatch(absl::string_view(text.data(), text.size()), **pattern);
            } catch (const std::exception &error) {
                return std::unexpected{HttpBoundaryError("Media type validator failed", std::make_shared<ExternalError>(error.what(), NativeDiagnostic{"re2", "match media type", "exception"}))};
            }
        }
    }

    std::expected<bool, HttpBoundaryError> matchesMediaTypeName(std::string_view text) {
        // RFC 6838 section 4.2: each name is 1..127 ASCII characters. FullMatch
        // rejects parameters, wildcards, whitespace and embedded/trailing data.
        static const auto pattern = compilePattern(R"([A-Za-z0-9][A-Za-z0-9!#$&^_.+-]{0,126}/[A-Za-z0-9][A-Za-z0-9!#$&^_.+-]{0,126})");
        return match(text, pattern);
    }

    std::expected<bool, HttpBoundaryError> matchesJsonContentType(std::string_view text) {
        // RFC 9110 sections 5.6.4, 5.6.6 and 8.3.1: token/quoted-string
        // parameters, quoted-pair escaping, OWS and empty parameters. Latin1
        // preserves HTTP obs-text bytes without Unicode normalization.
        static const auto pattern = compilePattern(R"([ \t]*(?i:application/json)(?:[ \t]*;[ \t]*(?:)"
                                                   R"([!#$%&'*+.^_`|~0-9A-Za-z-]+=(?:[!#$%&'*+.^_`|~0-9A-Za-z-]+|)"
                                                   R"re("(?:[\t\x20-\x21\x23-\x5B\x5D-\x7E\x80-\xFF]|\\[\t\x20-\x7E\x80-\xFF])*"))?)*[ \t]*)re");
        return match(text, pattern);
    }
}
