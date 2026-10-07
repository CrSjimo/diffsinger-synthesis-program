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

        explicit DeviceInfo(ExecutionProvider executionProvider, int index, std::string description, std::string id, std::size_t memory) : executionProvider_(executionProvider), index_(index), description_(std::move(description)), id_(std::move(id)), memory_(memory) {}

        ExecutionProvider executionProvider() const noexcept { return executionProvider_; }
        int index() const noexcept { return index_; }
        const std::string &description() const noexcept { return description_; }
        const std::string &id() const noexcept { return id_; }
        std::size_t memory() const noexcept { return memory_; }

        static std::expected<std::vector<DeviceInfo>, DeviceError> listDevices(std::optional<ExecutionProvider> provider = {});
        static ExecutionProvider defaultExecutionProvider() noexcept;
        static std::expected<DeviceInfo, DeviceError> defaultDevice(ExecutionProvider provider = defaultExecutionProvider());
        // Select from an existing inventory without enumerating hardware again.
        static std::expected<DeviceInfo, DeviceError> defaultDevice(std::span<const DeviceInfo> devices, ExecutionProvider provider = defaultExecutionProvider());
        static std::string executionProviderToString(ExecutionProvider ep);
        static std::expected<ExecutionProvider, DeviceError> executionProviderFromString(const std::string &s);

    private:
        ExecutionProvider executionProvider_;
        int index_;
        std::string description_;
        std::string id_;
        std::size_t memory_;
    };

    using ExecutionProvider = DeviceInfo::ExecutionProvider;

}

#endif // DSSP_DEVICEINFO_H
