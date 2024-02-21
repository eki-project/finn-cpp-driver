/**
 * @file ConfigTest.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Unittest for the FINN Runtime config
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */
#include <FINNCppDriver/utils/Types.h>

#include "gtest/gtest.h"

/*
TEST(ConfigTest, JsonConversion) {
    EXPECT_NO_THROW(auto j = json::parse(R"({"foldedShape ":[]," kernelName ":" test "," normalShape ":[1]," packedShape ":[1,2,2]})"););
    EXPECT_NO_THROW(auto j = json::parse(R"({"idmas":[{"foldedShape ":[]," kernelName ":" test "," normalShape ":[1]," packedShape ":[1,2,2]}], "odmas":[], "xclbinPath":"test", "name":"test"})"););
}
*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}