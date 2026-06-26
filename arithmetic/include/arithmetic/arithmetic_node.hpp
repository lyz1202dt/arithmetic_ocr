#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/core.hpp>

#include <rcl_interfaces/msg/set_parameters_result.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int32.hpp>

#include "arithmetic/ocr_engine.hpp"

namespace arithmetic {

class ArithmeticNode : public rclcpp::Node {
public:
  ArithmeticNode();
  ~ArithmeticNode() override;

private:
  struct RuntimeConfig {
    std::string camera_config_path;
    int timeout_ms{5000};
    bool show_image{true};
  };

  rcl_interfaces::msg::SetParametersResult OnParametersChanged(
      const std::vector<rclcpp::Parameter> &parameters);
  RuntimeConfig LoadRuntimeConfig() const;
  OCREngine::Config LoadOcrConfig() const;

  void AcquisitionWorker();
  void CalculationWorker();
  void RunCalculationTask();

  bool TryGetCurrentFrame(cv::Mat &frame, std::uint64_t &frame_id);
  bool WaitForNextFrame(std::uint64_t last_frame_id,
                        std::chrono::steady_clock::time_point deadline,
                        cv::Mat &frame,
                        std::uint64_t &frame_id);

  void UpdateOverlay(const std::string &status, const std::string &expression);
  void ClearOverlay();
  cv::Mat BuildPreviewFrame(const cv::Mat &frame);
  void ResetStartCalcParameter();

  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr vip_box_pub_;
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr
      parameter_callback_handle_;

  std::unique_ptr<OCREngine> ocr_engine_;

  std::atomic_bool worker_running_{true};
  std::atomic_bool preview_enabled_{true};
  std::atomic_bool calc_active_{false};
  std::atomic_bool cancel_requested_{false};

  std::mutex calc_mutex_;
  std::condition_variable calc_cv_;
  bool calc_requested_{false};

  std::mutex frame_mutex_;
  std::condition_variable frame_cv_;
  cv::Mat latest_frame_;
  std::uint64_t latest_frame_id_{0};

  std::mutex overlay_mutex_;
  std::string overlay_status_;
  std::string overlay_expression_;

  std::thread acquisition_thread_;
  std::thread calculation_thread_;
};

}  // namespace arithmetic
