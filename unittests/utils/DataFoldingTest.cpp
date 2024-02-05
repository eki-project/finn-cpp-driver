/**
 * @file FinnUtilsTest.cpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Unittest for the collection of Finn utility functions
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <FINNCppDriver/utils/Types.h>

#include <FINNCppDriver/utils/DynamicMdSpan.hpp>
#include <algorithm>

#include "gtest/gtest.h"


TEST(DynamicMdSpanTest, ConstructionTest) {
    std::vector<int> a{10, 0, -3, -7, 9, 2, 3, -15, 4, -4};
    Finn::DynamicMdSpan inputNormal(a.begin(), a.end(), {1, 10});

    auto vec = inputNormal.getMostInnerDims();
    EXPECT_EQ(vec.size(), 1);
    EXPECT_TRUE(std::equal(vec[0].begin(), vec[0].end(), a.begin(), a.end()));
    inputNormal.setShape({1, 5, 2});
    auto vec2 = inputNormal.getMostInnerDims();
    EXPECT_EQ(vec2.size(), 5);
    std::vector<std::vector<int>> b{{10, 0}, {-3, -7}, {9, 2}, {3, -15}, {4, -4}};
    for (std::size_t i = 0; i < b.size(); ++i) {
        EXPECT_TRUE(std::equal(vec2[i].begin(), vec2[i].end(), b[i].begin(), b[i].end()));
    }
}


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}