#include "Error.h"

namespace dssp::error {

    struct Error::Data {
        std::string message;
        Ptr cause;
        std::source_location location;
    };

    Error::Error(std::string message, Ptr cause, std::source_location location) : m_data(std::make_shared<const Data>(Data{std::move(message), std::move(cause), location})) {}

    const std::string &Error::message() const noexcept {
        return m_data->message;
    }

    Error::Ptr Error::cause() const noexcept {
        return m_data->cause;
    }

    std::source_location Error::location() const noexcept {
        return m_data->location;
    }

    std::string_view Error::kind() const noexcept {
        return "dssp.error";
    }

    Error::Ptr Error::clone() const {
        return std::make_shared<const Error>(*this);
    }

    std::string Error::formatChain() const {
        std::string text;
        for (auto current = this; current; current = current->cause().get()) {
            if (!text.empty()) {
                text += ": ";
            }
            text += current->message();
        }

        return text;
    }

}
