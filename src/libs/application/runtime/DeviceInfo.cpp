#include "DeviceInfo.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <utility>

#include <stdcorelib/support/logging.h>

#ifdef _WIN32
#    include <dxgi1_6.h>
#    include <windows.h>

#    include <wrl/client.h>
#elif defined(__APPLE__)
#    include <sys/sysctl.h>
#    include <sys/types.h>
#elif defined(__linux__)
#    include <unistd.h>
#endif

namespace dssp::runtime {

    namespace {

        stdc::LogCategory log("dssp.runtime.DeviceInfo");

        constexpr std::size_t kMinGpuVramBytes = 1ULL * 1024 * 1024 * 1024;

        bool isPreferredDirectMLDevice(const DeviceInfo &device) {
            // DXGI IDs encode the PCI device ID followed by the vendor ID.
            const auto &id = device.id();
            unsigned vendor = 0;
            if (id.size() == 8) {
                const auto [end, error] = std::from_chars(id.data() + 4, id.data() + 8, vendor, 16);
                if (error == std::errc{} && end == id.data() + 8 && (vendor == 0x10DE || vendor == 0x1002 || vendor == 0x174B)) {
                    return true;
                }
            }

            auto name = device.description();
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return name == "nvidia";
        }

        struct GpuData {
            int index = -1;
            std::string description;
            std::string id;
            std::size_t memory = 0;
        };

        uint64_t getMemorySize() {
#if defined(_WIN32)
            MEMORYSTATUSEX status{};
            status.dwLength = sizeof(status);
            return GlobalMemoryStatusEx(&status) ? static_cast<uint64_t>(status.ullTotalPhys) : 0;
#elif defined(__APPLE__)
            uint64_t size = 0;
            size_t len = sizeof(size);
            return sysctlbyname("hw.memsize", &size, &len, nullptr, 0) == 0 ? size : 0;
#elif defined(__linux__)
            const long pages = sysconf(_SC_PHYS_PAGES);
            const long pageSize = sysconf(_SC_PAGE_SIZE);
            return (pages > 0 && pageSize > 0) ? static_cast<uint64_t>(pages) * static_cast<uint64_t>(pageSize) : 0;
#else
            return 0;
#endif
        }

#ifdef _WIN32

        std::string wcharToString(const wchar_t *wstr) {
            if (!wstr) {
                return {};
            }

            const int length = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
            if (length <= 0) {
                return {};
            }

            std::string result(static_cast<std::size_t>(length), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], length, nullptr, nullptr);
            result.pop_back();
            return result;
        }

        GpuData createGpuDataFromDesc(const DXGI_ADAPTER_DESC1 &desc, const int index) {
            GpuData data;
            data.index = index;
            data.description = wcharToString(desc.Description);
            char idBuffer[8 + 1] = {'\0'};
            std::snprintf(idBuffer, sizeof(idBuffer), "%04X%04X", desc.DeviceId, desc.VendorId);
            data.id = idBuffer;
            data.memory = desc.DedicatedVideoMemory;
            return data;
        }

        std::expected<std::vector<GpuData>, DeviceError> listDirectMLGpus() {
            Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
            const auto status = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
            if (FAILED(status)) {
                return std::unexpected{DeviceError("Failed to create DXGI factory", std::make_shared<ExternalError>("CreateDXGIFactory1 failed", NativeDiagnostic{"DXGI", "CreateDXGIFactory1", "HRESULT", status}))};
            }

            std::vector<GpuData> gpus;
            Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
            for (int adapterIndex = 0;; ++adapterIndex) {
                const HRESULT enumResult = factory->EnumAdapters1(adapterIndex, &adapter);
                if (enumResult == DXGI_ERROR_NOT_FOUND) {
                    break;
                }
                if (FAILED(enumResult)) {
                    return std::unexpected{DeviceError("Failed to enumerate DXGI adapters", std::make_shared<ExternalError>("EnumAdapters1 failed", NativeDiagnostic{"DXGI", "EnumAdapters1", "HRESULT", enumResult}))};
                }

                DXGI_ADAPTER_DESC1 desc;
                const auto descStatus = adapter->GetDesc1(&desc);
                if (FAILED(descStatus)) {
                    return std::unexpected{DeviceError("Failed to query DXGI adapter description", std::make_shared<ExternalError>("GetDesc1 failed", NativeDiagnostic{"DXGI", "GetDesc1", "HRESULT", descStatus}))};
                }

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                    log.stdcTrace("Excluding DXGI software adapter %1 from DirectML devices", adapterIndex);
                    continue;
                }

                if (desc.DedicatedVideoMemory < kMinGpuVramBytes) {
                    log.stdcTrace("Excluding DXGI adapter %1 with %2 bytes of VRAM (minimum %3)", adapterIndex, desc.DedicatedVideoMemory, kMinGpuVramBytes);
                    continue;
                }

                log.stdcTrace("Discovered DirectML adapter %1 with %2 bytes of VRAM", adapterIndex, desc.DedicatedVideoMemory);

                gpus.push_back(createGpuDataFromDesc(desc, adapterIndex));
            }

