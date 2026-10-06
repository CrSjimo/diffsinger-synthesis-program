#include <gtest/gtest.h>
#include <stdcorelib/path.h>
#include <stdcorelib/support/systempaths.h>

#include <dsspruntime/ServerConfig.h>

namespace dssp::tests {

    TEST(ServerConfig, CliOverridesEnvironmentAndDefaults) {
        const auto root = std::filesystem::current_path();
        const runtime::ConfigValues options{{"--package-dir", root.string()}, {"--port", "20000"}, {"--silence-threshold", "0"}};
        const runtime::ConfigValues environment{{"DSSP_PORT", "invalid"}, {"DSSP_HOST", "0.0.0.0"}, {"DSSP_PACKAGE_DIR", ""}};
        auto result = runtime::resolveServerConfig(options, environment, root);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->port, 20000);
        EXPECT_EQ(result->host, "0.0.0.0");
        EXPECT_EQ(result->silenceThreshold, 0);
        EXPECT_EQ(result->deviceIndex, 0);
        EXPECT_TRUE(result->packageDir.is_absolute());
        EXPECT_TRUE(result->runtimePath.is_absolute());
    }

    TEST(ServerConfig, DefaultPackageRootUsesThePlatformUserDataLayout) {
        using SystemPaths = stdc::system::SystemPaths;
#ifdef _WIN32
        const auto base = SystemPaths::writableDirectory(SystemPaths::StateDirectory);
#else
        const auto base = SystemPaths::writableDirectory(SystemPaths::AppDataDirectory);
#endif
        const auto result = runtime::defaultPackageDir();
        if (!base) {
            ASSERT_FALSE(result);
            EXPECT_EQ(result.error().option(), "--package-dir");
            return;
        }
        ASSERT_TRUE(result);
        EXPECT_EQ(*result, (*base / "OpenVPI" / "DiffScope_packages").lexically_normal());
        EXPECT_TRUE(result->is_absolute());
    }

    TEST(ServerConfig, DeploymentPathsUseGnuLayoutAndFollowTheExecutableLocation) {
        const auto root = std::filesystem::current_path();
        for (const auto name : {"original-installation", "moved-installation"}) {
            const auto prefix = root / name;
            const auto result = runtime::resolveServerConfig({{"--package-dir", stdc::path::to_utf8(root)}}, {}, prefix / "bin");
            ASSERT_TRUE(result);
            EXPECT_EQ(result->pluginDir, prefix / "lib" / "plugins");
            EXPECT_EQ(result->resourceDir, prefix / "share");
            EXPECT_EQ(result->runtimePath.parent_path(), result->pluginDir / "dsinfer" / "inferencedrivers" / "onnx" / "runtime");
        }
    }

    TEST(ServerConfig, PluginOverridesDoNotRelocateApplicationData) {
        const auto root = std::filesystem::current_path();
        const auto applicationDir = root / "deployment" / "bin";
        const auto commandLinePlugins = root / "command-line-plugins";
        const auto environmentPlugins = root / "environment-plugins";
        const runtime::ConfigValues environment{{"DSSP_PLUGIN_DIR", stdc::path::to_utf8(environmentPlugins)}};
        const runtime::ConfigValues options{{"--package-dir", stdc::path::to_utf8(root)}, {"--plugin-dir", stdc::path::to_utf8(commandLinePlugins)}};
        const auto result = runtime::resolveServerConfig(options, environment, applicationDir);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->pluginDir, commandLinePlugins);
        EXPECT_EQ(result->resourceDir, root / "deployment" / "share");
        EXPECT_EQ(result->runtimePath.parent_path(), commandLinePlugins / "dsinfer" / "inferencedrivers" / "onnx" / "runtime");

        const auto fromEnvironment = runtime::resolveServerConfig({{"--package-dir", stdc::path::to_utf8(root)}}, environment, applicationDir);
        ASSERT_TRUE(fromEnvironment);
        EXPECT_EQ(fromEnvironment->pluginDir, environmentPlugins);
        EXPECT_EQ(fromEnvironment->resourceDir, result->resourceDir);

        const auto relativeApplication = runtime::resolveServerConfig(options, environment, "bin");
        ASSERT_FALSE(relativeApplication);
        EXPECT_EQ(relativeApplication.error().option(), "application-directory");
    }

    TEST(ServerConfig, OmittedPackageRootResolvesAndValidatesTheSystemDefault) {
        const auto root = std::filesystem::current_path();
        const auto packages = runtime::defaultPackageDir();
        const auto result = runtime::resolveServerConfig({}, {}, root);
        if (!packages) {
            ASSERT_FALSE(result);
            EXPECT_EQ(result.error().option(), "--package-dir");
            EXPECT_EQ(result.error().message(), packages.error().message());
            return;
        }
        // Read the user's directory without creating or modifying it.
        std::error_code error;
        if (!std::filesystem::is_directory(*packages, error)) {
            ASSERT_FALSE(result);
            EXPECT_EQ(result.error().option(), "--package-dir");
            EXPECT_EQ(result.error().message(), "Package root must be an existing directory");
            return;
        }
        ASSERT_TRUE(result);
        EXPECT_EQ(result->packageDir, *packages);
    }

    TEST(ServerConfig, EnvironmentPackageRootOverridesTheSystemDefault) {
        const auto root = std::filesystem::current_path();
        const auto result = runtime::resolveServerConfig({}, {{"DSSP_PACKAGE_DIR", stdc::path::to_utf8(root)}}, root);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->packageDir, root.lexically_normal());
    }

    TEST(ServerConfig, EmptyPackageRootCannotBecomeTheWorkingDirectory) {
        const auto root = std::filesystem::current_path();
        EXPECT_FALSE(runtime::resolveServerConfig({{"--package-dir", ""}}, {{"DSSP_PACKAGE_DIR", root.string()}}, root));
        const auto environment = runtime::resolveServerConfig({}, {{"DSSP_PACKAGE_DIR", ""}}, root);
        ASSERT_FALSE(environment);
        EXPECT_EQ(environment.error().option(), "--package-dir");
        EXPECT_EQ(environment.error().message(), "A nonempty path must be configured");
    }

    TEST(ServerConfig, MalformedNumbersReturnErrorsWithoutThrowing) {
        const auto root = std::filesystem::current_path();
        for (const auto port : {"0", "65536", "-1", "123suffix", "99999999999999999999"}) {
            const auto result = runtime::resolveServerConfig({{"--package-dir", root.string()}, {"--port", port}}, {}, root);
            ASSERT_FALSE(result);
            EXPECT_EQ(result.error().option(), "--port");
        }
        for (const auto threshold : {"nan", "inf", "-0.1", "0.02suffix"}) {
            EXPECT_FALSE(runtime::resolveServerConfig({{"--package-dir", root.string()}, {"--silence-threshold", threshold}}, {}, root));
        }
    }

    TEST(ServerConfig, BackendChoiceAndNativeDeviceIndexArePreserved) {
        const auto root = std::filesystem::current_path();
        auto result = runtime::resolveServerConfig({{"--package-dir", root.string()}, {"--execution-provider", "cuda"}, {"--device-index", "7"}}, {}, root);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->executionProvider, runtime::ExecutionProvider::CUDA);
        EXPECT_EQ(result->deviceIndex, 7);
    }

    TEST(ServerConfig, InvalidBackendAndCpuDeviceReturnTypedErrors) {
        const auto root = std::filesystem::current_path();
        const auto backend = runtime::resolveServerConfig({{"--package-dir", root.string()}, {"--execution-provider", "unknown"}}, {}, root);
        ASSERT_FALSE(backend);
        EXPECT_EQ(backend.error().option(), "--execution-provider");
        ASSERT_NE(backend.error().cause(), nullptr);
        EXPECT_EQ(backend.error().cause()->kind(), "dssp.device");
        EXPECT_FALSE(runtime::resolveServerConfig({{"--package-dir", root.string()}, {"--device-index", "1"}}, {}, root));
    }

}
