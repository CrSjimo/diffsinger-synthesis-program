#include <gtest/gtest.h>
#include <synthrt/Core/SynthUnit.h>

#include <dsspinference/ParallelismPolicy.h>
#include <dssptests/TestLanguagePackage.h>

namespace dssp::tests {
    TEST(ParallelismPolicy, OnlyExactWhitelistedContractsBypassSerialization) {
        EXPECT_TRUE(inference::isParallelLeaf("org.openvpi.wolf.inference.G2P", 1, "algo-pinyin"));
        for (const auto variant : {"direct", "dict", "mapping", "lua"}) EXPECT_TRUE(inference::isParallelLeaf("org.openvpi.wolf.inference.S2P", 1, variant));
        for (const auto variant : {"rule", "lua"}) EXPECT_TRUE(inference::isParallelLeaf("org.openvpi.wolf.inference.Onset", 1, variant));
        EXPECT_FALSE(inference::isParallelLeaf("org.openvpi.wolf.inference.G2P", 1, "multig2p-onnx"));
        EXPECT_FALSE(inference::isParallelLeaf("org.openvpi.wolf.inference.G2P", 1, "pipe-chain"));
        EXPECT_FALSE(inference::isParallelLeaf("org.openvpi.dsinfer.inference.Pitch", 1, "onnx"));
        EXPECT_FALSE(inference::isParallelLeaf("org.openvpi.wolf.inference.S2P", 2, "dict"));
        EXPECT_FALSE(inference::isParallelLeaf("org.openvpi.wolf.inference.S2P", 1, "future"));
        EXPECT_FALSE(inference::isParallelLeaf("com.example.future", 1, "dict"));
        EXPECT_FALSE(inference::isParallelLeaf("", 0, ""));
    }

    TEST(ParallelismPolicy, MetadataOnlyInspectionAllowsTableChainsAndRejectsUnresolvedDependencies) {
        TestFiles files;
        writeLanguagePackage(files, "one");
        srt::SynthUnit unit;
        auto package = unit.openPackage(files.path() / "one", srt::SynthUnit::DataOnly);
        ASSERT_TRUE(package);
        const auto handle = package.take();
        const auto *chain = handle.contribution("inference", "g2p");
        ASSERT_NE(chain, nullptr);
        EXPECT_TRUE(inference::permitsParallelExecution(*chain));
        EXPECT_TRUE(inference::permitsParallelExecution(*handle.contribution("inference", "s2p")));
        EXPECT_TRUE(inference::permitsParallelExecution(*handle.contribution("inference", "onset")));
        // A DataOnly import has no resolved binding. It must never be assumed
        // backend-free, regardless of the root's whitelisted composition type.
        files.write(files.path() / "one" / "g2p.json", R"({"interface":"org.openvpi.wolf.inference.G2P","variant":"pipe-chain","level":1,"exports":{},"configuration":{},"imports":[{"role":"backend","ref":":inference/s2p","options":{}}]})");
        srt::SynthUnit unresolvedUnit;
        auto unresolved = unresolvedUnit.openPackage(files.path() / "one", srt::SynthUnit::DataOnly);
        ASSERT_TRUE(unresolved);
        EXPECT_FALSE(inference::permitsParallelExecution(*unresolved.take().contribution("inference", "g2p")));
    }
}
