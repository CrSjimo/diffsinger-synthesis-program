#ifndef DSSP_DEVICEINFO_H
#define DSSP_DEVICEINFO_H

#include <cstdint>
#include <string>
#include <vector>

namespace dssp{

    class DeviceInfo {
    public:
        enum class ExecutionProvider {
            CPU,
            CUDA,
            DirectML,
            CoreML,
        };

        ExecutionProvider executionProvider() const noexcept {
            return executionProvider_;
        }
        int index() const noexcept {
            return index_;
        }
        const std::string &description() const noexcept {
            return description_;
        }
        const std::string &id() const noexcept {
            return id_;
        }
        std::size_t memory() const noexcept {
            return memory_;
        }

        static std::vector<DeviceInfo> listDevices();
        static DeviceInfo defaultDevice();
        static std::string executionProviderToString(ExecutionProvider ep);
        static ExecutionProvider executionProviderFromString(const std::string &s);

    private:
        ExecutionProvider executionProvider_;
        int index_;
        std::string description_;
        std::string id_;
        std::size_t memory_;

        explicit DeviceInfo(ExecutionProvider executionProvider, int index, std::string description, std::string id, std::size_t memory)
            : executionProvider_(executionProvider), index_(index), description_(std::move(description)), id_(std::move(id)), memory_(memory) {}

    };

}

#endif //DSSP_DEVICEINFO_H