            std::sort(gpus.begin(), gpus.end(), [](const GpuData &a, const GpuData &b) { return a.memory > b.memory; });
            return gpus;
        }

#endif // _WIN32

#ifdef DSSP_ENABLE_CUDA

        std::expected<std::string, DeviceError> getCommandOutput() {
            log.stdcTrace("Querying CUDA devices using nvidia-smi");
            constexpr const char *kCommand = "nvidia-smi --query-gpu=index,name,memory.total,uuid,pci.device_id "
                                             "--format=csv,noheader,nounits";

            std::string result;
#    ifdef _WIN32
            FILE *pipe = _popen(kCommand, "r");
#    else
            FILE *pipe = popen(kCommand, "r");
#    endif
            if (!pipe) {
                return std::unexpected{DeviceError("Failed to launch nvidia-smi")};
            }

            char buffer[256];
            while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }

#    ifdef _WIN32
            const auto status = _pclose(pipe);
#    else
            const auto status = pclose(pipe);
#    endif
            if (status != 0) {
                return std::unexpected{DeviceError("nvidia-smi returned a failure status", std::make_shared<ExternalError>("Device query failed", NativeDiagnostic{"nvidia-smi", "query-gpu", "exit-status", status}))};
            }

            return result;
        }

        std::expected<GpuData, DeviceError> parseGpuFromLine(const std::string &line) {
            GpuData data;
            bool insideQuotes = false;
            std::string currentField;
            int fieldIndex = 0;

            bool valid = true;
            const auto processField = [&data, &valid](const int fieldIndex_, const std::string &field) {
                auto parse = [&](auto &number) {
                    const auto [end, error] = std::from_chars(field.data(), field.data() + field.size(), number);
                    valid = valid && error == std::errc{} && end == field.data() + field.size();
                };

                switch (fieldIndex_) {
                    case 0: parse(data.index); break;
                    case 1: data.description = field; break;
                    case 2:
                        parse(data.memory);
                        if (data.memory > static_cast<std::size_t>(-1) / (1024 * 1024)) {
                            valid = false;
                        } else {
                            data.memory *= 1024 * 1024;
                        }
                        break;
                    case 3: data.id = field; break;
                    default: break;
                }
            };

            for (std::size_t i = 0; i < line.size(); ++i) {
                const char ch = line[i];

                if (ch == '"') {
                    insideQuotes = !insideQuotes;
                } else if (!insideQuotes && i + 1 < line.size() && line[i] == ',' && line[i + 1] == ' ') {
                    processField(fieldIndex, currentField);
                    currentField.clear();
                    ++fieldIndex;
                    ++i;
                } else {
                    currentField.append(1, ch);
                }

                if (i == line.size() - 1 && !currentField.empty()) {
                    processField(fieldIndex, currentField);
                }
            }

            if (!valid || insideQuotes || fieldIndex < 3 || data.index < 0) {
                return std::unexpected{DeviceError("Malformed nvidia-smi device record")};
            }

            return data;
        }

        std::expected<std::vector<GpuData>, DeviceError> listCudaGpus() {
            std::vector<GpuData> gpus;

            auto output = getCommandOutput();
            if (!output) {
                return std::unexpected{std::move(output.error())};
            }
            if (output->empty()) {
                return gpus;
            }

            std::istringstream stream(*output);
            std::string line;
            while (std::getline(stream, line)) {
                if (line.empty()) {
                    continue;
                }

                auto data = parseGpuFromLine(line);
                if (!data) {
                    return std::unexpected{std::move(data.error())};
                }
                if (data->memory < kMinGpuVramBytes) {
                    log.stdcTrace("Excluding CUDA device %1 with %2 bytes of VRAM (minimum %3)", data->index, data->memory, kMinGpuVramBytes);
                    continue;
                }

                log.stdcTrace("Discovered CUDA device %1 with %2 bytes of VRAM", data->index, data->memory);
                gpus.push_back(std::move(*data));
            }

            return gpus;
        }

