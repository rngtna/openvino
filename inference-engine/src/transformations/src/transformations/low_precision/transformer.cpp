﻿// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "transformations/low_precision/transformer.hpp"
#include "transformations/low_precision/network_helper.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <iostream>
#include <string>
#include <typeinfo>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ngraph_ops/type_relaxed.hpp"

#include "transformations/low_precision/add.hpp"
#include "transformations/low_precision/avg_pool.hpp"
#include "transformations/low_precision/concat.hpp"
#include "transformations/low_precision/concat_multi_channels.hpp"
// #include "low_precision_transformations/const.hpp"
#include "transformations/low_precision/convert.hpp"
#include "transformations/low_precision/convolution.hpp"
#include "transformations/low_precision/depth_to_space.hpp"
#include "transformations/low_precision/fake_quantize.hpp"
// #include "low_precision_transformations/fully_connected.hpp"
#include "transformations/low_precision/fuse_fake_quantize.hpp"
#include "transformations/low_precision/group_convolution.hpp"
#include "transformations/low_precision/multiply.hpp"
#include "transformations/low_precision/mat_mul.hpp"
// #include "low_precision_transformations/mvn.hpp"
// #include "low_precision_transformations/permute.hpp"
#include "transformations/low_precision/max_pool.hpp"
#include "transformations/low_precision/normalize_l2.hpp"
// #include "low_precision_transformations/resample.hpp"
#include "transformations/low_precision/reshape.hpp"
#include "transformations/low_precision/relu.hpp"
// #include "low_precision_transformations/scaleshift_to_convolution.hpp"
// #include "low_precision_transformations/squeeze.hpp"
#include "transformations/low_precision/subtract.hpp"

// uncomment to display precision info during low precision transformations
// #define DISPLAY_PECISION

// TODO: debug only
#include <ngraph/pass/visualize_tree.hpp>

