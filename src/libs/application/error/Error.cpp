#include "Error.h"

namespace dssp::error {

    struct Error::Data {
        std::string message;
        Ptr cause;
        std::source_location location;
    };

    Error::Error(std::string message, Ptr cause, std::source_location location) : data_(std::make_shared<const Data>(Data{std::move(message), std::move(cause), location})) {}

    const std::string &Error::message() const noexcept {
        return data_->message;
    }

    Error::Ptr Error::cause() const noexcept {
        return data_->cause;
    }

    std::source_location Error::location() const noexcept {
        return data_->location;
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
