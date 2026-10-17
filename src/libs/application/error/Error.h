#ifndef DSSP_ERROR_ERROR_H
#define DSSP_ERROR_ERROR_H

#include <concepts>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dssp::error {

    // Error is diagnostic data, not a thrown exception. No success sentinel is needed.
    class Error {
    public:
        using Ptr = std::shared_ptr<const Error>;

        explicit Error(std::string message, Ptr cause = {}, std::source_location location = std::source_location::current());
        virtual ~Error() = default;
        Error(const Error &) noexcept = default;
        Error(Error &&) noexcept = default;
        Error &operator=(const Error &) noexcept = default;
        Error &operator=(Error &&) noexcept = default;

        const std::string &message() const noexcept;
        Ptr cause() const noexcept;
        std::source_location location() const noexcept;
        virtual std::string_view kind() const noexcept;
        virtual Ptr clone() const;
        std::string formatChain() const;

    private:
        struct Data;
        std::shared_ptr<const Data> m_data;
    };

    // Concrete errors should be final. Cloning through Error retains their payload.
    template <class Derived>
    class ErrorType : public Error {
    public:
        using Error::Error;
        Ptr clone() const final {
            static_assert(std::is_final_v<Derived>);
            return std::make_shared<const Derived>(static_cast<const Derived &>(*this));
        }
    };

    struct NativeDiagnostic {
        std::string library;
        std::string operation;
        std::string domain;
        long long code = 0;
    };

    // Snapshot native codes instead of keeping pointers into unloadable plugins.
    class ExternalError final : public ErrorType<ExternalError> {
    public:
        ExternalError(std::string message, NativeDiagnostic diagnostic, Ptr cause = {}, std::source_location location = std::source_location::current()) : ErrorType(std::move(message), std::move(cause), location), m_diagnostic(std::make_shared<const NativeDiagnostic>(std::move(diagnostic))) {}
        const NativeDiagnostic &diagnostic() const noexcept { return *m_diagnostic; }
        std::string_view kind() const noexcept override { return "dssp.external"; }

    private:
        std::shared_ptr<const NativeDiagnostic> m_diagnostic;
    };

}

// Preserve the diagnostic type names agreed for the application API.
namespace dssp {
    using error::Error;
    using error::ErrorType;
    using error::ExternalError;
    using error::NativeDiagnostic;
}

#endif // DSSP_ERROR_ERROR_H
