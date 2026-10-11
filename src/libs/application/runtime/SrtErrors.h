#ifndef DSSP_SRTERRORS_H
#define DSSP_SRTERRORS_H

#include <expected>
#include <functional>
#include <vector>

#include <synthrt/Support/Expected.h>

#include <dssperror/Error.h>

namespace dssp::runtime {

    // Convert diagnostics while the native error category and plugin are alive.
    // The caller chooses the application's semantic error; native codes cannot
    // determine whether a failure is invalid input or an internal model fault.
    inline Error::Ptr snapshotSrtError(const srt::Error &error, std::string operation) {
        std::vector<srt::Error> chain;
        for (auto current = error; !current.ok(); current = current.cause()) {
            chain.push_back(current);
        }

        Error::Ptr cause;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            cause = std::make_shared<const ExternalError>(it->message(), NativeDiagnostic{"synthrt", operation, it->code().category().name(), it->code().value()}, std::move(cause));
        }

        return cause;
    }

    template <class T, class E, class MapError>
    std::expected<T, E> adaptSrtResult(srt::Expected<T> result, MapError &&mapError) {
        if (!result) {
            return std::unexpected<E>{std::invoke(std::forward<MapError>(mapError), result.error())};
        }

        if constexpr (std::is_void_v<T>) {
            return {};
        } else {
            return std::move(*result);
        }
    }

}

#endif // DSSP_SRTERRORS_H
