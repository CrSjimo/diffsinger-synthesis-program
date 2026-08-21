#include "ServerConfig.h"

#ifdef _WIN32
#   include <windows.h>
#   include <shlobj.h>
#endif

namespace dssp{

    namespace{

        std::filesystem::path defaultPackageDir() {
            return {}; // TODO
        }

        std::string host_{"127.0.0.1"};
        std::uint16_t port_{13711};
        std::filesystem::path packageDir_ = defaultPackageDir();
        std::filesystem::path gameModelDir_;
        std::filesystem::path rmvpeModelPath_;
    }

    const std::string & ServerConfig::host() {
        return host_;
    }

    void ServerConfig::setHost(const std::string &host) {
        host_ = host;
    }

    std::uint16_t ServerConfig::port() {
        return port_;
    }

    void ServerConfig::setPort(std::uint16_t port) {
        port_ = port;
    }

    const std::filesystem::path & ServerConfig::packageDir() {
        return packageDir_;
    }

    void ServerConfig::setPackageDir(const std::filesystem::path &packageDir) {
        packageDir_ = packageDir;
    }

    const std::filesystem::path & ServerConfig::gameModelDir() {
        return gameModelDir_;
    }

    void ServerConfig::setGAMEModelDir(const std::filesystem::path &gameModelDir) {
        gameModelDir_ = gameModelDir;
    }

    const std::filesystem::path & ServerConfig::rmvpeModelPath() {
        return rmvpeModelPath_;
    }

    void ServerConfig::setRMVPEModelPath(const std::filesystem::path &rmvpeModelPath) {
        rmvpeModelPath_ = rmvpeModelPath;
    }
}
