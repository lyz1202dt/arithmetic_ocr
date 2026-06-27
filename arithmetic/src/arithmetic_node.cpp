#include "arithmetic/arithmetic_node.hpp"

#include <chrono>
#include <cstdlib>
#include <utility>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "camera_driver.h"

namespace arithmetic {
namespace {

constexpr char kNodeName[] = "arithmetic_node";
constexpr char kStartCalcParameter[] = "start_calc";
constexpr char kShowImageParameter[] = "show_image";
constexpr char kPreviewWindowName[] = "Arithmetic Camera Preview";

std::string GetEnvOrDefault(const char *name, const std::string &fallback) {
  const char *value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return fallback;
  }
  return value;
}

std::string BuildDefaultCameraConfigPath() {
  return std::string(ARITHMETIC_SOURCE_DIR) +
         "/arithmetic/camera_driver/camera_init/HIKcamera0.yaml";
}

std::string BuildDefaultModelPath(const char *suffix) {
  return std::string(ARITHMETIC_SOURCE_DIR) + "/../../../models/" + suffix;
}

}  // namespace

ArithmeticNode::ArithmeticNode() : Node(kNodeName) {
  declare_parameter<std::string>(
      "camera_config_path",
      GetEnvOrDefault("ARITHMETIC_CAMERA_CONFIG", BuildDefaultCameraConfigPath()));
  declare_parameter<int>("timeout_ms", 5000);
  declare_parameter<bool>(kShowImageParameter, true);
  declare_parameter<bool>(kStartCalcParameter, false);
  declare_parameter<std::string>(
      "det_model_dir",
      GetEnvOrDefault("ARITHMETIC_DET_MODEL_DIR",
                      BuildDefaultModelPath("PP-OCRv4_mobile_det_infer")));
  declare_parameter<std::string>(
      "rec_model_dir",
      GetEnvOrDefault("ARITHMETIC_REC_MODEL_DIR",
                      BuildDefaultModelPath("PP-OCRv4_mobile_rec_infer")));
  declare_parameter<std::string>(
      "textline_ori_model_dir",
      GetEnvOrDefault("ARITHMETIC_TEXTLINE_ORI_MODEL_DIR",
                      BuildDefaultModelPath("PP-LCNet_x1_0_textline_ori_infer")));
  declare_parameter<std::string>("device", "cpu");
  declare_parameter<bool>("enable_mkldnn", true);
  declare_parameter<int>("cpu_threads", std::max(1u, std::thread::hardware_concurrency()));
  declare_parameter<int>("text_recognition_batch_size", 8);

  vip_box_pub_ = create_publisher<std_msgs::msg::Int32>(
      "vip_box_id", rclcpp::QoS(1).reliable().transient_local());

  ocr_engine_ = std::make_unique<OCREngine>(LoadOcrConfig());

  parameter_callback_handle_ = add_on_set_parameters_callback(
      [this](const std::vector<rclcpp::Parameter> &parameters) {
        return OnParametersChanged(parameters);
      });

  preview_enabled_.store(get_parameter(kShowImageParameter).as_bool());
  acquisition_thread_ = std::thread(&ArithmeticNode::AcquisitionWorker, this);
  calculation_thread_ = std::thread(&ArithmeticNode::CalculationWorker, this);

  RCLCPP_INFO(get_logger(), "arithmetic_node 已启动，实时预览已开启");
}

ArithmeticNode::~ArithmeticNode() {
  worker_running_.store(false);
  cancel_requested_.store(true);
  calc_cv_.notify_all();
  frame_cv_.notify_all();

  if (acquisition_thread_.joinable()) {
    acquisition_thread_.join();
  }
  if (calculation_thread_.joinable()) {
    calculation_thread_.join();
  }

  cv::destroyAllWindows();
}

