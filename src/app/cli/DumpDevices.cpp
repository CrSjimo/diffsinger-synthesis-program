#include "DumpDevices.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <stdcorelib/support/json.h>
#include <stdcorelib/support/logging.h>

#include <dsspruntime/DeviceInfo.h>

namespace dssp {

    namespace {

        stdc::LogCategory log("dssp.cli.DumpDevices");

        std::string toLower(std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        stdc::json::Object deviceToObject(const runtime::DeviceInfo &device, const bool includeExecutionProvider) {
            stdc::json::Object obj;
            if (includeExecutionProvider) {
                obj.emplace("execution_provider", stdc::json::Value(runtime::DeviceInfo::executionProviderToString(device.executionProvider())));
            }
            obj.emplace("index", stdc::json::Value(static_cast<int64_t>(device.index())));
            obj.emplace("description", stdc::json::Value(device.description()));
            obj.emplace("id", stdc::json::Value(device.id()));
            obj.emplace("memory", stdc::json::Value(static_cast<uint64_t>(device.memory())));
            return obj;
        }

        stdc::json::Object groupDevices(const std::vector<runtime::DeviceInfo> &devices) {
            stdc::json::Object grouped;
            for (const auto &device : devices) {
                const std::string epId = toLower(runtime::DeviceInfo::executionProviderToString(device.executionProvider()));

                stdc::json::Array *array = nullptr;
                const auto it = grouped.find(epId);
                if (it != grouped.end()) {
                    array = it->second.asArray();
                } else {
                    const auto [it2, inserted] = grouped.emplace(epId, stdc::json::Array{});
                    array = it2->second.asArray();
                }
                if (array) {
                    array->push_back(stdc::json::Value(deviceToObject(device, false)));
                }
            }
            return grouped;
        }

        int runDumpDevices(const stdc::cli::ParseResult &) {
            auto devices = runtime::DeviceInfo::listDevices();
            if (!devices) {
                log.stdcCritical("Failed to list devices: %1", devices.error().formatChain());
                return 1;
            }
            try {
                const runtime::DeviceInfo defaultDevice = runtime::DeviceInfo::defaultDevice();

                stdc::json::Object root;
                root.emplace("devices", stdc::json::Value(groupDevices(*devices)));
                root.emplace("default_device", stdc::json::Value(deviceToObject(defaultDevice, true)));

                std::cout << stdc::json::Value(std::move(root)).toJson(-1) << std::endl;
                return 0;
            } catch (const std::exception &e) {
                log.stdcCritical("Device JSON serialization failed: %1", e.what());
                return 1;
            }
        }

    }

    stdc::cli::Command dumpDevicesCommand = stdc::cli::Command{"dump-devices", "Dump information of all devices to JSON"}.addHelpOption().setHandler(runDumpDevices);

}
