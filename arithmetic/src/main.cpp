#include <memory>

#include <rclcpp/rclcpp.hpp>

#include "arithmetic/arithmetic_node.hpp"

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<arithmetic::ArithmeticNode>());
  rclcpp::shutdown();
  return 0;
}