rcl_interfaces::msg::SetParametersResult ArithmeticNode::OnParametersChanged(
    const std::vector<rclcpp::Parameter> &parameters) {
  rcl_interfaces::msg::SetParametersResult result;
  result.successful = true;

  for (const auto &parameter : parameters) {
    if (parameter.get_name() == kShowImageParameter) {
      if (parameter.get_type() != rclcpp::ParameterType::PARAMETER_BOOL) {
        result.successful = false;
        result.reason = "show_image 必须是 bool 类型";
        return result;
      }
      preview_enabled_.store(parameter.as_bool());
      continue;
    }

    if (parameter.get_name() == kStartCalcParameter) {
      if (parameter.get_type() != rclcpp::ParameterType::PARAMETER_BOOL) {
        result.successful = false;
        result.reason = "start_calc 必须是 bool 类型";
        return result;
      }

      if (parameter.as_bool()) {
        std::lock_guard<std::mutex> lock(calc_mutex_);
        if (!calc_active_.load()) {
          cancel_requested_.store(false);
          calc_requested_ = true;
          calc_cv_.notify_one();
        }
      } else if (calc_active_.load()) {
        cancel_requested_.store(true);
        calc_cv_.notify_all();
        frame_cv_.notify_all();
      }
    }
  }

  return result;
}

ArithmeticNode::RuntimeConfig ArithmeticNode::LoadRuntimeConfig() const {
  RuntimeConfig config;
  config.camera_config_path = get_parameter("camera_config_path").as_string();
  config.timeout_ms = static_cast<int>(get_parameter("timeout_ms").as_int());
  config.show_image = get_parameter(kShowImageParameter).as_bool();
  return config;
}

OCREngine::Config ArithmeticNode::LoadOcrConfig() const {
  OCREngine::Config config;
  config.det_model_dir = get_parameter("det_model_dir").as_string();
  config.rec_model_dir = get_parameter("rec_model_dir").as_string();
  config.textline_ori_model_dir = get_parameter("textline_ori_model_dir").as_string();
  config.device = get_parameter("device").as_string();
  config.enable_mkldnn = get_parameter("enable_mkldnn").as_bool();
  config.cpu_threads = static_cast<int>(get_parameter("cpu_threads").as_int());
  config.text_recognition_batch_size =
      static_cast<int>(get_parameter("text_recognition_batch_size").as_int());
  return config;
}

void ArithmeticNode::AcquisitionWorker() {
  const RuntimeConfig config = LoadRuntimeConfig();
  Camera camera(config.camera_config_path);
  if (!camera.isOpened()) {
    RCLCPP_ERROR(get_logger(), "打开相机失败: %s", config.camera_config_path.c_str());
    return;
  }

  while (worker_running_.load()) {
    cv::Mat frame;
    if (!camera.getFrame(frame) || frame.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(frame_mutex_);
      latest_frame_ = frame.clone();
      ++latest_frame_id_;
    }
    frame_cv_.notify_all();

    if (!preview_enabled_.load()) {
      continue;
    }

    cv::Mat preview = BuildPreviewFrame(frame);
    if (!preview.empty()) {
      cv::imshow(kPreviewWindowName, preview);
      cv::waitKey(1);
    }
  }
}

void ArithmeticNode::CalculationWorker() {
  while (worker_running_.load()) {
    {
      std::unique_lock<std::mutex> lock(calc_mutex_);
      calc_cv_.wait(lock, [this] { return !worker_running_.load() || calc_requested_; });
      if (!worker_running_.load()) {
        break;
      }
      calc_requested_ = false;
      calc_active_.store(true);
    }

    RunCalculationTask();
    calc_active_.store(false);
    cancel_requested_.store(false);
    ClearOverlay();
    ResetStartCalcParameter();
  }
}

