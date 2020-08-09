// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <gtest/gtest.h>

#include <cpp/ie_cnn_network.h>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <memory>
#include <map>

#include <ngraph/function.hpp>
#include <ngraph/op/interpolate.hpp>
#include <ngraph/op/constant.hpp>
#include <ngraph/op/parameter.hpp>
#include <ngraph/op/op.hpp>
#include <ngraph/op/relu.hpp>
#include <ngraph_ops/type_relaxed.hpp>
#include <ngraph/op/result.hpp>
#include <ngraph/opsets/opset1.hpp>

#include <ie_util_internal.hpp>
#include <ie_core.hpp>

#include "common_test_utils/test_common.hpp"
#include "common_test_utils/data_utils.hpp"
#include "common_test_utils/file_utils.hpp"
#include "common_test_utils/common_utils.hpp"
#include "generic_ie.hpp"

IE_SUPPRESS_DEPRECATED_START

using namespace testing;
using namespace InferenceEngine;
using namespace CommonTestUtils;
namespace element = ngraph::element;
using std::make_shared;
using TypeVector = std::vector<element::Type>;

using TypeRelaxedTests = TestsCommon;

TEST_F(TypeRelaxedTests, noOverrideCopyCtor) {
    std::shared_ptr<ngraph::Function> ngraph;
    {
        ngraph::PartialShape shape({1, 3, 22, 22});
        ngraph::element::Type type(ngraph::element::Type_t::f32);
        auto param = std::make_shared<ngraph::opset1::Parameter>(type, shape);
        auto op = ngraph::opset1::Relu(param);
        auto relaxed_op = std::make_shared<ngraph::op::TypeRelaxed<ngraph::opset1::Relu>>(op);
        auto result = std::make_shared<ngraph::opset1::Result>(relaxed_op);

        ngraph::ParameterVector params = {param};
        ngraph::ResultVector results = {result};

        ngraph = std::make_shared<ngraph::Function>(results, params);

        ASSERT_EQ(ngraph::element::f32, relaxed_op->get_input_element_type(0));
        ASSERT_EQ(ngraph::element::f32, relaxed_op->get_output_element_type(0));
    }

    ASSERT_EQ(3, ngraph->get_ops().size());
}

TEST_F(TypeRelaxedTests, overrideOutputCopyCtor) {
    auto input_type = element::f32;
    auto overriden_type = element::i32;
    std::shared_ptr<ngraph::Function> ngraph;
    {
        ngraph::PartialShape shape({1, 3, 22, 22});
        auto param = std::make_shared<ngraph::opset1::Parameter>(input_type, shape);
        auto op = ngraph::opset1::Relu(param);
        auto relaxed_op = std::make_shared<ngraph::op::TypeRelaxed<ngraph::opset1::Relu>>(
                op, TypeVector{}, TypeVector{overriden_type});
        auto result = std::make_shared<ngraph::opset1::Result>(relaxed_op);

        ngraph = std::make_shared<ngraph::Function>(ngraph::ResultVector{result}, ngraph::ParameterVector{param});

        ASSERT_EQ(input_type, relaxed_op->get_input_element_type(0));
        ASSERT_EQ(overriden_type, relaxed_op->get_output_element_type(0));
    }

    ASSERT_EQ(3, ngraph->get_ops().size());
}

TEST_F(TypeRelaxedTests, overrideInputCopyCtor) {
    auto input_type = element::f32;
    auto overriden_type = element::i32;
    std::shared_ptr<ngraph::Function> ngraph;
    {
        ngraph::PartialShape shape({1, 3, 22, 22});
        auto param = std::make_shared<ngraph::opset1::Parameter>(input_type, shape);
        auto op = ngraph::opset1::Relu(param);
        auto relaxed_op = std::make_shared<ngraph::op::TypeRelaxed<ngraph::opset1::Relu>>(
                op, TypeVector{overriden_type}, TypeVector{});
        auto result = std::make_shared<ngraph::opset1::Result>(relaxed_op);

        ngraph = std::make_shared<ngraph::Function>(ngraph::ResultVector{result}, ngraph::ParameterVector{param});

        ASSERT_EQ(input_type, relaxed_op->get_input_element_type(0));
        ASSERT_EQ(overriden_type, relaxed_op->get_output_element_type(0));
    }

    ASSERT_EQ(3, ngraph->get_ops().size());
}

TEST_F(TypeRelaxedTests, mixedInputsAutoOutput) {
    auto input_type1 = element::u8;
    auto input_type2 = element::i8;
    auto overriden_type = element::i16;
    std::shared_ptr<ngraph::Function> ngraph;
    {
        ngraph::PartialShape shape({1, 3, 22, 22});
        auto param1 = std::make_shared<ngraph::opset1::Parameter>(input_type1, shape);
        auto param2 = std::make_shared<ngraph::opset1::Parameter>(input_type2, shape);
        auto op = ngraph::opset1::Add(
                ngraph::op::TemporaryReplaceOutputType(param1, overriden_type).get(),
                ngraph::op::TemporaryReplaceOutputType(param2, overriden_type).get());
        auto relaxed_op = std::make_shared<ngraph::op::TypeRelaxed<ngraph::opset1::Add>>(
                op, TypeVector{overriden_type, overriden_type}, TypeVector{});
        auto result = std::make_shared<ngraph::opset1::Result>(relaxed_op);

        ngraph = std::make_shared<ngraph::Function>(ngraph::ResultVector{result}, ngraph::ParameterVector{param1, param2});

        ASSERT_EQ(input_type1, relaxed_op->get_input_element_type(0));
        ASSERT_EQ(input_type2, relaxed_op->get_input_element_type(1));
        ASSERT_EQ(overriden_type, relaxed_op->get_output_element_type(0));
    }

    ASSERT_EQ(4, ngraph->get_ops().size());
}