#endif // DSSP_ENABLE_CUDA

    }

    std::expected<std::vector<DeviceInfo>, DeviceError> DeviceInfo::listDevices(std::optional<ExecutionProvider> provider) {
        log.stdcTrace("Enumerating execution devices for %1", provider ? executionProviderToString(*provider) : "all providers");
        std::vector<DeviceInfo> devices;
        if (!provider || *provider == ExecutionProvider::CPU) {
            devices.emplace_back(DeviceInfo{ExecutionProvider::CPU, 0, "CPU", "cpu", getMemorySize()});
        }

#ifdef _WIN32
        if (!provider || *provider == ExecutionProvider::DirectML) {
            auto directMLGpus = listDirectMLGpus();
            if (!directMLGpus) {
                return std::unexpected{std::move(directMLGpus.error())};
            }

            for (const auto &gpu : *directMLGpus) {
                devices.emplace_back(DeviceInfo{ExecutionProvider::DirectML, gpu.index, gpu.description, gpu.id, gpu.memory});
            }
        }
#endif

#ifdef DSSP_ENABLE_CUDA
        if (!provider || *provider == ExecutionProvider::CUDA) {
            auto cudaGpus = listCudaGpus();
            if (!cudaGpus) {
                return std::unexpected{std::move(cudaGpus.error())};
            }

            for (const auto &gpu : *cudaGpus) {
                devices.emplace_back(DeviceInfo{ExecutionProvider::CUDA, gpu.index, gpu.description, gpu.id, gpu.memory});
            }
        }
#endif

        log.stdcDebug("Available execution devices for %1: %2", provider ? executionProviderToString(*provider) : "all providers", devices.size());
        return devices;
    }

    DeviceInfo::ExecutionProvider DeviceInfo::defaultExecutionProvider() noexcept {
#ifdef _WIN32
        return ExecutionProvider::DirectML;
#else
        return ExecutionProvider::CPU;
#endif
    }

    std::expected<DeviceInfo, DeviceError> DeviceInfo::defaultDevice(ExecutionProvider provider) {
        auto devices = listDevices(provider);
        if (!devices) {
            return std::unexpected{std::move(devices.error())};
        }

        return defaultDevice(*devices, provider);
    }

    std::expected<DeviceInfo, DeviceError> DeviceInfo::defaultDevice(std::span<const DeviceInfo> devices, ExecutionProvider provider) {
        const DeviceInfo *selected = nullptr;
        bool selectedPreferred = false;
        for (const auto &device : devices) {
            if (device.executionProvider() != provider || device.index() < 0) {
                continue;
            }
            if (provider == ExecutionProvider::CPU && device.index() != 0) {
                continue;
            }
            if ((provider == ExecutionProvider::DirectML || provider == ExecutionProvider::CUDA) && device.memory() < kMinGpuVramBytes) {
                continue;
            }

            const bool preferred = provider == ExecutionProvider::DirectML && isPreferredDirectMLDevice(device);
            if (!selected || (preferred && !selectedPreferred) ||
                (preferred == selectedPreferred && (device.memory() > selected->memory() ||
                                                   (device.memory() == selected->memory() && device.index() < selected->index())))) {
                selected = &device;
                selectedPreferred = preferred;
            }
        }

        if (!selected) {
            return std::unexpected{DeviceError("No available device found for execution provider " + executionProviderToString(provider))};
        }

        log.stdcDebug("Selected default execution provider %1, native device %2 (%3)", executionProviderToString(provider), selected->index(), selected->description());
        return *selected;
    }

    std::string DeviceInfo::executionProviderToString(ExecutionProvider ep) {
        switch (ep) {
            case ExecutionProvider::CPU: return "cpu";
            case ExecutionProvider::CUDA: return "cuda";
            case ExecutionProvider::DirectML: return "directml";
            case ExecutionProvider::CoreML: return "coreml";
        }

        std::unreachable();
    }

    std::expected<DeviceInfo::ExecutionProvider, DeviceError> DeviceInfo::executionProviderFromString(const std::string &s) {
        if (s == "cuda") {
            return ExecutionProvider::CUDA;
        }
        if (s == "directml") {
            return ExecutionProvider::DirectML;
        }
        if (s == "coreml") {
            return ExecutionProvider::CoreML;
        }
        if (s == "cpu") {
            return ExecutionProvider::CPU;
        }

        return std::unexpected{DeviceError("Unknown execution provider: " + s)};
    }

}
