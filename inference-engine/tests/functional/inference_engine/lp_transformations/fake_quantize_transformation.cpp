// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "layer_transformation.hpp"

#include <map>
#include <memory>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include <ngraph/pass/visualize_tree.hpp>
#include "transformations/low_precision/fake_quantize.hpp"
#include "transformations/utils/utils.hpp"
#include "../transformations/ngraph_test_utils.hpp"
#include "ngraph_functions/low_precision_transformations/fake_quantize_function.hpp"

#include "simple_low_precision_transformer.hpp"

using namespace testing;
using namespace ngraph;
using namespace ngraph::pass;

class ExpectedValues {
public:
    std::vector<float> subtract;
    std::vector<float> multiply;
};

class FakeQuantizeOnDataTestValues {
public:
    low_precision::LayerTransformation::Params params;
    builder::subgraph::FakeQuantizeOnData actual;
    builder::subgraph::FakeQuantizeOnData expected;
    std::map<ngraph::element::Type, ExpectedValues> expectedValues;
};

inline std::ostream& operator<<(std::ostream& os, const std::vector<float>& values) {
    os << "{ ";
    for (size_t i = 0; i < values.size(); ++i) {
        os << values[i];
        if (i != (values.size() - 1ul)) {
            os << ", ";
        }
    }
    os << " }";
    return os;
}

inline std::ostream& operator<<(std::ostream& out, const FakeQuantizeOnDataTestValues& testValue) {
    return out << "_" <<
        testValue.actual.constantShape << "_" << testValue.actual.outputLowValues << "_" << testValue.actual.outputHighValues << "_" <<
        testValue.expected.constantShape << "_" << testValue.expected.outputLowValues << "_" << testValue.expected.outputHighValues;;
}

typedef std::tuple<
    ngraph::element::Type,
    ngraph::Shape,
    bool,
    FakeQuantizeOnDataTestValues> FakeQuantizeTransformationParams;

class FakeQuantizeTransformation : public LayerTransformation, public testing::WithParamInterface<FakeQuantizeTransformationParams> {
public:
    void SetUp() override {
        const ngraph::element::Type precision = std::get<0>(GetParam());
        const ngraph::Shape shape = std::get<1>(GetParam());
        const bool updatePrecision = std::get<2>(GetParam());
        const FakeQuantizeOnDataTestValues fakeQuantizeOnData = std::get<3>(GetParam());

        const low_precision::LayerTransformation::Params params = low_precision::LayerTransformation::Params(fakeQuantizeOnData.params).
            setUpdatePrecisions(updatePrecision);

        actualFunction = ngraph::builder::subgraph::FakeQuantizeFunction::getOriginal(
            precision,
            shape,
            params,
            fakeQuantizeOnData.actual);

        SimpleLowPrecisionTransformer transform;
        transform.add<ngraph::pass::low_precision::FakeQuantizeTransformation, ngraph::opset1::FakeQuantize>(params);
        transform.transform(actualFunction);

        referenceFunction = ngraph::builder::subgraph::FakeQuantizeFunction::getReference(
            precision,
            shape,
            params,
            fakeQuantizeOnData.expected,
            fakeQuantizeOnData.expectedValues.find(precision)->second.subtract,
            fakeQuantizeOnData.expectedValues.find(precision)->second.multiply);
    }

    static std::string getTestCaseName(testing::TestParamInfo<FakeQuantizeTransformationParams> obj) {
        ngraph::element::Type precision;
        ngraph::Shape shape;
        bool updatePrecision;
        FakeQuantizeOnDataTestValues fakeQuantizeOnData;
        std::tie(precision, shape, updatePrecision, fakeQuantizeOnData) = obj.param;

        std::ostringstream result;
        result << LayerTransformation::getTestCaseNameByParams(precision, shape, fakeQuantizeOnData.params) <<
            (updatePrecision ? "" : "_notUpdatePrecision_") <<
            fakeQuantizeOnData;
        return result.str();
    }
};

