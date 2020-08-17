// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "ngraph_functions/low_precision_transformations/interpolate_function.hpp"

#include "ngraph_functions/subgraph_builders.hpp"
#include "ngraph_functions/low_precision_transformations/common/builders.hpp"

namespace ngraph {
namespace builder {
namespace subgraph {

std::shared_ptr<ngraph::Function> InterpolateFunction::getOriginal(
    const ngraph::Shape& inputShape,
    const ngraph::Shape& outputShape,
    const ngraph::op::InterpolateAttrs& interpAttrs,
    const ngraph::element::Type precisionBeforeDequantization,
    const ngraph::builder::subgraph::DequantizationOperations& dequantization) {
    const std::shared_ptr<op::v0::Parameter> input = std::make_shared<ngraph::opset1::Parameter>(
        precisionBeforeDequantization,
        ngraph::Shape(inputShape));

    const auto dequantizationOp = makeDequantization(input, dequantization);
    const auto outShape = std::make_shared<ngraph::opset1::Constant>(ngraph::element::i64, ngraph::Shape{ outputShape.size() }, outputShape);
    const auto interpolate = std::make_shared<ngraph::opset1::Interpolate>(dequantizationOp, outShape, interpAttrs);

    ngraph::ResultVector results{ std::make_shared<ngraph::opset1::Result>(interpolate) };
    return std::make_shared<ngraph::Function>(results, ngraph::ParameterVector{ input }, "InterpolateFunction");
}

std::shared_ptr<ngraph::Function> InterpolateFunction::getOriginal(
    const ngraph::element::Type precision,
    const ngraph::Shape& inputShape,
    const ngraph::Shape& outputShape,
    const ngraph::op::InterpolateAttrs& interpAttrs) {
    float k = 50.f;

    const auto input = std::make_shared<ngraph::opset1::Parameter>(precision, inputShape);
    const auto fakeQuantizeOnActivations = ngraph::builder::makeFakeQuantize(
        input, precision, 256ul, { 1ul },
        { 0.f }, { 255.f / k }, { 10.f }, { 255.f / k });
    const auto outShape = std::make_shared<ngraph::opset1::Constant>(ngraph::element::i64, ngraph::Shape{ outputShape.size() }, outputShape);
    const auto interpolate = std::make_shared<ngraph::opset1::Interpolate>(fakeQuantizeOnActivations, outShape, interpAttrs);

    ngraph::ResultVector results{ std::make_shared<ngraph::opset1::Result>(interpolate) };
    return std::make_shared<ngraph::Function>(results, ngraph::ParameterVector{ input }, "InterpolateFunction");
}

std::shared_ptr<ngraph::Function> InterpolateFunction::getReference(
    const ngraph::Shape& inputShape,
    const ngraph::Shape& outputShape,
    const ngraph::op::InterpolateAttrs& interpAttrs,
    const ngraph::element::Type precisionBeforeDequantization,
    const ngraph::builder::subgraph::DequantizationOperations& dequantizationBefore,
    const ngraph::element::Type precisionAfterOperation,
    const ngraph::builder::subgraph::DequantizationOperations& dequantizationAfter) {
    const std::shared_ptr<op::v0::Parameter> input = std::make_shared<ngraph::opset1::Parameter>(
        precisionBeforeDequantization,
        ngraph::Shape(inputShape));

    const std::shared_ptr<Node> quantizationOpBefore = makeDequantization(input, dequantizationBefore);
    const auto outShape = std::make_shared<ngraph::opset1::Constant>(ngraph::element::i64, ngraph::Shape{ outputShape.size() }, outputShape);
    const auto interpolate = std::make_shared<ngraph::opset1::Interpolate>(quantizationOpBefore, outShape, interpAttrs);
    const std::shared_ptr<Node> quantizationOpAfter = makeDequantization(interpolate, dequantizationAfter);

    ngraph::ResultVector results{ std::make_shared<ngraph::opset1::Result>(quantizationOpAfter) };
    return std::make_shared<ngraph::Function>(results, ngraph::ParameterVector{ input }, "InterpolateFunction");
}

}  // namespace subgraph
}  // namespace builder
}  // namespace ngraph