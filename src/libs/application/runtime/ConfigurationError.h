#ifndef DSSP_CONFIG_CONFIGURATIONERROR_H
#define DSSP_CONFIG_CONFIGURATIONERROR_H

#include <dssperror/Error.h>

namespace dssp::runtime {
    class ConfigurationError final : public ErrorType<ConfigurationError> {
    public:
        ConfigurationError(std::string option, std::string message, Ptr cause = {}, std::source_location location = std::source_location::current()) : ErrorType(std::move(message), std::move(cause), location), m_option(std::make_shared<const std::string>(std::move(option))) {}
        const std::string &option() const noexcept { return *m_option; }
        std::string_view kind() const noexcept override { return "dssp.configuration"; }

    private:
        std::shared_ptr<const std::string> m_option;
    };
}

#endif // DSSP_CONFIG_CONFIGURATIONERROR_H
