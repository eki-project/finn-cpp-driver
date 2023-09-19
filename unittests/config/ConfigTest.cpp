#include "../../src/utils/Types.h"
#include "gtest/gtest.h"

TEST(ConfigTest, JsonConversion) {
    EXPECT_NO_THROW(auto j = json::parse(R"({"foldedShape ":[]," kernelName ":" test "," normalShape ":[1]," packedShape ":[1,2,2]})"););
    EXPECT_NO_THROW(auto j = json::parse(R"({"idmas":[{"foldedShape ":[]," kernelName ":" test "," normalShape ":[1]," packedShape ":[1,2,2]}], "odmas":[], "xclbinPath":"test", "name":"test"})"););
}


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}