TEST_P(FakeQuantizeTransformation, CompareFunctions) {
    actualFunction->validate_nodes_and_infer_types();
    auto res = compare_functions(referenceFunction, actualFunction, true);
    ASSERT_TRUE(res.first) << res.second;
}

const std::vector<ngraph::element::Type> precisions = {
    ngraph::element::f32,
    // ngraph::element::f16
};

const std::vector<bool> updatePrecisions = { true, false };

const std::vector<FakeQuantizeOnDataTestValues> fakeQuantizeOnDataTestValues = {
    // U8
    {
        LayerTransformation::createParamsU8I8(),
        { 256ul, {}, { 0.f }, { 2.55f }, { 0.f }, { 2.55f } },
        { 256ul, {}, { 0.f }, { 2.55f }, { 0.f }, { 255.f } },
        {
            { ngraph::element::f32, { {}, { 0.01f }} },
            { ngraph::element::f16, { {}, { 0.01f }} }
        }
    },
    {
        LayerTransformation::createParamsU8I8(),
        { 256ul, {}, { -1.23f }, { 2.55f }, { -1.23f }, { 2.55f } },
        { 256ul, {}, { -1.23f }, { 2.55f }, { 0.f }, { 255.f } },
        {
            { ngraph::element::f32, {{ 82.97619048f }, { 0.014823529f }} },
            { ngraph::element::f16, {{ 83.f }, { 0.014823529f }} }
        }
    },
    {
        LayerTransformation::createParamsU8I8(),
        { 256ul, {}, { -1.28f} , { 1.27f }, { -1.28f} , { 1.27f } },
        { 256ul, {}, { -1.28f} , { 1.27f }, { 0.f }, { 255.f } },
        {
            { ngraph::element::f32, {{ 128.f }, { 0.01f }} },
            { ngraph::element::f16, {{ 128.f }, { 0.01f }} }
        }
    },

    // I8
    {
        LayerTransformation::createParamsI8I8(),
        { 256ul, {}, { -1.28f}, { 1.27f }, { -1.28f}, { 1.27f } },
        { 256ul, {}, { -1.28f}, { 1.27f }, { -128.f}, { 127.f } },
        {
            { ngraph::element::f32, {{ }, { 0.01f }} },
            { ngraph::element::f16, {{ }, { 0.01f }} }
        }
    },
    {
        LayerTransformation::createParamsI8I8(),
        { 256ul, {}, { -0.12f}, { 1.27f }, { -0.12f}, { 1.27f } },
        { 256ul, {}, { -0.12f}, { 1.27f }, { -128.f}, { 127.f } },
        {
            { ngraph::element::f32, {{ -105.9856115f }, { 0.00545098f }} },
            { ngraph::element::f16, {{ -105.9856115f }, { 0.00545098f }} }
        }
    },
    {
        LayerTransformation::createParamsI8I8(),
        { 256ul, {}, { 0.f }, { 2.55f }, { 0.f }, { 2.55f } },
        { 256ul, {}, { 0.f }, { 2.55f }, { -128.f }, { 127.f } },
        {
            { ngraph::element::f32, {{ -128.f }, { 0.01f }} },
            { ngraph::element::f16, {{ -128.f }, { 0.01f }} }
        }
    }
};

const std::vector<ngraph::Shape> shapes = {
    { 1, 32, 72, 48 },
    // TODO: 3D tensor
};

INSTANTIATE_TEST_CASE_P(
    LPT,
    FakeQuantizeTransformation,
    ::testing::Combine(
        ::testing::ValuesIn(precisions),
        ::testing::ValuesIn(shapes),
        ::testing::ValuesIn(updatePrecisions),
        ::testing::ValuesIn(fakeQuantizeOnDataTestValues)),
    FakeQuantizeTransformation::getTestCaseName);