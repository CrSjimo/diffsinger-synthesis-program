#ifndef DSSP_DEVICEINFO_H
#define DSSP_DEVICEINFO_H

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <dsspruntime/DeviceError.h>

namespace dssp::runtime {

    class DeviceInfo {
    public:
        enum class ExecutionProvider {
            CPU,
            CUDA,
            DirectML,
            CoreML,
        };

        explicit DeviceInfo(ExecutionProvider executionProvider, int index, std::string description, std::string id, std::size_t memory) : m_executionProvider(executionProvider), m_index(index), m_description(std::move(description)), m_id(std::move(id)), m_memory(memory) {}

        ExecutionProvider executionProvider() const noexcept { return m_executionProvider; }
        int index() const noexcept { return m_index; }
        const std::string &description() const noexcept { return m_description; }
        const std::string &id() const noexcept { return m_id; }
        std::size_t memory() const noexcept { return m_memory; }

        static std::expected<std::vector<DeviceInfo>, DeviceError> listDevices(std::optional<ExecutionProvider> provider = {});
        static ExecutionProvider defaultExecutionProvider() noexcept;
        static std::expected<DeviceInfo, DeviceError> defaultDevice(ExecutionProvider provider = defaultExecutionProvider());
        // Select from an existing inventory without enumerating hardware again.
        static std::expected<DeviceInfo, DeviceError> defaultDevice(std::span<const DeviceInfo> devices, ExecutionProvider provider = defaultExecutionProvider());
        static std::string executionProviderToString(ExecutionProvider ep);
        static std::expected<ExecutionProvider, DeviceError> executionProviderFromString(const std::string &s);

    private:
        ExecutionProvider m_executionProvider;
        int m_index;
        std::string m_description;
        std::string m_id;
        std::size_t m_memory;
    };

    using ExecutionProvider = DeviceInfo::ExecutionProvider;

}

#endif // DSSP_DEVICEINFO_H
