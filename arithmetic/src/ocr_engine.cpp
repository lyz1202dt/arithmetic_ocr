#include "arithmetic/ocr_engine.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <sstream>
#include <thread>
#include <unistd.h>

#include <opencv2/imgcodecs.hpp>

#include "arithmetic/expression_utils.hpp"

namespace arithmetic {
namespace {

std::atomic<std::uint64_t> g_temp_image_counter{0};

bool IsAllowedUtf8OperatorPrefix(unsigned char byte) {
  return byte == 0xC3 || byte == 0xC3 || byte == 0xC2 || byte == 0xE2 ||
         byte == 0xEF;
}

}  // namespace

OCREngine::OCREngine(const Config &config) : pipeline_(BuildParams(config)) {}

OCREngine::Result OCREngine::Recognize(const cv::Mat &frame) {
  Result result;
  if (frame.empty()) {
    result.status = "空帧";
    return result;
  }

  const std::string temp_image_path = WriteTempImage(frame);
  const auto cleanup = [&]() { std::remove(temp_image_path.c_str()); };

  try {
    pipeline_.Predict(std::vector<std::string>{temp_image_path});
    const std::vector<OCRPipelineResult> pipeline_results = pipeline_.PipelineResult();
    cleanup();

    if (pipeline_results.empty()) {
      result.status = "OCR 无结果";
      return result;
    }

    const OCRPipelineResult &ocr_result = pipeline_results.front();
    result.raw_texts = ocr_result.rec_texts;

    std::string best_expression;
    long long best_value = 0;

    for (const std::string &raw_text : ocr_result.rec_texts) {
      std::string normalized = NormalizeText(raw_text);
      result.normalized_texts.push_back(normalized);
      if (normalized.empty()) {
        continue;
      }

      std::string candidate = normalized;
      if (!NormalizeParentheses(candidate)) {
        continue;
      }
      if (!IsAcceptableExpression(candidate)) {
        continue;
      }

      long long value = 0;
      if (!TryCalcExpression(candidate, value)) {
        continue;
      }

      if (candidate.size() > best_expression.size()) {
        best_expression = candidate;
        best_value = value;
      }
    }

    if (best_expression.empty()) {
      result.status = "未识别出合法算式";
      return result;
    }

    result.success = true;
    result.value = static_cast<int>(best_value);
    result.expression = best_expression;
    result.status = "识别成功";
    return result;
  } catch (...) {
    cleanup();
    throw;
  }
}

std::string OCREngine::NormalizeText(const std::string &text) {
  std::string normalized = text;
  ReplaceAll(normalized, "×", "*");
  ReplaceAll(normalized, "÷", "/");
  ReplaceAll(normalized, "（", "(");
  ReplaceAll(normalized, "）", ")");
  ReplaceAll(normalized, " ", "");
  ReplaceAll(normalized, "\t", "");
  ReplaceAll(normalized, "\r", "");
  ReplaceAll(normalized, "\n", "");

  std::string filtered;
  filtered.reserve(normalized.size());
  for (char ch : normalized) {
    if ((ch >= '0' && ch <= '9') || ch == '+' || ch == '-' || ch == '*' ||
        ch == '/' || ch == '(' || ch == ')') {
      filtered.push_back(ch);
    }
  }
  return filtered;
}

void OCREngine::ReplaceAll(std::string &text,
                           const std::string &from,
                           const std::string &to) {
  if (from.empty()) {
    return;
  }

  std::size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
}

OCRPipelineParams OCREngine::BuildParams(const Config &config) {
  OCRPipelineParams params;
  params.text_detection_model_name = "PP-OCRv4_mobile_det";
  params.text_detection_model_dir = config.det_model_dir;
  params.text_recognition_model_name = "PP-OCRv4_mobile_rec";
  params.text_recognition_model_dir = config.rec_model_dir;
  params.textline_orientation_model_name = "PP-LCNet_x1_0_textline_ori";
  params.textline_orientation_model_dir = config.textline_ori_model_dir;
  params.use_textline_orientation = false;
  params.use_doc_orientation_classify = false;
  params.use_doc_unwarping = false;
  params.device = config.device;
  params.enable_mkldnn = config.enable_mkldnn;
  params.mkldnn_cache_capacity = config.mkldnn_cache_capacity;
  params.cpu_threads = std::max(1, config.cpu_threads);
  params.thread_num = 1;
  params.text_recognition_batch_size = config.text_recognition_batch_size;
  return params;
}

std::string OCREngine::WriteTempImage(const cv::Mat &frame) const {
  const std::uint64_t id = g_temp_image_counter.fetch_add(1);
  std::ostringstream oss;
  oss << "/tmp/arithmetic_ocr_" << ::getpid() << "_"
      << std::this_thread::get_id() << "_" << id << ".png";
  const std::string path = oss.str();
  cv::imwrite(path, frame);
  return path;
}

}  // namespace arithmetic
