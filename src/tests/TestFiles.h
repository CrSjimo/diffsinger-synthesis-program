#ifndef DSSP_TESTS_TESTFILES_H
#define DSSP_TESTS_TESTFILES_H

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace dssp::tests {
    class TestFiles {
    public:
        TestFiles() {
            static std::atomic<unsigned> serial = 0;
            m_path = std::filesystem::path(::testing::TempDir()) / ("dssp-metadata-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(serial++));
            std::error_code error;
            EXPECT_TRUE(std::filesystem::create_directory(m_path, error));
            EXPECT_FALSE(error);
        }

        ~TestFiles() {
            std::error_code error;
            std::filesystem::remove_all(m_path, error);
            EXPECT_FALSE(error);
        }

        TestFiles(const TestFiles &) = delete;
        TestFiles &operator=(const TestFiles &) = delete;
        const std::filesystem::path &path() const { return m_path; }

        void write(const std::filesystem::path &relative, std::string_view bytes) const {
            const auto file = m_path / relative;
            std::error_code error;
            std::filesystem::create_directories(file.parent_path(), error);
            ASSERT_FALSE(error);
            std::ofstream stream(file, std::ios::binary);
            ASSERT_TRUE(stream);
            stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            ASSERT_TRUE(stream);
        }

    private:
        std::filesystem::path m_path;
    };
}

#endif // DSSP_TESTS_TESTFILES_H
