#include "NvInfer.h"
#include "NvInferPlugin.h"
#include <cmath>

#include "../utils/weights.h"

using namespace nvinfer1;

#define USE_FP16
static int get_width(int x, float gw, int divisor = 8)  {
    //return math.ceil(x / divisor) * divisor
    if (int(x * gw) % divisor == 0) {
        return int(x * gw);
    }
    return (int(x * gw / divisor) + 1) * divisor;
}

static int get_depth(int x, float gd) {
    if (x == 1) {
        return 1;
    } else {
        return round(x * gd) > 1 ? round(x * gd) : 1;
    }
}
namespace yolov4p5 {

// stuff we know about the network and the input/output blobs
static constexpr int MAX_OUTPUT_BBOX_COUNT = 1000;
static constexpr int CLASS_NUM = 8;
static constexpr int INPUT_H = 896;
static constexpr int INPUT_W = 896;

static const int YOLO_FACTOR_1 = 8;
static const std::vector<float> YOLO_ANCHORS_1 = {12, 16, 19, 36, 40, 28};
static const float YOLO_SCALE_XY_1 = 1.2f;
static const int YOLO_NEWCOORDS_1 = 0;

static const int YOLO_FACTOR_2 = 16;
static const std::vector<float> YOLO_ANCHORS_2 = {36, 75, 76, 55, 72, 146};
static const float YOLO_SCALE_XY_2 = 1.1f;
static const int YOLO_NEWCOORDS_2 = 0;

static const int YOLO_FACTOR_3 = 32;
static const std::vector<float> YOLO_ANCHORS_3 = {142, 110, 192, 243, 459, 401};
static const float YOLO_SCALE_XY_3 = 1.05f;
static const int YOLO_NEWCOORDS_3 = 0;

const char *INPUT_BLOB_NAME = "input";
const char *OUTPUT_BLOB_NAME = "detections";

std::vector<float> getAnchors(std::map<std::string, Weights>& weightMap)
{
    std::vector<float> anchors_yolo;
    Weights Yolo_Anchors = weightMap["model.31.anchor_grid"];
    assert(Yolo_Anchors.count == 24);
    int each_yololayer_anchorsnum = Yolo_Anchors.count / 3;
    const float* tempAnchors = (const float*)(Yolo_Anchors.values);
    for (int i = 0; i < Yolo_Anchors.count; i++)
    {
        if (i < each_yololayer_anchorsnum)
        {
            anchors_yolo.push_back(const_cast<float*>(tempAnchors)[i]);
        }
        if ((i >= each_yololayer_anchorsnum) && (i < (2 * each_yololayer_anchorsnum)))
        {
            anchors_yolo.push_back(const_cast<float*>(tempAnchors)[i]);
        }
        if (i >= (2 * each_yololayer_anchorsnum))
        {
            anchors_yolo.push_back(const_cast<float*>(tempAnchors)[i]);
        }
    }
    return anchors_yolo;
}
IPluginV2Layer* addYoLoLayer(INetworkDefinition *network, std::map<std::string, Weights>& weightMap, IConvolutionLayer* det0, IConvolutionLayer* det1, IConvolutionLayer* det2)
{
    auto creator = getPluginRegistry()->getPluginCreator("YoloLayer_TRT", "1");
    // std::vector<float> anchors = getAnchors(weightMap);
    std::vector<float> anchors = YOLO_ANCHORS_1 ;//getAnchors(weightMap);
    // PluginField pluginMultidata[4];
    // int NetData[4];
    // NetData[0] = CLASS_NUM;
    // NetData[1] = INPUT_W;
    // NetData[2] = INPUT_H;
    // NetData[3] = MAX_OUTPUT_BBOX_COUNT;
    // pluginMultidata[0].data = NetData;
    // pluginMultidata[0].length = 3;
    // pluginMultidata[0].name = "netdata";
    // pluginMultidata[0].type = PluginFieldType::kFLOAT32;
    // int scale[3] = { 8, 16, 32 };
    // int plugindata[3][10];
    // std::string names[3];
    // for (int k = 1; k < 4; k++)
    // {
    //     plugindata[k - 1][0] = INPUT_W / scale[k - 1];
    //     plugindata[k - 1][1] = INPUT_H / scale[k - 1];
    //     for (int i = 2; i < 10; i++)
    //     {
    //         plugindata[k - 1][i] = int(anchors_yolo[(k - 1) * 8 + i - 2]);
    //     }
    //     pluginMultidata[k].data = plugindata[k - 1];
    //     pluginMultidata[k].length = 10;
    //     names[k - 1] = "yolodata" + std::to_string(k);
    //     pluginMultidata[k].name = names[k - 1].c_str();
    //     pluginMultidata[k].type = PluginFieldType::kFLOAT32;
    // }
    // PluginFieldCollection pluginData;
    // pluginData.nbFields = 4;

    // pluginData.fields = pluginMultidata;
  int yoloWidth = INPUT_W / YOLO_FACTOR_1;
  int yoloHeight = INPUT_H / YOLO_FACTOR_2;
  int numAnchors = anchors.size() / 2;
  int numClasses = CLASS_NUM;
  int widthFactor = 32; //?????????
  float scaleXY = YOLO_SCALE_XY_1;
 // std::cout << sc
  int newCoords = YOLO_NEWCOORDS_1;
  PluginFieldCollection pluginData;
  std::vector<PluginField> pluginFields;
  pluginFields.emplace_back(
      PluginField("yoloWidth", &yoloWidth, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(
      PluginField("yoloHeight", &yoloHeight, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(
      PluginField("numAnchors", &numAnchors, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(
      PluginField("numClasses", &numClasses, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(
      PluginField("inputMultiplier", &widthFactor, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(PluginField(
      "anchors", anchors.data(), PluginFieldType::kFLOAT32, anchors.size()));
  pluginFields.emplace_back(
      PluginField("scaleXY", &scaleXY, PluginFieldType::kFLOAT32, 1));
  pluginFields.emplace_back(
      PluginField("newCoords", &newCoords, PluginFieldType::kINT32, 1));
  pluginData.nbFields = pluginFields.size();
  pluginData.fields = pluginFields.data();


  std::cout << "Here " << std::endl;
    IPluginV2 *pluginObj = creator->createPlugin("YoloLayer_TRT", &pluginData);
    ITensor* inputTensors_yolo[] = { det2->getOutput(0), det1->getOutput(0), det0->getOutput(0) };
    auto yolo = network->addPluginV2(inputTensors_yolo, 3, *pluginObj);
    return yolo;
    /*
// TODO: Make sure this is identical with the original implemention
  int yoloWidth = inputWidth / widthFactor;
  int yoloHeight = inputHeight / heightFactor;
  int numAnchors = anchors.size() / 2;

  PluginFieldCollection pluginData;
  std::vector<PluginField> pluginFields;
  pluginFields.emplace_back(
      PluginField("yoloWidth", &yoloWidth, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(
      PluginField("yoloHeight", &yoloHeight, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(
      PluginField("numAnchors", &numAnchors, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(
      PluginField("numClasses", &numClasses, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(
      PluginField("inputMultiplier", &widthFactor, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(PluginField(
      "anchors", anchors.data(), PluginFieldType::kFLOAT32, anchors.size()));
  pluginFields.emplace_back(
      PluginField("scaleXY", &scaleXY, PluginFieldType::kFLOAT32, 1));
  pluginFields.emplace_back(
      PluginField("newCoords", &newCoords, PluginFieldType::kINT32, 1));
  pluginData.nbFields = pluginFields.size();
  pluginData.fields = pluginFields.data();

  IPluginV2 *plugin = creator->createPlugin("YoloLayer_TRT", &pluginData);
  ITensor *inputTensors[] = {&input};
      */
}
  IScaleLayer *addBatchNorm2d(INetworkDefinition *network,
                            std::map<std::string, Weights> &weightMap,
                            ITensor &input, std::string lname, float eps) {
  float *gamma = (float *)weightMap[lname + ".weight"].values;
  float *beta = (float *)weightMap[lname + ".bias"].values;
  float *mean = (float *)weightMap[lname + ".running_mean"].values;
  float *var = (float *)weightMap[lname + ".running_var"].values;
  int len = weightMap[lname + ".running_var"].count;
//   if (len == 0)
//     len =1;
// std::cout<<len<<std::endl;
  float *scval = reinterpret_cast<float *>(malloc(sizeof(float) * len));
  for (int i = 0; i < len; i++) {
    scval[i] = gamma[i] / sqrt(var[i] + eps);
  }
  Weights scale{DataType::kFLOAT, scval, len};

  float *shval = reinterpret_cast<float *>(malloc(sizeof(float) * len));
  for (int i = 0; i < len; i++) {
    shval[i] = beta[i] - mean[i] * gamma[i] / sqrt(var[i] + eps);
  }
  Weights shift{DataType::kFLOAT, shval, len};

  float *pval = reinterpret_cast<float *>(malloc(sizeof(float) * len));
  for (int i = 0; i < len; i++) {
    pval[i] = 1.0;
  }
  Weights power{DataType::kFLOAT, pval, len};

  weightMap[lname + ".scale"] = scale;
  weightMap[lname + ".shift"] = shift;
  weightMap[lname + ".power"] = power;
  IScaleLayer *scale_1 =
      network->addScale(input, ScaleMode::kCHANNEL, shift, scale, power);
  assert(scale_1);
  return scale_1;
}

ILayer *convBnMish(INetworkDefinition *network,
                   std::map<std::string, Weights> &weightMap, ITensor &input,
                   int outch, int ksize, int s, int p, int linx) {
  Weights emptywts{DataType::kFLOAT, nullptr, 0};
  IConvolutionLayer *conv1 = network->addConvolutionNd(
      input, outch, DimsHW{ksize, ksize},
      weightMap["model." + std::to_string(linx) + ".conv.weight"], emptywts);
  assert(conv1);
  conv1->setStrideNd(DimsHW{s, s});
  conv1->setPaddingNd(DimsHW{p, p});

  IScaleLayer *bn1 =
      addBatchNorm2d(network, weightMap, *conv1->getOutput(0),
                     "model." + std::to_string(linx) + ".bn", 1e-4);

  auto mish_softplus =
      network->addActivation(*bn1->getOutput(0), ActivationType::kSOFTPLUS);
  auto mish_tanh = network->addActivation(*mish_softplus->getOutput(0),
                                          ActivationType::kTANH);
  auto mish_mul =
      network->addElementWise(*mish_tanh->getOutput(0), *bn1->getOutput(0),
                              ElementWiseOperation::kPROD);

  return mish_mul;
}

ILayer *convBnLeaky(INetworkDefinition *network,
                    std::map<std::string, Weights> &weightMap, ITensor &input,
                    int outch, int ksize, int s, int p, int linx) {
  Weights emptywts{DataType::kFLOAT, nullptr, 0};
  IConvolutionLayer *conv1 = network->addConvolutionNd(
      input, outch, DimsHW{ksize, ksize},
      weightMap["model." + std::to_string(linx) + ".conv.weight"], emptywts);
  assert(conv1);
  conv1->setStrideNd(DimsHW{s, s});
  conv1->setPaddingNd(DimsHW{p, p});

  IScaleLayer *bn1 =
      addBatchNorm2d(network, weightMap, *conv1->getOutput(0),
                     "model." + std::to_string(linx) + ".bn", 1e-4);

  auto lr =
      network->addActivation(*bn1->getOutput(0), ActivationType::kLEAKY_RELU);
  lr->setAlpha(0.1);

  return lr;
}

IPluginV2Layer *yoloLayer(INetworkDefinition *network, ITensor &input,
                          int inputWidth, int inputHeight, int widthFactor,
                          int heightFactor, int numClasses,
                          const std::vector<float> &anchors, float scaleXY,
                          int newCoords) {
  auto creator = getPluginRegistry()->getPluginCreator("YoloLayer_TRT", "1");

  int yoloWidth = inputWidth / widthFactor;
  int yoloHeight = inputHeight / heightFactor;
  int numAnchors = anchors.size() / 2;

  PluginFieldCollection pluginData;
  std::vector<PluginField> pluginFields;
  pluginFields.emplace_back(
      PluginField("yoloWidth", &yoloWidth, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(
      PluginField("yoloHeight", &yoloHeight, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(
      PluginField("numAnchors", &numAnchors, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(
      PluginField("numClasses", &numClasses, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(
      PluginField("inputMultiplier", &widthFactor, PluginFieldType::kINT32, 1));
  pluginFields.emplace_back(PluginField(
      "anchors", anchors.data(), PluginFieldType::kFLOAT32, anchors.size()));
  pluginFields.emplace_back(
      PluginField("scaleXY", &scaleXY, PluginFieldType::kFLOAT32, 1));
  pluginFields.emplace_back(
      PluginField("newCoords", &newCoords, PluginFieldType::kINT32, 1));
  pluginData.nbFields = pluginFields.size();
  pluginData.fields = pluginFields.data();

  IPluginV2 *plugin = creator->createPlugin("YoloLayer_TRT", &pluginData);
  ITensor *inputTensors[] = {&input};
  return network->addPluginV2(inputTensors, 1, *plugin);
}
ILayer *convBlock(INetworkDefinition *network,
                  std::map<std::string, Weights> &weightMap, ITensor &input,
                  int outch, int ksize, int s, int g, std::string lname) {
  Weights emptywts{DataType::kFLOAT, nullptr, 0};
  int p = ksize / 2;
  IConvolutionLayer *conv1 =
      network->addConvolutionNd(input, outch, DimsHW{ksize, ksize},
                                weightMap[lname + ".conv.weight"], emptywts);
  assert(conv1);
  conv1->setStrideNd(DimsHW{s, s});
  conv1->setPaddingNd(DimsHW{p, p});
  conv1->setNbGroups(g);
  IScaleLayer *bn1 = addBatchNorm2d(network, weightMap, *conv1->getOutput(0),
                                    lname + ".bn", 1e-3);

  // auto creator = getPluginRegistry()->getPluginCreator("Mish_TRT", "1");
  // const PluginFieldCollection *pluginData = creator->getFieldNames();

  // IPluginV2 *pluginObj = creator->createPlugin(("mish" + lname).c_str(),
  // pluginData);
  ITensor *inputTensors[] = {bn1->getOutput(0)};
  // auto mish = network->addPluginV2(&inputTensors[0], 1, *pluginObj);

  auto mish_softplus =
      network->addActivation(*bn1->getOutput(0), ActivationType::kSOFTPLUS);
  auto mish_tanh = network->addActivation(*mish_softplus->getOutput(0),
                                          ActivationType::kTANH);
  auto mish_mul =
      network->addElementWise(*mish_tanh->getOutput(0), *bn1->getOutput(0),
                              ElementWiseOperation::kPROD);

  return mish_mul;
}

ILayer *bottleneck(INetworkDefinition *network,
                   std::map<std::string, Weights> &weightMap, ITensor &input,
                   int c1, int c2, bool shortcut, int g, float e,
                   std::string lname) {
  int c_ = (int)((float)c2 * e);
  auto cv1 = convBlock(network, weightMap, input, c_, 1, 1, 1, lname + ".cv1");
  auto cv2 = convBlock(network, weightMap, *cv1->getOutput(0), c2, 3, 1, g,
                       lname + ".cv2");
  if (shortcut && c1 == c2) {
    auto ew = network->addElementWise(input, *cv2->getOutput(0),
                                      ElementWiseOperation::kSUM);
    return ew;
  }
  return cv2;
}

ILayer *bottleneckCSP(INetworkDefinition *network,
                      std::map<std::string, Weights> &weightMap, ITensor &input,
                      int c1, int c2, int n, bool shortcut, int g, float e,
                      std::string lname) {
  Weights emptywts{DataType::kFLOAT, nullptr, 0};
  int c_ = (int)((float)c2 * e);
  auto cv1 = convBlock(network, weightMap, input, c_, 1, 1, 1, lname + ".cv1");
  auto cv2 = network->addConvolutionNd(
      input, c_, DimsHW{1, 1}, weightMap[lname + ".cv2.weight"], emptywts);
  ITensor *y1 = cv1->getOutput(0);
  for (int i = 0; i < n; i++) {
    auto b = bottleneck(network, weightMap, *y1, c_, c_, shortcut, g, 1.0,
                        lname + ".m." + std::to_string(i));
    y1 = b->getOutput(0);
  }
  auto cv3 = network->addConvolutionNd(
      *y1, c_, DimsHW{1, 1}, weightMap[lname + ".cv3.weight"], emptywts);

  ITensor *inputTensors1[] = {cv3->getOutput(0), cv2->getOutput(0)};
  auto cat = network->addConcatenation(inputTensors1, 2);

  IScaleLayer *bn1 = addBatchNorm2d(network, weightMap, *cat->getOutput(0),
                                    lname + ".bn", 1e-4);
  auto mish_softplus =
      network->addActivation(*bn1->getOutput(0), ActivationType::kSOFTPLUS);
  auto mish_tanh = network->addActivation(*mish_softplus->getOutput(0),
                                          ActivationType::kTANH);
  auto mish_mul =
      network->addElementWise(*mish_tanh->getOutput(0), *bn1->getOutput(0),
                              ElementWiseOperation::kPROD);

  auto cv4 = convBlock(network, weightMap, *mish_mul->getOutput(0), c2, 1, 1, 1,
                       lname + ".cv4");
  return cv4;
}

ILayer *bottleneckCSP2(INetworkDefinition *network,
                       std::map<std::string, Weights> &weightMap,
                       ITensor &input, int c1, int c2, int n, bool shortcut,
                       int g, float e, std::string lname) {
  Weights emptywts{DataType::kFLOAT, nullptr, 0};
  int c_ = (int)(c2);
  auto cv1 = convBlock(network, weightMap, input, c_, 1, 1, 1, lname + ".cv1");

  auto cv2 =
      network->addConvolutionNd(*cv1->getOutput(0), c_, DimsHW{1, 1},
                                weightMap[lname + ".cv2.weight"], emptywts);

  ITensor *y1 = cv1->getOutput(0);
  for (int i = 0; i < n; i++) {
    auto b = bottleneck(network, weightMap, *y1, c_, c_, shortcut, g, 1.0,
                        lname + ".m." + std::to_string(i));
    y1 = b->getOutput(0);
  }

  ITensor *inputTensors1[] = {y1, cv2->getOutput(0)};
  auto cat = network->addConcatenation(inputTensors1, 2);

  IScaleLayer *bn1 = addBatchNorm2d(network, weightMap, *cat->getOutput(0),
                                    lname + ".bn", 1e-4);

  // const PluginFieldCollection *pluginData = creator->getFieldNames();
  auto mish_softplus =
      network->addActivation(*bn1->getOutput(0), ActivationType::kSOFTPLUS);
  auto mish_tanh = network->addActivation(*mish_softplus->getOutput(0),
                                          ActivationType::kTANH);
  auto mish_mul =
      network->addElementWise(*mish_tanh->getOutput(0), *bn1->getOutput(0),
                              ElementWiseOperation::kPROD);

  auto cv3 = convBlock(network, weightMap, *mish_mul->getOutput(0), c2, 1, 1, 1,
                       lname + ".cv3");
  return cv3;
}

ILayer *SPPCSP(INetworkDefinition *network,
               std::map<std::string, Weights> &weightMap, ITensor &input,
               int c1, int c2, float e, int k1, int k2, int k3,
               std::string lname) {
  Weights emptywts{DataType::kFLOAT, nullptr, 0};

  int c_ = int(2 * c2 * e);
  auto cv1 = convBlock(network, weightMap, input, c_, 1, 1, 1, lname + ".cv1");
  auto cv2 = network->addConvolutionNd(
      input, c_, DimsHW{1, 1}, weightMap[lname + ".cv2.weight"], emptywts);
  auto cv3 = convBlock(network, weightMap, *cv1->getOutput(0), c_, 3, 1, 1,
                       lname + ".cv3");
  auto cv4 = convBlock(network, weightMap, *cv3->getOutput(0), c_, 1, 1, 1,
                       lname + ".cv4");

  auto pool1 = network->addPoolingNd(*cv4->getOutput(0), PoolingType::kMAX,
                                     DimsHW{k1, k1});
  pool1->setPaddingNd(DimsHW{k1 / 2, k1 / 2});
  pool1->setStrideNd(DimsHW{1, 1});
  auto pool2 = network->addPoolingNd(*cv4->getOutput(0), PoolingType::kMAX,
                                     DimsHW{k2, k2});
  pool2->setPaddingNd(DimsHW{k2 / 2, k2 / 2});
  pool2->setStrideNd(DimsHW{1, 1});
  auto pool3 = network->addPoolingNd(*cv4->getOutput(0), PoolingType::kMAX,
                                     DimsHW{k3, k3});
  pool3->setPaddingNd(DimsHW{k3 / 2, k3 / 2});
  pool3->setStrideNd(DimsHW{1, 1});

  ITensor *inputTensors1[] = {cv4->getOutput(0), pool1->getOutput(0),
                              pool2->getOutput(0), pool3->getOutput(0)};
  auto cat1 = network->addConcatenation(inputTensors1, 4);
  auto cv5 = convBlock(network, weightMap, *cat1->getOutput(0), c_, 1, 1, 1,
                       lname + ".cv5");
  auto cv6 = convBlock(network, weightMap, *cv5->getOutput(0), c_, 3, 1, 1,
                       lname + ".cv6");

  ITensor *inputTensors2[] = {cv6->getOutput(0), cv2->getOutput(0)};
  auto cat2 = network->addConcatenation(inputTensors2, 2);

  IScaleLayer *bn1 = addBatchNorm2d(network, weightMap, *cat2->getOutput(0),
                                    lname + ".bn", 1e-4);
  // auto creator = getPluginRegistry()->getPluginCreator("Mish_TRT", "1");
  // const PluginFieldCollection *pluginData = creator->getFieldNames();
  // IPluginV2 *pluginObj = creator->createPlugin(("mish" + lname).c_str(),
  // pluginData); ITensor *inputTensors3[] = {bn1->getOutput(0)}; auto mish =
  // network->addPluginV2(&inputTensors3[0], 1, *pluginObj);
  auto mish_softplus =
      network->addActivation(*bn1->getOutput(0), ActivationType::kSOFTPLUS);
  auto mish_tanh = network->addActivation(*mish_softplus->getOutput(0),
                                          ActivationType::kTANH);
  auto mish_mul =
      network->addElementWise(*mish_tanh->getOutput(0), *bn1->getOutput(0),
                              ElementWiseOperation::kPROD);

  auto cv7 = convBlock(network, weightMap, *mish_mul->getOutput(0), c2, 1, 1, 1,
                       lname + ".cv7");
  return cv7;
}

ILayer *upSample(INetworkDefinition *network,
                 std::map<std::string, Weights> &weightMap, ITensor &input,
                 int channels) {
  float *deval =
      reinterpret_cast<float *>(malloc(sizeof(float) * channels * 2 * 2));
  for (int i = 0; i < channels * 2 * 2; i++) {
    deval[i] = 1.0;
  }
  Weights deconvwts{DataType::kFLOAT, deval, channels * 2 * 2};
  Weights emptywts{DataType::kFLOAT, nullptr, 0};
  IDeconvolutionLayer *deconv = network->addDeconvolutionNd(
      input, channels, DimsHW{2, 2}, deconvwts, emptywts);
  deconv->setStrideNd(DimsHW{2, 2});
  deconv->setNbGroups(channels);

  return deconv;
}

ICudaEngine *createEngine(unsigned int maxBatchSize, IBuilder *builder,
                          IBuilderConfig *config, DataType dt, float &gd,
                          float &gw, const std::string &weightsPath) {
  INetworkDefinition *network = builder->createNetworkV2(0U);

  // Create input tensor of shape {3, INPUT_H, INPUT_W} with name
  // INPUT_BLOB_NAME
  std::cout << "Here " << std::endl;
  ITensor *data =
      network->addInput(INPUT_BLOB_NAME, dt, Dims3{3, INPUT_H, INPUT_W});
  assert(data);

  std::map<std::string, Weights> weightMap = loadWeights(weightsPath);
  Weights emptywts{DataType::kFLOAT, nullptr, 0};

  // yolov4-p5 backbone
  auto conv0 = convBlock(network, weightMap, *data, get_width(32, gw), 3, 1, 1,
                         "model.0");
  auto conv1 = convBlock(network, weightMap, *conv0->getOutput(0),
                         get_width(64, gw), 3, 2, 1, "model.1");
  auto bottleneck_CSP2 = bottleneckCSP(
      network, weightMap, *conv1->getOutput(0), get_width(64, gw),
      get_width(64, gw), get_depth(1, gd), true, 1, 0.5, "model.2");
  auto conv3 = convBlock(network, weightMap, *bottleneck_CSP2->getOutput(0),
                         get_width(128, gw), 3, 2, 1, "model.3");
  auto bottleneck_csp4 = bottleneckCSP(
      network, weightMap, *conv3->getOutput(0), get_width(128, gw),
      get_width(128, gw), get_depth(3, gd), true, 1, 0.5, "model.4");
  auto conv5 = convBlock(network, weightMap, *bottleneck_csp4->getOutput(0),
                         get_width(256, gw), 3, 2, 1, "model.5");
  auto bottleneck_csp6 = bottleneckCSP(
      network, weightMap, *conv5->getOutput(0), get_width(256, gw),
      get_width(256, gw), get_depth(15, gd), true, 1, 0.5, "model.6");
  auto conv7 = convBlock(network, weightMap, *bottleneck_csp6->getOutput(0),
                         get_width(512, gw), 3, 2, 1, "model.7");
  auto bottleneck_csp8 = bottleneckCSP(
      network, weightMap, *conv7->getOutput(0), get_width(512, gw),
      get_width(512, gw), get_depth(15, gd), true, 1, 0.5, "model.8");
  auto conv9 = convBlock(network, weightMap, *bottleneck_csp8->getOutput(0),
                         get_width(1024, gw), 3, 2, 1, "model.9");
  auto bottleneck_csp10 = bottleneckCSP(
      network, weightMap, *conv9->getOutput(0), get_width(1024, gw),
      get_width(1024, gw), get_depth(7, gd), true, 1, 0.5, "model.10");

 std::cout << "Here " << std::endl;
  // yolov4-p5 head
  auto sppcsp11 =
      SPPCSP(network, weightMap, *bottleneck_csp10->getOutput(0),
             get_width(512, gw), get_width(512, gw), 0.5, 5, 9, 13, "model.11");
  auto conv12 = convBlock(network, weightMap, *sppcsp11->getOutput(0),
                          get_width(256, gw), 1, 1, 1, "model.12");
  auto deconv13 =
      upSample(network, weightMap, *conv12->getOutput(0), get_width(256, gw));
  auto conv14 = convBlock(network, weightMap, *bottleneck_csp8->getOutput(0),
                          get_width(256, gw), 1, 1, 1, "model.14");
  ITensor *inputTensors15[] = {conv14->getOutput(0), deconv13->getOutput(0)};
  auto cat15 = network->addConcatenation(inputTensors15, 2);
  auto bottleneck_csp16 = bottleneckCSP2(
      network, weightMap, *cat15->getOutput(0), get_width(256, gw),
      get_width(256, gw), get_depth(3, gd), false, 1, 0.5, "model.16");
  auto conv17 = convBlock(network, weightMap, *bottleneck_csp16->getOutput(0),
                          get_width(128, gw), 1, 1, 1, "model.17");
  auto deconv18 =
      upSample(network, weightMap, *conv17->getOutput(0), get_width(128, gw));
  auto conv19 = convBlock(network, weightMap, *bottleneck_csp6->getOutput(0),
                          get_width(128, gw), 1, 1, 1, "model.19");
  ITensor *inputTensors20[] = {conv19->getOutput(0), deconv18->getOutput(0)};
  auto cat20 = network->addConcatenation(inputTensors20, 2);
  auto bottleneck_csp21 = bottleneckCSP2(
      network, weightMap, *cat20->getOutput(0), get_width(128, gw),
      get_width(128, gw), get_depth(3, gd), false, 1, 0.5, "model.21");
  auto conv22 = convBlock(network, weightMap, *bottleneck_csp21->getOutput(0),
                          get_width(256, gw), 3, 1, 1, "model.22");

  auto conv23 = convBlock(network, weightMap, *bottleneck_csp21->getOutput(0),
                          get_width(256, gw), 3, 2, 1, "model.23");
  ITensor *inputTensors24[] = {conv23->getOutput(0),
                               bottleneck_csp16->getOutput(0)};
  auto cat24 = network->addConcatenation(inputTensors24, 2);
  auto bottleneck_csp25 = bottleneckCSP2(
      network, weightMap, *cat24->getOutput(0), get_width(156, gw),
      get_width(256, gw), get_depth(3, gd), false, 1, 0.5, "model.25");
  auto conv26 = convBlock(network, weightMap, *bottleneck_csp25->getOutput(0),
                          get_width(512, gw), 3, 1, 1, "model.26");

  auto conv27 = convBlock(network, weightMap, *bottleneck_csp25->getOutput(0),
                          get_width(512, gw), 3, 2, 1, "model.27");
  ITensor *inputTensors28[] = {conv27->getOutput(0), sppcsp11->getOutput(0)};
  auto cat28 = network->addConcatenation(inputTensors28, 2);
  auto bottleneck_csp29 = bottleneckCSP2(
      network, weightMap, *cat28->getOutput(0), get_width(512, gw),
      get_width(512, gw), get_depth(3, gd), false, 1, 0.5, "model.29");
  auto conv30 = convBlock(network, weightMap, *bottleneck_csp29->getOutput(0),
                          get_width(1024, gw), 3, 1, 1, "model.30");

  IConvolutionLayer *det0 = network->addConvolutionNd(
      *conv22->getOutput(0), 4 * (CLASS_NUM + 5), DimsHW{1, 1},
      weightMap["model.31.m.0.weight"], weightMap["model.31.m.0.bias"]);
  IConvolutionLayer *det1 = network->addConvolutionNd(
      *conv26->getOutput(0), 4 * (CLASS_NUM + 5), DimsHW{1, 1},
      weightMap["model.31.m.1.weight"], weightMap["model.31.m.1.bias"]);
  IConvolutionLayer *det2 = network->addConvolutionNd(
      *conv30->getOutput(0), 4 * (CLASS_NUM + 5), DimsHW{1, 1},
      weightMap["model.31.m.2.weight"], weightMap["model.31.m.2.bias"]);

  std::cout << "Here " << std::endl;
  auto yolo = addYoLoLayer(network, weightMap, det0, det1, det2);
  std::cout << "layers loaded" << std::endl;
  yolo->getOutput(0)->setName(OUTPUT_BLOB_NAME);
  network->markOutput(*yolo->getOutput(0));

  // Build engine
  builder->setMaxBatchSize(maxBatchSize);
  config->setMaxWorkspaceSize(16 * (1 << 20)); // 16MB
#ifdef USE_FP16
  config->setFlag(BuilderFlag::kFP16);
#endif
  std::cout << "Building engine, please wait for a while..." << std::endl;
  ICudaEngine *engine = builder->buildEngineWithConfig(*network, *config);
  std::cout << "Build engine successfully!" << std::endl;

  // Don't need the network any more
  network->destroy();

  // Release host memory
  for (auto &mem : weightMap) {
    free((void *)(mem.second.values));
  }
  return engine;
}

} // namespace yolov4p5
