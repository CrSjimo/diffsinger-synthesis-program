#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <dssptests/TestArchitecture.h>

namespace dssp::tests {

    TEST(ArchitectureCatalog, PreservesOrderAndUsesExactIds) {
        const auto first = std::make_shared<TestArchitecture>("second");
        const auto second = std::make_shared<TestArchitecture>("first");
        const auto catalog = testCatalog({first, second});
        ASSERT_NE(catalog, nullptr);
        ASSERT_EQ(catalog->entries().size(), 2U);
        EXPECT_EQ(catalog->entries()[0], first);
        EXPECT_EQ(catalog->entries()[1], second);
        EXPECT_EQ(catalog->find("first"), second);
        EXPECT_EQ(catalog->find("First"), nullptr);
        EXPECT_EQ(catalog->find("unknown"), nullptr);
    }

    TEST(ArchitectureCatalog, RejectsNullEmptyAndDuplicateEntries) {
        const auto null = arch::ArchitectureCatalog::create({arch::ArchitecturePtr{}});
        EXPECT_FALSE(null);
        const auto empty = arch::ArchitectureCatalog::create({std::make_shared<TestArchitecture>("")});
        EXPECT_FALSE(empty);
        const auto duplicate = arch::ArchitectureCatalog::create({std::make_shared<TestArchitecture>("same"), std::make_shared<TestArchitecture>("same")});
        ASSERT_FALSE(duplicate);
        EXPECT_NE(duplicate.error().message().find("same"), std::string::npos);
        const auto noEntries = arch::ArchitectureCatalog::create({});
        ASSERT_TRUE(noEntries);
        EXPECT_TRUE(noEntries->entries().empty());
    }

    TEST(ArchitectureCatalog, ConcurrentReadersShareStableInstances) {
        const auto implementation = std::make_shared<TestArchitecture>();
        const auto catalog = testCatalog({implementation});
        ASSERT_NE(catalog, nullptr);
        std::vector<std::jthread> workers;
        for (unsigned i = 0; i < 8; ++i) {
            workers.emplace_back([&] {
                for (unsigned j = 0; j < 32; ++j) {
                    EXPECT_EQ(catalog->find("diffsinger"), implementation);
                    EXPECT_EQ(catalog->entries().front(), implementation);
                }
            });
        }
    }

    TEST(ArchitectureCatalog, SelectedInstanceCanOutliveItsCatalog) {
        std::weak_ptr<const arch::Architecture> observed;
        arch::ArchitecturePtr selected;
        {
            const auto implementation = std::make_shared<TestArchitecture>();
            observed = implementation;
            const auto catalog = testCatalog({implementation});
            ASSERT_NE(catalog, nullptr);
            selected = catalog->find("diffsinger");
        }
        EXPECT_FALSE(observed.expired());
        selected.reset();
        EXPECT_TRUE(observed.expired());
    }

}