void ArithmeticNode::RunCalculationTask() {
  const RuntimeConfig runtime_config = LoadRuntimeConfig();
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(runtime_config.timeout_ms);

  cv::Mat frame;
  std::uint64_t frame_id = 0;
  if (!TryGetCurrentFrame(frame, frame_id)) {
    UpdateOverlay("等待相机首帧", "");
    if (!WaitForNextFrame(0, deadline, frame, frame_id)) {
      if (cancel_requested_.load()) {
        UpdateOverlay("已取消", "");
      } else {
        UpdateOverlay("等待首帧超时", "");
      }
      return;
    }
  }

  while (worker_running_.load()) {
    if (cancel_requested_.load()) {
      UpdateOverlay("已取消", "");
      return;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      UpdateOverlay("识别超时", "");
      return;
    }

    UpdateOverlay("OCR 识别中", "");
    OCREngine::Result ocr_result = ocr_engine_->Recognize(frame);

    if (ocr_result.success) {
      UpdateOverlay("识别成功", ocr_result.expression);
      std_msgs::msg::Int32 message;
      message.data = ocr_result.value;
      vip_box_pub_->publish(message);
      RCLCPP_INFO(get_logger(), "发布结果: %s = %d", ocr_result.expression.c_str(), ocr_result.value);
      return;
    }

    UpdateOverlay(ocr_result.status, "");
    if (!WaitForNextFrame(frame_id, deadline, frame, frame_id)) {
      if (cancel_requested_.load()) {
        UpdateOverlay("已取消", "");
      } else {
        UpdateOverlay("识别超时", "");
      }
      return;
    }
  }
}

bool ArithmeticNode::TryGetCurrentFrame(cv::Mat &frame, std::uint64_t &frame_id) {
  std::lock_guard<std::mutex> lock(frame_mutex_);
  if (latest_frame_.empty()) {
    return false;
  }
  frame = latest_frame_.clone();
  frame_id = latest_frame_id_;
  return true;
}

bool ArithmeticNode::WaitForNextFrame(std::uint64_t last_frame_id,
                                      std::chrono::steady_clock::time_point deadline,
                                      cv::Mat &frame,
                                      std::uint64_t &frame_id) {
  std::unique_lock<std::mutex> lock(frame_mutex_);
  while (worker_running_.load()) {
    if (cancel_requested_.load()) {
      return false;
    }
    if (latest_frame_id_ > last_frame_id && !latest_frame_.empty()) {
      frame = latest_frame_.clone();
      frame_id = latest_frame_id_;
      return true;
    }
    if (frame_cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
      return latest_frame_id_ > last_frame_id && !latest_frame_.empty();
    }
  }
  return false;
}

void ArithmeticNode::UpdateOverlay(const std::string &status,
                                   const std::string &expression) {
  std::lock_guard<std::mutex> lock(overlay_mutex_);
  overlay_status_ = status;
  overlay_expression_ = expression;
}

void ArithmeticNode::ClearOverlay() {
  UpdateOverlay("", "");
}

cv::Mat ArithmeticNode::BuildPreviewFrame(const cv::Mat &frame) {
  cv::Mat output = frame.clone();
  if (output.empty()) {
    return output;
  }

  std::string status;
  std::string expression;
  {
    std::lock_guard<std::mutex> lock(overlay_mutex_);
    status = overlay_status_;
    expression = overlay_expression_;
  }

  if (!calc_active_.load()) {
    return output;
  }

  cv::rectangle(output, cv::Rect(10, 10, std::max(320, output.cols / 2), 80),
                cv::Scalar(0, 0, 0), cv::FILLED);
  cv::putText(output, "status: " + status, cv::Point(20, 40),
              cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);
  cv::putText(output, "expr: " + expression, cv::Point(20, 75),
              cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);
  return output;
}

void ArithmeticNode::ResetStartCalcParameter() {
  try {
    set_parameter(rclcpp::Parameter(kStartCalcParameter, false));
  } catch (const std::exception &exception) {
    RCLCPP_WARN(get_logger(), "重置 start_calc 失败: %s", exception.what());
  }
}

}  // namespace arithmetic
