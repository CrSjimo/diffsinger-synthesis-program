#ifndef DSSP_DEVICEINFO_H
#define DSSP_DEVICEINFO_H

#include <cstdint>
#include <expected>
#include <string>
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

        ExecutionProvider executionProvider() const noexcept { return executionProvider_; }
        int index() const noexcept { return index_; }
        const std::string &description() const noexcept { return description_; }
        const std::string &id() const noexcept { return id_; }
        std::size_t memory() const noexcept { return memory_; }

        static std::expected<std::vector<DeviceInfo>, DeviceError> listDevices();
        static DeviceInfo defaultDevice();
        static std::string executionProviderToString(ExecutionProvider ep);
        static std::expected<ExecutionProvider, DeviceError> executionProviderFromString(const std::string &s);

    private:
        ExecutionProvider executionProvider_;
        int index_;
        std::string description_;
        std::string id_;
        std::size_t memory_;

        explicit DeviceInfo(ExecutionProvider executionProvider, int index, std::string description, std::string id, std::size_t memory) : executionProvider_(executionProvider), index_(index), description_(std::move(description)), id_(std::move(id)), memory_(memory) {}
    };

    using ExecutionProvider = DeviceInfo::ExecutionProvider;

}

#endif // DSSP_DEVICEINFO_H
