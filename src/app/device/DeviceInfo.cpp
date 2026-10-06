#include "DeviceInfo.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#   include <windows.h>
#   include <dxgi1_6.h>
#   include <wrl/client.h>
#elif defined(__APPLE__)
#   include <sys/types.h>
#   include <sys/sysctl.h>
#elif defined(__linux__)
#   include <unistd.h>
#endif

namespace dssp {

    namespace {

        constexpr std::size_t kMinGpuVramBytes = 1ULL * 1024 * 1024 * 1024;

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
            return (pages > 0 && pageSize > 0)
                ? static_cast<uint64_t>(pages) * static_cast<uint64_t>(pageSize)
                : 0;
#else
            return 0;
#endif
        }

#ifdef _WIN32

        Microsoft::WRL::ComPtr<IDXGIFactory6> g_dxgiFactory;

        void initDxgi() {
            if (!g_dxgiFactory) {
                if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&g_dxgiFactory)))) {
                    throw std::runtime_error("Failed to create DXGI factory");
                }
            }
        }

        std::string wcharToString(const wchar_t *wstr) {
            if (!wstr) {
                return {};
            }
            const int length = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
            if (length <= 0) {
                return {};
            }
            std::string result(static_cast<std::size_t>(length) - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], length, nullptr, nullptr);
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

        std::vector<GpuData> listDirectMLGpus() {
            initDxgi();

            std::vector<GpuData> gpus;
            Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
            for (int adapterIndex = 0;; ++adapterIndex) {
                const HRESULT enumResult = g_dxgiFactory->EnumAdapters1(adapterIndex, &adapter);
                if (enumResult == DXGI_ERROR_NOT_FOUND) {
                    break;
                }
                if (FAILED(enumResult)) {
                    throw std::runtime_error("Failed to enumerate DXGI adapters");
                }

                DXGI_ADAPTER_DESC1 desc;
                if (FAILED(adapter->GetDesc1(&desc))) {
                    throw std::runtime_error("Failed to query DXGI adapter description");
                }

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                    continue;
                }

                if (desc.DedicatedVideoMemory < kMinGpuVramBytes) {
                    continue;
                }

                gpus.push_back(createGpuDataFromDesc(desc, adapterIndex));
            }

            std::sort(gpus.begin(), gpus.end(),
                      [](const GpuData &a, const GpuData &b) { return a.memory > b.memory; });
            return gpus;
        }

#endif // _WIN32

#ifdef DSSP_ENABLE_CUDA

        std::string getCommandOutput() {
            constexpr const char *kCommand =
                "nvidia-smi --query-gpu=index,name,memory.total,uuid,pci.device_id "
                "--format=csv,noheader,nounits";

            std::string result;
#ifdef _WIN32
            FILE *pipe = _popen(kCommand, "r");
#else
            FILE *pipe = popen(kCommand, "r");
#endif
            if (!pipe) {
                throw std::runtime_error("Failed to launch nvidia-smi");
            }

            char buffer[256];
            while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }

#ifdef _WIN32
            _pclose(pipe);
#else
            pclose(pipe);
#endif
            return result;
        }

        GpuData parseGpuFromLine(const std::string &line) {
            GpuData data;
            bool insideQuotes = false;
            std::string currentField;
            int fieldIndex = 0;

            const auto processField = [&data](const int fieldIndex_, const std::string &field) {
                switch (fieldIndex_) {
                    case 0:
                        data.index = std::stoi(field);
                        break;
                    case 1:
                        data.description = field;
                        break;
                    case 2:
                        data.memory = static_cast<std::size_t>(std::stoull(field)) * 1024 * 1024;
                        break;
                    case 3:
                        data.id = field;
                        break;
                    default:
                        break;
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

            return data;
        }

        std::vector<GpuData> listCudaGpus() {
            std::vector<GpuData> gpus;

            const std::string output = getCommandOutput();
            if (output.empty()) {
                return gpus;
            }

            std::istringstream stream(output);
            std::string line;
            while (std::getline(stream, line)) {
                if (line.empty()) {
                    continue;
                }
                GpuData data = parseGpuFromLine(line);
                if (data.memory < kMinGpuVramBytes) {
                    continue;
                }
                gpus.push_back(std::move(data));
            }
            return gpus;
        }

#endif // DSSP_ENABLE_CUDA

    }

    std::vector<DeviceInfo> DeviceInfo::listDevices() {
        std::vector<DeviceInfo> devices;
        devices.emplace_back(DeviceInfo{ExecutionProvider::CPU, 0, "CPU", "cpu", getMemorySize()});

#ifdef _WIN32
        for (const auto &gpu : listDirectMLGpus()) {
            devices.emplace_back(DeviceInfo{ExecutionProvider::DirectML, gpu.index, gpu.description, gpu.id, gpu.memory});
        }
#endif

#ifdef DSSP_ENABLE_CUDA
        for (const auto &gpu : listCudaGpus()) {
            devices.emplace_back(ExecutionProvider::CUDA, gpu.index, gpu.description, gpu.id, gpu.memory);
        }
#endif

        return devices;
    }

    DeviceInfo DeviceInfo::defaultDevice() {
        return DeviceInfo(ExecutionProvider::CPU, 0, "CPU", "cpu", 0);
    }

    std::string DeviceInfo::executionProviderToString(ExecutionProvider ep) {
        switch (ep) {
            case ExecutionProvider::CPU:
                return "cpu";
            case ExecutionProvider::CUDA:
                return "cuda";
            case ExecutionProvider::DirectML:
                return "directml";
            case ExecutionProvider::CoreML:
                return "coreml";
        }
        throw std::invalid_argument("Unknown execution provider");
    }

    DeviceInfo::ExecutionProvider DeviceInfo::executionProviderFromString(const std::string &s) {
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
        throw std::invalid_argument("Unknown execution provider: " + s);
    }

}
