// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "ngraph_functions/low_precision_transformations/max_pool_function.hpp"

#include <ngraph/opsets/opset1.hpp>
#include "ngraph_functions/subgraph_builders.hpp"

namespace ngraph {
namespace builder {
namespace subgraph {

std::shared_ptr<ngraph::Function> MaxPoolFunction::getOriginal(
    const ngraph::element::Type ngPrecision,
    const ngraph::Shape& inputShape) {
    return nullptr;
}

std::shared_ptr<ngraph::Function> MaxPoolFunction::getReference(
    const ngraph::element::Type ngPrecision,
    const ngraph::Shape& inputShape) {
    return nullptr;
}

}  // namespace subgraph
}  // namespace builder
}  // namespace ngraph