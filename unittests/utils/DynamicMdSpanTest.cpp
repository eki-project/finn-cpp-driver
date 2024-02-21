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

#include "gtest/gtest.h"


TEST(DynamicMdSpanTest, ConstructionTest) {
    std::vector<int> a;
    EXPECT_THROW(Finn::DynamicMdSpan(a.begin(), a.end(), {}), std::runtime_error);
    EXPECT_THROW(Finn::DynamicMdSpan(a.begin(), a.end(), {1}), std::runtime_error);

    a.emplace_back(1);
    EXPECT_THROW(Finn::DynamicMdSpan(a.begin(), a.end(), {}), std::runtime_error);

    EXPECT_THROW(Finn::DynamicMdSpan(a.begin(), a.end(), {4}), std::runtime_error);

    EXPECT_NO_THROW(Finn::DynamicMdSpan(a.begin(), a.end(), {1}));
}

TEST(DynamicMdSpanTest, StridesTest) {
    std::vector<int> a{0, 1, 2, 3, 4, 5};

    EXPECT_NO_THROW(Finn::DynamicMdSpan(a.begin(), a.end(), {1, 2, 3}));

    auto mdspanA = Finn::DynamicMdSpan(a.begin(), a.end(), {1, 2, 3});
    auto stridesA = mdspanA.getStrides();
    std::vector<std::size_t> expectedStridesA{6, 6, 3, 1};
    EXPECT_EQ(stridesA, expectedStridesA);

    auto mdspanB = Finn::DynamicMdSpan(a.begin(), a.end(), {6});
    auto stridesB = mdspanB.getStrides();
    std::vector<std::size_t> expectedStridesB{6, 1};
    EXPECT_EQ(stridesB, expectedStridesB);

    auto mdspanC = Finn::DynamicMdSpan(a.begin(), a.end(), {2, 3});
    auto stridesC = mdspanC.getStrides();
    std::vector<std::size_t> expectedStridesC{6, 3, 1};
    EXPECT_EQ(stridesC, expectedStridesC);

    auto mdspanD = Finn::DynamicMdSpan(a.begin(), a.end(), {1, 1, 2, 3});
    auto stridesD = mdspanD.getStrides();
    std::vector<std::size_t> expectedStridesD{6, 6, 6, 3, 1};
    EXPECT_EQ(stridesD, expectedStridesD);
}

TEST(DynamicMdSpanTest, SpanTest) {
    std::vector<int> a{0, 1, 2, 3, 4, 5};

    auto mdspanA = Finn::DynamicMdSpan(a.begin(), a.end(), {1, 2, 3});
    auto stridesA = mdspanA.getMostInnerDims();
    EXPECT_EQ(stridesA.size(), 2);
    std::span spa(a.begin(), 3);
    EXPECT_EQ(stridesA[0].begin(), spa.begin());
    EXPECT_EQ(stridesA[0].end(), spa.end());
    std::span spb(a.begin() + 3, 3);
    EXPECT_EQ(stridesA[1].begin(), spb.begin());
    EXPECT_EQ(stridesA[1].end(), spb.end());

    auto mdspanB = Finn::DynamicMdSpan(a.begin(), a.end(), {6});
    auto stridesB = mdspanB.getMostInnerDims();
    EXPECT_EQ(stridesB.size(), 1);
    std::span spc(a.begin(), 6);
    EXPECT_EQ(stridesB[0].begin(), spc.begin());
    EXPECT_EQ(stridesB[0].end(), spc.end());
}

TEST(DynamicMdSpanTest, ReshapeTest) {
    std::vector<int> a{0, 1, 2, 3, 4, 5};

    auto mdspanA = Finn::DynamicMdSpan(a.begin(), a.end(), {1, 2, 3});
    auto stridesA = mdspanA.getMostInnerDims();
    EXPECT_EQ(stridesA.size(), 2);
    std::span spa(a.begin(), 3);
    EXPECT_EQ(stridesA[0].begin(), spa.begin());
    EXPECT_EQ(stridesA[0].end(), spa.end());
    std::span spb(a.begin() + 3, 3);
    EXPECT_EQ(stridesA[1].begin(), spb.begin());
    EXPECT_EQ(stridesA[1].end(), spb.end());

    mdspanA.setShape({6});

    auto stridesB = mdspanA.getMostInnerDims();
    EXPECT_EQ(stridesB.size(), 1);
    std::span spc(a.begin(), 6);
    EXPECT_EQ(stridesB[0].begin(), spc.begin());
    EXPECT_EQ(stridesB[0].end(), spc.end());
}


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}