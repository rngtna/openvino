// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <ie_blob.h>
#include <gtest/gtest.h>
#include "transformations/low_precision/reshape.hpp"

using LPT_ReshapeTransformation = ::testing::Test;

TEST(LPT_ReshapeTransformation, canBeTransformed_4D_to_2D_perTensor) {
    ASSERT_TRUE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({}),
        ngraph::Shape({}),
        ngraph::Shape({ 1, 3, 4, 5 }),
        ngraph::Shape({ 1, 60 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_4D_to_2D_perTensor2) {
    ASSERT_TRUE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1 }),
        ngraph::Shape({ 1 }),
        ngraph::Shape({ 1, 3, 4, 5 }),
        ngraph::Shape({ 1, 60 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_4D_to_2D_perChannels) {
    ASSERT_FALSE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1, 3 }),
        ngraph::Shape({ 1, 3 }),
        ngraph::Shape({ 1, 3, 4, 5 }),
        ngraph::Shape({ 1, 60 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_4D_to_2D_perChannels2) {
    ASSERT_FALSE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1, 3, 1, 1 }),
        ngraph::Shape({ 1, 3, 1, 1 }),
        ngraph::Shape({ 1, 3, 4, 5 }),
        ngraph::Shape({ 1, 60 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_4D_to_2D_perChannels3) {
    ASSERT_FALSE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1, 3, 1, 1 }),
        ngraph::Shape({ 1, 3, 1, 1 }),
        ngraph::Shape({ 1, 3, 4, 5 }),
        ngraph::Shape({ 12, 5 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_4D_to_2D_spatial) {
    ASSERT_FALSE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1, 3, 4, 1 }),
        ngraph::Shape({ 1, 3, 4, 1 }),
        ngraph::Shape({ 1, 3, 4, 5 }),
        ngraph::Shape({ 1, 60 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_4D_to_3D_perTensor) {
    ASSERT_TRUE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({}),
        ngraph::Shape({}),
        ngraph::Shape({1, 3, 4, 5}),
        ngraph::Shape({1, 3, 20})));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_4D_to_3D_perTensor2) {
    ASSERT_TRUE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1 }),
        ngraph::Shape({ 1 }),
        ngraph::Shape({ 1, 3, 4, 5 }),
        ngraph::Shape({ 1, 3, 20 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_4D_to_3D_perChannels) {
    ASSERT_TRUE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1, 3 }),
        ngraph::Shape({ 1, 3 }),
        ngraph::Shape({ 1, 3, 4, 5 }),
        ngraph::Shape({ 1, 3, 20 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_4D_to_3D_perChannels2) {
    ASSERT_TRUE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1, 3, 1, 1 }),
        ngraph::Shape({ 1, 3, 1, 1 }),
        ngraph::Shape({ 1, 3, 4, 5 }),
        ngraph::Shape({ 1, 3, 20 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_4D_to_3D_perSpacial1) {
    ASSERT_FALSE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1, 3, 4, 1 }),
        ngraph::Shape({ 1, 3, 4, 1 }),
        ngraph::Shape({ 1, 3, 4, 5 }),
        ngraph::Shape({ 1, 3, 20 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_4D_to_3D_perSpacial2) {
    ASSERT_FALSE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1, 3, 1, 4 }),
        ngraph::Shape({ 1, 3, 1, 4 }),
        ngraph::Shape({ 1, 3, 4, 5 }),
        ngraph::Shape({ 1, 3, 20 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_3D_to_4D_perTensor) {
    ASSERT_TRUE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({}),
        ngraph::Shape({}),
        ngraph::Shape({ 1, 3, 20 }),
        ngraph::Shape({ 1, 3, 4, 5 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_3D_to_4D_perTensor2) {
    ASSERT_TRUE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1 }),
        ngraph::Shape({ 1 }),
        ngraph::Shape({ 1, 3, 20 }),
        ngraph::Shape({ 1, 3, 4, 5 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_3D_to_4D_perChannels) {
    ASSERT_TRUE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1, 3 }),
        ngraph::Shape({ 1, 3 }),
        ngraph::Shape({ 1, 3, 20 }),
        ngraph::Shape({ 1, 3, 4, 5 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_3D_to_4D_perChannels2) {
    ASSERT_TRUE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1, 3, 1, 1 }),
        ngraph::Shape({ 1, 3, 1, 1 }),
        ngraph::Shape({ 1, 3, 20 }),
        ngraph::Shape({ 1, 3, 4, 5 })));
}

TEST(LPT_ReshapeTransformation, canBeTransformed_3D_to_4D_perSpacial) {
    ASSERT_FALSE(ngraph::pass::low_precision::ReshapeTransformation::canBeTransformed(
        ngraph::Shape({ 1, 3, 20 }),
        ngraph::Shape({ 1, 3, 20 }),
        ngraph::Shape({ 1, 3, 20 }),
        ngraph::Shape({ 1, 3, 4, 5 })));
}