namespace ngraph {
namespace pass {
namespace low_precision {

LowPrecisionTransformations::LowPrecisionTransformations(
    const std::map<std::string, LayerTransformationPtr>& branchSpecificTransformations,
    const std::map<std::string, LayerTransformationPtr>& transformations,
    const std::map<std::string, LayerTransformationPtr>& cleanupTransformations) :
    branchSpecificTransformations(branchSpecificTransformations),
    transformations(transformations),
    cleanupTransformations(cleanupTransformations) {}

void LowPrecisionTransformations::setUpdatePrecisions(const bool updatePrecisions) {
    for (auto it = branchSpecificTransformations.begin(); it != branchSpecificTransformations.end(); ++it) {
        it->second->setUpdatePrecisions(updatePrecisions);
    }
    for (auto it = transformations.begin(); it != transformations.end(); ++it) {
        it->second->setUpdatePrecisions(updatePrecisions);
    }
}

void LowPrecisionTransformations::setQuantizeOutputs(const bool quantizeOutputs) {
    for (auto it = branchSpecificTransformations.begin(); it != branchSpecificTransformations.end(); ++it) {
        it->second->setQuantizeOutputs(quantizeOutputs);
    }
    for (auto it = transformations.begin(); it != transformations.end(); ++it) {
        it->second->setQuantizeOutputs(quantizeOutputs);
    }
}

void LowPrecisionTransformations::setWeightsToConst(const bool weightsToConst) {
    for (auto it = branchSpecificTransformations.begin(); it != branchSpecificTransformations.end(); ++it) {
        it->second->setWeightsToConst(weightsToConst);
    }
    for (auto it = transformations.begin(); it != transformations.end(); ++it) {
        it->second->setWeightsToConst(weightsToConst);
    }
}

void LowPrecisionTransformations::setQuantizedTensorAlignmentOnActivations(
    const LayerTransformation::QuantizedTensorAlignment quantizedTensorAlignmentOnActivations) {
    for (auto it = branchSpecificTransformations.begin(); it != branchSpecificTransformations.end(); ++it) {
        it->second->setQuantizedTensorAlignmentOnActivations(quantizedTensorAlignmentOnActivations);
    }
    for (auto it = transformations.begin(); it != transformations.end(); ++it) {
        it->second->setQuantizedTensorAlignmentOnActivations(quantizedTensorAlignmentOnActivations);
    }
}

void LowPrecisionTransformations::setQuantizedTensorAlignmentOnWeights(
    const LayerTransformation::QuantizedTensorAlignment quantizedTensorAlignmentOnWeights) {
    for (auto it = branchSpecificTransformations.begin(); it != branchSpecificTransformations.end(); ++it) {
        it->second->setQuantizedTensorAlignmentOnWeights(quantizedTensorAlignmentOnWeights);
    }
    for (auto it = transformations.begin(); it != transformations.end(); ++it) {
        it->second->setQuantizedTensorAlignmentOnWeights(quantizedTensorAlignmentOnWeights);
    }
}

LowPrecisionTransformations& LowPrecisionTransformations::remove(const std::string& layerName) {
    removeBranchSpecificTransformations(layerName);
    removeTransformations(layerName);
    removeCleanupTransformations(layerName);
    return *this;
}

LowPrecisionTransformations& LowPrecisionTransformations::removeBranchSpecificTransformations(const std::string& layerName) {
    branchSpecificTransformations.erase(layerName);
    return *this;
}

LowPrecisionTransformations& LowPrecisionTransformations::removeTransformations(const std::string& layerName) {
    transformations.erase(layerName);
    return *this;
}

LowPrecisionTransformations& LowPrecisionTransformations::removeCleanupTransformations(const std::string& layerName) {
    cleanupTransformations.erase(layerName);
    return *this;
}

LayerTransformationPtr LowPrecisionTransformations::find(const std::string& transformationKey) const {
    auto it = branchSpecificTransformations.find(transformationKey);
    if (it != branchSpecificTransformations.end()) {
        return it->second;
    }

    it = transformations.find(transformationKey);
    if (it != transformations.end()) {
        return it->second;
    }

    it = cleanupTransformations.find(transformationKey);
    if (it != cleanupTransformations.end()) {
        return it->second;
    }

    return nullptr;
}

void LowPrecisionTransformations::setParamsManager(IParamsManager* paramsManager) noexcept {
    setParamsManager(paramsManager, branchSpecificTransformations);
    setParamsManager(paramsManager, transformations);
    setParamsManager(paramsManager, cleanupTransformations);
}

void LowPrecisionTransformations::setLayerTransformationsManager(ILayerTransformationsManager* layerTransformationsManager) noexcept {
    setLayerTransformationsManager(layerTransformationsManager, branchSpecificTransformations);
    setLayerTransformationsManager(layerTransformationsManager, transformations);
    setLayerTransformationsManager(layerTransformationsManager, cleanupTransformations);
}

void LowPrecisionTransformations::setParamsManager(
    IParamsManager* paramsManager,
    std::map<std::string, LayerTransformationPtr>& transformations) noexcept {
    for (auto it : transformations) {
        it.second->setParamsManager(paramsManager);
    }
}

void LowPrecisionTransformations::setLayerTransformationsManager(
    ILayerTransformationsManager* layerTransformationsManager,
    std::map<std::string, LayerTransformationPtr>& transformations) noexcept {
    for (auto it : transformations) {
        it.second->setLayerTransformationsManager(layerTransformationsManager);
    }
}

LowPrecisionTransformations LowPrecisionTransformer::getAllTransformations(const LayerTransformation::Params& params) {
    // return LowPrecisionTransformations(
    //    std::map<std::string, LayerTransformationPtr>({
    //        { "Concat", LayerTransformationPtr(new ConcatTransformation(params))}
    //        // { "Concat", LayerTransformationPtr(new ConcatMultiChannelsTransformation(params))}
    //    }),
    //    std::map<std::string, LayerTransformationPtr>({
    //        { "AvgPool", LayerTransformationPtr(new AvgPoolTransformation(params)) },
    //        { "Convolution", LayerTransformationPtr(new ConvolutionTransformation(params)) },
    //        { "GroupConvolution", LayerTransformationPtr(new GroupConvolutionTransformation(params)) },
    //        { "MaxPool", LayerTransformationPtr(new MaxPoolTransformation(params)) },
    //        { "FakeQuantize", LayerTransformationPtr(new FakeQuantizeTransformation(params)) },
    //        // { "Reshape", LayerTransformationPtr(new ReshapeTransformation(params)) },
    //        // { "MatMul", LayerTransformationPtr(new MatMulTransformation(params)) },
    //        //// { "Transpose", LayerTransformationPtr(new TransposeTransformation(params)) },
    //        //// { "Squeeze", LayerTransformationPtr(new SqueezeTransformation(params)) },
    //        { "ReLU", LayerTransformationPtr(new ReluTransformation(params)) },
    //        //// { "MVN", LayerTransformationPtr(new MvnTransformation(params)) },
    //        { "Add", LayerTransformationPtr(new AddTransformation(params)) },
    //        //// { "Interpolate", LayerTransformationPtr(new InterpolateTransformation(params)) },
    //        // { "DepthToSpace", LayerTransformationPtr(new DepthToSpaceTransformation(params)) },
    //        // { "NormalizeL2", LayerTransformationPtr(new NormalizeL2Transformation(params)) }
    //    }),
    //    std::map<std::string, LayerTransformationPtr>({
    //        // fuse FakeQuantize with dequantization operations BEFORE FakeQuantize, AFTER is not implemented yet
    //        { "FakeQuantize", LayerTransformationPtr(new FuseFakeQuantizeTransformation(params)) },
    //        // { "ScaleShift", LayerTransformationPtr(new ScaleShiftToConvolutionTransformation(params)) },  // ???
    //        // { "MultiplyAdd", LayerTransformationPtr(new DecomposeMultiplyAddTransformation(params)) },
    //        // { "Subtract", LayerTransformationPtr(new SubtractTransformation(params)) },
    //        { "Multiply", LayerTransformationPtr(new MultiplyTransformation(params)) },
    //    }));

    using namespace pass::low_precision;

    // one operation type => one transformation
    // TODO: refactor: duplication: declaration & registerMatcherIn
    return LowPrecisionTransformations().
        addBranchSpecific<pass::low_precision::ConcatTransformation, opset1::Concat>(params).

        add<AddTransformation, opset1::Add>(params).
        add<AvgPoolTransformation, opset1::AvgPool>(params).
        add<ConvolutionTransformation, opset1::Convolution>(params).
        //add<DepthToSpaceTransformation, opset1::DepthToSpace>(params).
        add<FakeQuantizeTransformation, opset1::FakeQuantize>(params).
        add<GroupConvolutionTransformation, opset1::GroupConvolution>(params).
        add<MatMulTransformation, opset1::MatMul>(params).
        add<MaxPoolTransformation, opset1::MaxPool>(params).
        //add<NormalizeL2Transformation, opset1::NormalizeL2>(params).
        add<ReluTransformation, opset1::Relu>(params).
        // Multiply const change is not supported
        // add<ReshapeTransformation, opset1::Reshape>(params).

        addCleanup<FuseFakeQuantizeTransformation, opset1::FakeQuantize>(params).
        addCleanup<MultiplyTransformation, opset1::Multiply>(params).
        //// TODO: workaround: Convert I8 -> FP32 is not supported by CPU plugin
        addCleanup<ConvertTransformation, opset1::Convert>(params);
}

LowPrecisionTransformer::LowPrecisionTransformer(): transformations(LowPrecisionTransformer::getAllTransformations()) {}

template <typename BaseOp>
void make_matcher_type_relaxed(ngraph::pass::GraphRewrite* transformation) {
    using namespace ngraph;

    auto is_op_type = [](std::shared_ptr<Node> n) {
        return !!as_type_ptr<BaseOp>(n);
    };

    auto p_node = std::make_shared<pattern::op::Label>(element::f32, Shape{}, is_op_type);

    ngraph::graph_rewrite_callback callback = [](ngraph::pattern::Matcher &m) {
        auto l_node = std::dynamic_pointer_cast<BaseOp>(m.get_match_root());
        if (!l_node) {
            std::cerr << "Error my matcher 1!!!\n";
            return false;
        }
        // std::cerr << "My matcher pass was triggered: " << l_node->get_friendly_name() << " with " << l_node->get_inputs().size() << " inputs\n";
        // TODO: replaces only operation with one output port
        auto replacement = std::make_shared<ngraph::op::TypeRelaxed<BaseOp>>(*l_node, l_node->get_output_element_type(0));
        // auto replacement = std::make_shared<BaseOp>(*l_node);
        copy_runtime_info(l_node, replacement);
        replace_node(l_node, replacement);
        return true;
    };

    auto m = std::make_shared<ngraph::pattern::Matcher>(p_node, "TypeRelaxedReplacer");
    transformation->add_matcher(m, callback, ngraph::pass::PassProperty::CHANGE_DYNAMIC_STATE);
}

TypeRelaxedReplacer::TypeRelaxedReplacer() {
    // List all operations that support polymorphic inputs/outputs
    make_matcher_type_relaxed<opset1::AvgPool>(this);
    make_matcher_type_relaxed<opset1::Concat>(this);
    make_matcher_type_relaxed<opset1::Convolution>(this);
    make_matcher_type_relaxed<opset1::FakeQuantize>(this);
    make_matcher_type_relaxed<opset1::GroupConvolution>(this);
    make_matcher_type_relaxed<opset1::Relu>(this);
    make_matcher_type_relaxed<opset1::MaxPool>(this);
    make_matcher_type_relaxed<opset1::Add>(this);
    make_matcher_type_relaxed<opset1::Subtract>(this);
    make_matcher_type_relaxed<ngraph::op::Subtract>(this);
    make_matcher_type_relaxed<opset1::NormalizeL2>(this);

    // TODO: can we do it?
    make_matcher_type_relaxed<opset1::Multiply>(this);
}

LowPrecisionTransformer::LowPrecisionTransformer(const LowPrecisionTransformations& transformations)
    : transformations(transformations) {}

#if 0 // TODO LPT-TO-NGRAPH
void LowPrecisionTransformer::renameLayersByType(const std::vector<CNNLayerPtr>& layers, const std::string& type) {
    size_t number = 1;
    for (size_t i = 0; i < layers.size(); ++i) {
        const CNNLayerPtr layer = layers[i];
        if (layer->type != type) {
            continue;
        }

        layer->name = layer->type + std::to_string(number);
        ++number;
    }
}

void LowPrecisionTransformer::rename(ICNNNetwork& network) const {
    TransformationContext context(network);

    const std::unordered_set<std::string> standaloneLayerTypes = {"Convolution", "Concat",  "Eltwise",
                                                                  "Reshape",     "Pooling", "Clamp"};
    for (const std::string& standaloneLayerType : standaloneLayerTypes) {
        renameLayersByType(context.getLayers(), standaloneLayerType);
    }

    size_t fakeQuantizeNumber = 1;
    for (size_t i = 0lu; i < context.getLayers().size(); ++i) {
        const CNNLayerPtr layer = context.getLayers()[i];
        if (layer->type != "FakeQuantize") {
            continue;
        }

        const std::vector<CNNLayerPtr> children = CNNNetworkHelper::getChildren(*layer);
        if ((children.size() == 1) && (children[0]->type == "Convolution")) {
            const std::string postfix = CNNNetworkHelper::getIndex(*layer) == 0 ? "data" : "weights";
            layer->name = children[0]->name + "_FakeQuantize_" + postfix;
        } else {
            layer->name = layer->type + std::to_string(fakeQuantizeNumber);
            ++fakeQuantizeNumber;
        }
    }

    size_t otherNumber = 1;
    for (size_t i = 0; i < context.getLayers().size(); ++i) {
        std::string name;
        const CNNLayerPtr layer = context.getLayers()[i];
        if ((standaloneLayerTypes.find(layer->type) != standaloneLayerTypes.end()) || (layer->type == "FakeQuantize")) {
            continue;
        }

        if (layer->type == "Const") {
            const std::vector<CNNLayerPtr> children = CNNNetworkHelper::getChildren(*layer);
            if (children.size() == 1) {
                if (children[0]->type == "Convolution") {
                    const std::string postfix = CNNNetworkHelper::getIndex(*layer) == 1 ? "weights" : "biases";
                    name = children[0]->name + "_Const_" + postfix;
                } else if (children[0]->type == "FakeQuantize") {
                    name = children[0]->name + "_Const_" + std::to_string(CNNNetworkHelper::getIndex(*layer));
                }
            }
        }

        if (name.empty()) {
            name = layer->type + std::to_string(otherNumber);
            ++otherNumber;
        }

        layer->name = name;
    }
}

#endif

void LowPrecisionTransformer::transform(std::shared_ptr<Function> network) {
#if 0 // TODO LPT-TO-NGRAPH
#ifdef LPT_ORIGINAL_MODEL_PATH
    ResponseDesc originalModelResponse;
    network.serialize(
        std::string(LPT_ORIGINAL_MODEL_PATH) + ".xml",
        std::string(LPT_ORIGINAL_MODEL_PATH) + ".bin",
        &originalModelResponse);
    if (originalModelResponse.msg[0] != '\0') {
        THROW_TRANSFORMATION_EXCEPTION << "LowPrecisionTransformer::transform: " << LPT_ORIGINAL_MODEL_PATH << ": " << originalModelResponse.msg;
    }
#endif
#endif

#if 0 // TODO Check all FQ.level values are supported (simple); for now it is supposed all are supported

    auto it = details::CNNNetworkIterator(&network);
    auto end = details::CNNNetworkIterator();
    bool fqFound = false;
    bool allFQareUnsupported = true;
    while (it != end) {
        if (CaselessEq<std::string>()((*it)->type, "FakeQuantize")) {
            fqFound = true;
            if (QuantizationDetails::isSupportedLevel((*it)->GetParamAsUInt("levels"))) {
                allFQareUnsupported = false;
                break;
            }
        }
        it++;
    }
    // If network does not have FakeQuantize layers
    // or all found FQ layers are binary - do nothing and return
    if (!fqFound || allFQareUnsupported) return;

#endif

    // {
    //    std::vector<std::shared_ptr<ngraph::Function>> module{ network };
    //    VisualizeTree("C:\\Projects\\temp\\test.original").run_on_module(module);
    // }

    transformations.setParamsManager(this);
    transformations.setLayerTransformationsManager(this);

    TransformationContext context(network);

    // Extend necessary operations with polymorphic semantics
    {
        TypeRelaxedReplacer pass;
        pass.run_on_function(network);
    }

    { // Branch specific transformations
        GraphRewrite pass;
        registerAllMatchers(transformations.branchSpecificTransformations, pass, context);
        pass.run_on_function(network);
    }

    { // Step #1: FakeQuantize layer transformation execution
        LayerTransformationPtr fqTransformation = transformations.find<opset1::FakeQuantize>();
        if (fqTransformation == nullptr) {
            THROW_TRANSFORMATION_EXCEPTION << "FakeQuantize transformation was not found";
        }
        GraphRewrite pass;
        fqTransformation->registerMatcherIn(pass, context);
        pass.run_on_function(network);
    }

    { // Step #2: layer transformations execution
        GraphRewrite pass;
        registerAllMatchers(transformations.transformations, pass, context);
        pass.run_on_function(network);

        #if 0 // TODO LPT-TO-NGRAPH
        // TODO(slyalin) Find a new place for this dump inside nGraph -- it cannot be executed here
        #ifdef DISPLAY_PECISION
                CNNLayerPtr transformedLayer = CNNNetworkHelper::getLayer(context.network, layer->name);
                if (transformedLayer == nullptr) {
                    if (layer->type == "FakeQuantize") {
                        std::cout << "Layer " << layer->name << ": " << QuantizationDetails::getDetails(*layer) << std::endl;
                    }

                    std::cout << "Layer was " << (transformed ? "transformed: " : "skipped: ") << layer->type << ", "
                            << layer->name << ": [REMOVED]" << std::endl;
                } else {
                    if (transformedLayer->type == "FakeQuantize") {
                        std::cout << "Layer " << transformedLayer->name << ": "
                                << QuantizationDetails::getDetails(*transformedLayer) << std::endl;
                    }

                    std::cout << "Layer was " << (transformed ? "transformed: " : "skipped: ") << transformedLayer->type << ", "
                            << transformedLayer->name << ", output layer precision: "
                            << ((transformedLayer->outData.size() != 0) ? transformedLayer->outData[0]->getPrecision()
                                                                        : Precision::UNSPECIFIED)
                            << std::endl;
                }

        #endif
        #endif
    }

    // {
    //    std::vector<std::shared_ptr<ngraph::Function>> module{network};
    //    ngraph::pass::VisualizeTree("after_layers_replacement.svg").run_on_module(module);
    // }


    { // Step #3: cleanup transformations execution
        GraphRewrite pass;
        registerAllMatchers(transformations.cleanupTransformations, pass, context);
        pass.run_on_function(network);
    }

    // {
    //    std::vector<std::shared_ptr<ngraph::Function>> module{ network };
    //    VisualizeTree("C:\\Projects\\temp\\test.transformed").run_on_module(module);
    // }


#if 0 // TODO LPT-TO-NGRAPH
#ifdef LPT_TRANSFORMED_MODEL_PATH
    ResponseDesc transformedModelResponse;
    network.serialize(
        std::string(LPT_TRANSFORMED_MODEL_PATH) + ".xml",
        std::string(LPT_TRANSFORMED_MODEL_PATH) + ".bin",
        &transformedModelResponse);
    if (transformedModelResponse.msg[0] != '\0') {
        THROW_TRANSFORMATION_EXCEPTION << "LowPrecisionTransformer::transform: " << LPT_TRANSFORMED_MODEL_PATH << ": " << transformedModelResponse.msg;
    }
#endif
#endif
}

std::vector<element::Type> LowPrecisionTransformer::getPrecisionsOnActivations(const Node& op) const noexcept {
    const LayerTransformationPtr transformation = transformations.find(LowPrecisionTransformations::getType(op));
    if (transformation == nullptr) {
        return std::vector<element::Type>();
    }
    return transformation->getPrecisionsOnActivations();
}

bool LowPrecisionTransformer::isQuantized(std::shared_ptr<Node> layer) const noexcept {
    const std::string operantionType = LowPrecisionTransformations::getType(*layer);

    const LayerTransformationPtr transformation = transformations.find(operantionType);
    if (transformation == nullptr) {
        return false;
    }
    return transformation->isQuantized(layer);
}

bool LowPrecisionTransformer::isPrecisionPreserved(std::shared_ptr<Node> layer) const noexcept {
    const std::string operantionType = LowPrecisionTransformations::getType(*layer);
    const LayerTransformationPtr transformation = transformations.find(operantionType);
    if (transformation == nullptr) {
        return false;
    }
    return transformation->isPrecisionPreserved(layer);
}

void LowPrecisionTransformer::registerAllMatchers(
    std::map<std::string, LayerTransformationPtr> transformations,
    GraphRewrite& pass,
    TransformationContext& context) {
    for (auto it : transformations) {
        it.second->registerMatcherIn(pass, context);
    }
}

} // namespace low_precision
} // namespace pass
} // namespace ngraph