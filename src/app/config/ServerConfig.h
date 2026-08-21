#ifndef DSSP_SERVERCONFIG_H
#define DSSP_SERVERCONFIG_H

#include <cstdint>
#include <filesystem>
#include <string>

namespace dssp{

    class ServerConfig {
    public:
        static const std::string &host();
        static void setHost(const std::string &host);

        static std::uint16_t port();
        static void setPort(std::uint16_t port);

        static const std::filesystem::path &packageDir();
        static void setPackageDir(const std::filesystem::path &packageDir);

        static const std::filesystem::path &gameModelDir();
        static void setGAMEModelDir(const std::filesystem::path &gameModelDir);

        static const std::filesystem::path &rmvpeModelPath();
        static void setRMVPEModelPath(const std::filesystem::path &rmvpeModelPath);
    };

}

#endif //DSSP_SERVERCONFIG_H