TEST_F(TypeRelaxedTests, mixedInputsAutoOutputForwardCtor) {
    auto input_type1 = element::u8;
    auto input_type2 = element::i8;
    auto overriden_type = element::i16;
    std::shared_ptr<ngraph::Function> ngraph;
    {
        ngraph::PartialShape shape({1, 3, 22, 22});
        auto param1 = std::make_shared<ngraph::opset1::Parameter>(input_type1, shape);
        auto param2 = std::make_shared<ngraph::opset1::Parameter>(input_type2, shape);
        auto relaxed_op = std::make_shared<ngraph::op::TypeRelaxed<ngraph::opset1::Add>>(
                TypeVector{overriden_type, overriden_type}, TypeVector{},
                ngraph::op::TemporaryReplaceOutputType(param1, overriden_type).get(),
                ngraph::op::TemporaryReplaceOutputType(param2, overriden_type).get());
        auto result = std::make_shared<ngraph::opset1::Result>(relaxed_op);

        ngraph = std::make_shared<ngraph::Function>(ngraph::ResultVector{result}, ngraph::ParameterVector{param1, param2});

        ASSERT_EQ(input_type1, relaxed_op->get_input_element_type(0));
        ASSERT_EQ(input_type2, relaxed_op->get_input_element_type(1));
        ASSERT_EQ(overriden_type, relaxed_op->get_output_element_type(0));
    }

    ASSERT_EQ(4, ngraph->get_ops().size());
}

TEST_F(TypeRelaxedTests, notSupportedTypeOverride) {
    auto overriden_type = element::u8;
    auto orig_type = element::boolean;
    std::shared_ptr<ngraph::Function> ngraph;
    {
        ngraph::PartialShape shape({1, 3, 22, 22});
        auto param1 = std::make_shared<ngraph::opset1::Parameter>(overriden_type, shape);
        auto param2 = std::make_shared<ngraph::opset1::Parameter>(overriden_type, shape);
        auto op = ngraph::opset1::LogicalAnd(
                ngraph::op::TemporaryReplaceOutputType(param1, orig_type).get(),
                ngraph::op::TemporaryReplaceOutputType(param2, orig_type).get());
        auto relaxed_op = std::make_shared<ngraph::op::TypeRelaxed<ngraph::opset1::LogicalAnd>>(
                op, TypeVector{orig_type, orig_type}, TypeVector{overriden_type});
        auto result = std::make_shared<ngraph::opset1::Result>(relaxed_op);

        ngraph = std::make_shared<ngraph::Function>(ngraph::ResultVector{result}, ngraph::ParameterVector{param1, param2});

        ASSERT_EQ(overriden_type, relaxed_op->get_input_element_type(0));
        ASSERT_EQ(overriden_type, relaxed_op->get_input_element_type(1));
        ASSERT_EQ(overriden_type, relaxed_op->get_output_element_type(0));
    }

    ASSERT_EQ(4, ngraph->get_ops().size());
}

TEST_F(TypeRelaxedTests, notSupportedTypeOverridePartially) {
    auto some_type = element::u8;
    auto overriden_type = element::f32;
    auto orig_type = element::i64;
    std::shared_ptr<ngraph::Function> ngraph;
    {
        ngraph::PartialShape shape({1, 3, 22, 22});
        auto param1 = std::make_shared<ngraph::opset1::Parameter>(some_type, shape);
        auto param2 = std::make_shared<ngraph::opset1::Parameter>(overriden_type, ngraph::PartialShape{1});
        auto op = ngraph::opset1::Reshape(
                param1,
                ngraph::op::TemporaryReplaceOutputType(param2, orig_type).get(),
                false);
        auto relaxed_op = std::make_shared<ngraph::op::TypeRelaxed<ngraph::opset1::Reshape>>(
                op, TypeVector{element::undefined, orig_type}, TypeVector{});
        auto result = std::make_shared<ngraph::opset1::Result>(relaxed_op);

        ngraph = std::make_shared<ngraph::Function>(ngraph::ResultVector{result}, ngraph::ParameterVector{param1, param2});

        ASSERT_EQ(some_type, relaxed_op->get_input_element_type(0));
        ASSERT_EQ(overriden_type, relaxed_op->get_input_element_type(1));
        ASSERT_EQ(some_type, relaxed_op->get_output_element_type(0));
    }

    ASSERT_EQ(4, ngraph->get_ops().size());
}