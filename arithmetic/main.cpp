#include "src/api/pipelines/ocr.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string BuildDefaultPath(const char *relative_path) {
  return std::string(ARITHMETIC_SOURCE_DIR) + "/" + relative_path;
}

const std::string kImagePath = BuildDefaultPath("test.jpg");
constexpr char kOutputDir[] = "./output";
const std::string kDetModelDir =
    BuildDefaultPath("../../../models/PP-OCRv4_mobile_det_infer");
const std::string kRecModelDir =
    BuildDefaultPath("../../../models/PP-OCRv4_mobile_rec_infer");
const std::string kTextlineOriModelDir =
    BuildDefaultPath("../../../models/PP-LCNet_x1_0_textline_ori_infer");

PaddleOCRParams CreateDefaultParams() {
  PaddleOCRParams params;
  params.text_detection_model_name = "PP-OCRv4_mobile_det";
  params.text_detection_model_dir = kDetModelDir;
  params.text_recognition_model_name = "PP-OCRv4_mobile_rec";
  params.text_recognition_model_dir = kRecModelDir;
  params.textline_orientation_model_name = "PP-LCNet_x1_0_textline_ori";
  params.textline_orientation_model_dir = kTextlineOriModelDir;
  params.use_textline_orientation = false;
  params.use_doc_orientation_classify = false;
  params.use_doc_unwarping = false;
  params.device = "cpu";
  params.enable_mkldnn = true;
  params.mkldnn_cache_capacity = 20;
  params.cpu_threads = std::max(1u, std::thread::hardware_concurrency());
  params.thread_num = 1;
  params.text_recognition_batch_size = 8;
  return params;
}

void PrintAndSaveResults(
    const std::vector<std::unique_ptr<BaseCVResult>> &results) {
  for (const auto &result : results) {
    result->Print();
    result->SaveToJson(kOutputDir);
  }
}

PaddleOCR &GetOCRInstance() {
  static PaddleOCR ocr(CreateDefaultParams());
  return ocr;
}

long long ElapsedMs(std::chrono::steady_clock::time_point start,
                    std::chrono::steady_clock::time_point end) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
      .count();
}

}  // namespace

int main() {
  try {
    const auto init_start = std::chrono::steady_clock::now();
    PaddleOCR &ocr = GetOCRInstance();
    const auto init_end = std::chrono::steady_clock::now();

    const auto predict_start = std::chrono::steady_clock::now();
    auto results = ocr.Predict(kImagePath);
    const auto predict_end = std::chrono::steady_clock::now();

    if (results.empty()) {
      std::cerr << "OCR 未返回任何结果" << std::endl;
      return 1;
    }

    const auto output_start = std::chrono::steady_clock::now();
    PrintAndSaveResults(results);
    const auto output_end = std::chrono::steady_clock::now();

    std::cout << "初始化耗时: " << ElapsedMs(init_start, init_end) << " ms"
              << std::endl;
    std::cout << "推理耗时: " << ElapsedMs(predict_start, predict_end) << " ms"
              << std::endl;
    std::cout << "结果输出耗时: " << ElapsedMs(output_start, output_end)
              << " ms" << std::endl;
    std::cout << "总耗时: " << ElapsedMs(init_start, output_end) << " ms"
              << std::endl;
    std::cout << "结果已输出到终端，并写入 " << kOutputDir << std::endl;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "运行 OCR 失败: " << e.what() << std::endl;
    return 1;
  }
}
