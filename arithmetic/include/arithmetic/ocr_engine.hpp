#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "src/pipelines/ocr/pipeline.h"

namespace arithmetic {

class OCREngine {
public:
  struct Config {
    std::string det_model_dir;
    std::string rec_model_dir;
    std::string textline_ori_model_dir;
    std::string device{"cpu"};
    bool enable_mkldnn{true};
    int mkldnn_cache_capacity{20};
    int cpu_threads{1};
    int text_recognition_batch_size{8};
  };

  struct Result {
    bool success{false};
    int value{0};
    std::string expression;
    std::vector<std::string> raw_texts;
    std::vector<std::string> normalized_texts;
    std::string status;
  };

  explicit OCREngine(const Config &config);

  Result Recognize(const cv::Mat &frame);

private:
  static std::string NormalizeText(const std::string &text);
  static void ReplaceAll(std::string &text,
                         const std::string &from,
                         const std::string &to);
  static OCRPipelineParams BuildParams(const Config &config);
  std::string WriteTempImage(const cv::Mat &frame) const;

  _OCRPipeline pipeline_;
  std::uint64_t temp_image_counter_{0};
};

}  // namespace arithmetic
