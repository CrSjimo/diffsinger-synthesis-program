#include <algorithm>

#include <gtest/gtest.h>

#include <dsspruntime/DeviceInfo.h>

namespace dssp::tests {
    namespace {
        using Device = runtime::DeviceInfo;
        using Provider = runtime::ExecutionProvider;
        constexpr std::size_t GiB = 1024ULL * 1024 * 1024;
    }

    TEST(DeviceInfo, DefaultProviderMatchesEditorPlatformPolicy) {
#ifdef _WIN32
        EXPECT_EQ(Device::defaultExecutionProvider(), Provider::DirectML);
#else
        EXPECT_EQ(Device::defaultExecutionProvider(), Provider::CPU);
#endif
    }

    TEST(DeviceInfo, DirectMLPrefersSupportedVendorsOverLargerOtherGpus) {
        for (const auto id : {"123410DE", "123410de", "12341002", "1234174B"}) {
            const std::vector<Device> devices{
                Device{Provider::CPU, 0, "CPU", "cpu", 64 * GiB},
                Device{Provider::DirectML, 0, "Intel", "12348086", 16 * GiB},
                Device{Provider::DirectML, 7, "Preferred GPU", id, 4 * GiB},
                Device{Provider::CUDA, 3, "CUDA GPU", "uuid", 24 * GiB},
            };
            const auto result = Device::defaultDevice(devices, Provider::DirectML);
            ASSERT_TRUE(result);
            EXPECT_EQ(result->index(), 7);
            EXPECT_EQ(result->id(), id);
            EXPECT_EQ(result->memory(), 4 * GiB);
        }
    }

    TEST(DeviceInfo, DirectMLChoosesLargestPreferredGpuRegardlessOfInventoryOrder) {
        std::vector<Device> devices{
            Device{Provider::DirectML, 9, "NVIDIA", "123410DE", 12 * GiB},
            Device{Provider::DirectML, 2, "AMD", "12341002", 8 * GiB},
            Device{Provider::DirectML, 0, "Intel", "12348086", 24 * GiB},
        };

        for (int i = 0; i < 2; ++i) {
            const auto result = Device::defaultDevice(devices, Provider::DirectML);
            ASSERT_TRUE(result);
            EXPECT_EQ(result->index(), 9);
            std::reverse(devices.begin(), devices.end());
        }
    }

    TEST(DeviceInfo, DirectMLFallsBackToLargestOtherGpuAndUsesDescriptionFallback) {
        std::vector<Device> devices{
            Device{Provider::DirectML, 5, "Other", "12349999", 8 * GiB},
            Device{Provider::DirectML, 0, "Intel", "12348086", 4 * GiB},
        };

        auto result = Device::defaultDevice(devices, Provider::DirectML);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->index(), 5);
        devices.emplace_back(Provider::DirectML, 7, "nViDiA", "unknown", GiB);
        result = Device::defaultDevice(devices, Provider::DirectML);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->index(), 7);
    }

    TEST(DeviceInfo, CudaChoosesLargestVramAndKeepsNativeIndex) {
        const std::vector<Device> devices{
            Device{Provider::CUDA, 7, "Largest", "uuid-7", 24 * GiB},
            Device{Provider::CUDA, 0, "Smallest", "uuid-0", 2 * GiB},
            Device{Provider::CUDA, 3, "Medium", "uuid-3", 8 * GiB},
            Device{Provider::DirectML, 1, "Other provider", "123410DE", 32 * GiB},
        };

        const auto result = Device::defaultDevice(devices, Provider::CUDA);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->index(), 7);
        EXPECT_EQ(result->id(), "uuid-7");
        EXPECT_EQ(result->memory(), 24 * GiB);
    }

    TEST(DeviceInfo, GpusBelowOneGiBAndInvalidIndicesAreExcluded) {
        for (const auto provider : {Provider::DirectML, Provider::CUDA}) {
            const std::vector<Device> devices{
                Device{provider, 0, "Too small", "123410DE", GiB - 1},
                Device{provider, -1, "Invalid", "123410DE", 24 * GiB},
                Device{provider, 6, "Eligible", "12348086", GiB},
            };
            const auto result = Device::defaultDevice(devices, provider);
            ASSERT_TRUE(result);
            EXPECT_EQ(result->index(), 6);
        }
    }

    TEST(DeviceInfo, EqualMemoryUsesLowestNativeIndex) {
        std::vector<Device> devices{
            Device{Provider::DirectML, 7, "NVIDIA", "123410DE", 4 * GiB},
            Device{Provider::DirectML, 3, "AMD", "12341002", 4 * GiB},
        };

        for (int i = 0; i < 2; ++i) {
            const auto result = Device::defaultDevice(devices, Provider::DirectML);
            ASSERT_TRUE(result);
            EXPECT_EQ(result->index(), 3);
            std::reverse(devices.begin(), devices.end());
        }
    }

    TEST(DeviceInfo, MissingProviderReturnsAnErrorWithoutCpuFallback) {
        const std::vector<Device> devices{Device{Provider::CPU, 0, "CPU", "cpu", 64 * GiB}};
        for (const auto provider : {Provider::CUDA, Provider::DirectML, Provider::CoreML}) {
            const auto result = Device::defaultDevice(devices, provider);
            ASSERT_FALSE(result);
            EXPECT_EQ(result.error().kind(), "dssp.device");
        }

        EXPECT_FALSE(Device::defaultDevice(std::span<const Device>{}, Provider::CPU));
        const auto cpu = Device::defaultDevice(devices, Provider::CPU);
        ASSERT_TRUE(cpu);
        EXPECT_EQ(cpu->index(), 0);
        EXPECT_EQ(cpu->memory(), 64 * GiB);
    }

    TEST(DeviceInfo, CpuInventoryDoesNotQueryGpuBackends) {
        const auto devices = Device::listDevices(Provider::CPU);
        ASSERT_TRUE(devices);
        ASSERT_EQ(devices->size(), 1);
        EXPECT_EQ(devices->front().executionProvider(), Provider::CPU);
        EXPECT_EQ(devices->front().index(), 0);
    }
}
