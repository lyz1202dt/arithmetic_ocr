cmake_minimum_required(VERSION 3.16)
project(arithmetic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ament_cmake REQUIRED)
find_package(rcl_interfaces REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(OpenCV REQUIRED)

get_filename_component(ARITHMETIC_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

set(PADDLE_DIR "${ARITHMETIC_PROJECT_ROOT}/../../../third_party/paddle_inference" CACHE PATH "Path to Paddle Inference root")
set(PADDLEOCR_CPP_DIR "${ARITHMETIC_PROJECT_ROOT}/../../../PaddleOCR/deploy/cpp_infer" CACHE PATH "Path to PaddleOCR C++ deploy directory")

set(PADDLE_INCLUDE_DIR "${PADDLE_DIR}/paddle/include")
set(PADDLE_LIB_DIR "${PADDLE_DIR}/paddle/lib")
set(PADDLE_PROTOBUF_INCLUDE_DIR "${PADDLE_DIR}/third_party/install/protobuf/include")
set(PADDLE_GLOG_INCLUDE_DIR "${PADDLE_DIR}/third_party/install/glog/include")
set(PADDLE_GFLAGS_INCLUDE_DIR "${PADDLE_DIR}/third_party/install/gflags/include")
set(PADDLE_XXHASH_INCLUDE_DIR "${PADDLE_DIR}/third_party/install/xxhash/include")
set(PADDLE_ZLIB_INCLUDE_DIR "${PADDLE_DIR}/third_party/install/zlib/include")
set(PADDLE_ONNXRUNTIME_INCLUDE_DIR "${PADDLE_DIR}/third_party/install/onnxruntime/include")
set(PADDLE_P2O_INCLUDE_DIR "${PADDLE_DIR}/third_party/install/paddle2onnx/include")
set(PADDLE_YAML_INCLUDE_DIR "${PADDLE_DIR}/third_party/install/yaml-cpp/include")
set(PADDLE_OPENVINO_INCLUDE_DIR "${PADDLE_DIR}/third_party/install/openvino/include")
set(PADDLE_TBB_INCLUDE_DIR "${PADDLE_DIR}/third_party/install/tbb/include")
set(PADDLE_BOOST_INCLUDE_DIR "${PADDLE_DIR}/third_party/boost")
set(PADDLE_EIGEN_INCLUDE_DIR "${PADDLE_DIR}/third_party/eigen3")

set(PADDLE_PROTOBUF_LIB_DIR "${PADDLE_DIR}/third_party/install/protobuf/lib")
set(PADDLE_GLOG_LIB_DIR "${PADDLE_DIR}/third_party/install/glog/lib")
set(PADDLE_GFLAGS_LIB_DIR "${PADDLE_DIR}/third_party/install/gflags/lib")
set(PADDLE_XXHASH_LIB_DIR "${PADDLE_DIR}/third_party/install/xxhash/lib")
set(PADDLE_ZLIB_LIB_DIR "${PADDLE_DIR}/third_party/install/zlib/lib")
set(PADDLE_ONNXRUNTIME_LIB_DIR "${PADDLE_DIR}/third_party/install/onnxruntime/lib")
set(PADDLE_P2O_LIB_DIR "${PADDLE_DIR}/third_party/install/paddle2onnx/lib")
set(PADDLE_YAML_LIB_DIR "${PADDLE_DIR}/third_party/install/yaml-cpp/lib")
set(PADDLE_OPENVINO_LIB_DIR "${PADDLE_DIR}/third_party/install/openvino/intel64")
set(PADDLE_TBB_LIB_DIR "${PADDLE_DIR}/third_party/install/tbb/lib")
set(PADDLE_ONEDNN_LIB_DIR "${PADDLE_DIR}/third_party/install/onednn/lib")
set(PADDLE_MKL_LIB_DIR "${PADDLE_DIR}/third_party/install/mklml/lib")

set(PADDLE_INFERENCE_LIB "${PADDLE_LIB_DIR}/libpaddle_inference.so")
set(PADDLE_MKLML_LIB "${PADDLE_MKL_LIB_DIR}/libmklml_intel.so")
set(PADDLE_IOMP5_LIB "${PADDLE_MKL_LIB_DIR}/libiomp5.so")
set(PADDLE_DNNL_LIB "${PADDLE_ONEDNN_LIB_DIR}/libdnnl.so.3")

include_directories(
  include
  camera_driver/include
  ${OpenCV_INCLUDE_DIRS}
  "${PADDLE_INCLUDE_DIR}"
  "${PADDLE_PROTOBUF_INCLUDE_DIR}"
  "${PADDLE_GLOG_INCLUDE_DIR}"
  "${PADDLE_GFLAGS_INCLUDE_DIR}"
  "${PADDLE_XXHASH_INCLUDE_DIR}"
  "${PADDLE_ZLIB_INCLUDE_DIR}"
  "${PADDLE_ONNXRUNTIME_INCLUDE_DIR}"
  "${PADDLE_P2O_INCLUDE_DIR}"
  "${PADDLE_YAML_INCLUDE_DIR}"
  "${PADDLE_OPENVINO_INCLUDE_DIR}"
  "${PADDLE_TBB_INCLUDE_DIR}"
  "${PADDLE_BOOST_INCLUDE_DIR}"
  "${PADDLE_EIGEN_INCLUDE_DIR}"
  "${PADDLEOCR_CPP_DIR}"
)

link_directories(
  "${PADDLE_LIB_DIR}"
  "${PADDLE_PROTOBUF_LIB_DIR}"
  "${PADDLE_GLOG_LIB_DIR}"
  "${PADDLE_GFLAGS_LIB_DIR}"
  "${PADDLE_XXHASH_LIB_DIR}"
  "${PADDLE_ZLIB_LIB_DIR}"
  "${PADDLE_ONNXRUNTIME_LIB_DIR}"
  "${PADDLE_P2O_LIB_DIR}"
  "${PADDLE_YAML_LIB_DIR}"
  "${PADDLE_OPENVINO_LIB_DIR}"
  "${PADDLE_TBB_LIB_DIR}"
)

add_subdirectory(camera_driver)
add_subdirectory("${PADDLEOCR_CPP_DIR}/third_party/abseil-cpp" "${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp")
add_subdirectory("${PADDLEOCR_CPP_DIR}/third_party/clipper_ver6.4.2/cpp" "${CMAKE_CURRENT_BINARY_DIR}/clipper")
include_directories(${POLYCLIPPING_INCLUDE_DIR})

file(GLOB_RECURSE OCR_SRC_LIST "${PADDLEOCR_CPP_DIR}/src/*.cc")

add_executable(arithmetic
  src/main.cpp
  src/arithmetic_node.cpp
  src/ocr_engine.cpp
  ${OCR_SRC_LIST}
)

target_compile_definitions(arithmetic PRIVATE ARITHMETIC_SOURCE_DIR="${ARITHMETIC_PROJECT_ROOT}")

ament_target_dependencies(arithmetic
  rcl_interfaces
  rclcpp
  std_msgs
)

target_link_libraries(arithmetic
  camera_driver
  ${OpenCV_LIBS}
  "${PADDLE_INFERENCE_LIB}"
  "${PADDLE_MKLML_LIB}"
  "${PADDLE_IOMP5_LIB}"
  "${PADDLE_DNNL_LIB}"
  glog
  gflags
  protobuf
  z
  xxhash
  yaml-cpp
  absl::statusor
  polyclipping
  pthread
  dl
  rt
  gomp
  m
)

target_link_options(arithmetic PRIVATE "-Wl,--disable-new-dtags")

set_target_properties(arithmetic PROPERTIES
  BUILD_WITH_INSTALL_RPATH TRUE
  BUILD_RPATH "${PADDLE_LIB_DIR};${PADDLE_PROTOBUF_LIB_DIR};${PADDLE_GLOG_LIB_DIR};${PADDLE_GFLAGS_LIB_DIR};${PADDLE_XXHASH_LIB_DIR};${PADDLE_ZLIB_LIB_DIR};${PADDLE_YAML_LIB_DIR};${PADDLE_OPENVINO_LIB_DIR};${PADDLE_TBB_LIB_DIR};${PADDLE_ONEDNN_LIB_DIR};${PADDLE_MKL_LIB_DIR};${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/status;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/strings;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/profiling;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/synchronization;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/time;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/types;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/debugging;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/base;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/numeric;${CMAKE_CURRENT_BINARY_DIR}/clipper;${CMAKE_CURRENT_SOURCE_DIR}/camera_driver/lib"
  INSTALL_RPATH "${PADDLE_LIB_DIR};${PADDLE_PROTOBUF_LIB_DIR};${PADDLE_GLOG_LIB_DIR};${PADDLE_GFLAGS_LIB_DIR};${PADDLE_XXHASH_LIB_DIR};${PADDLE_ZLIB_LIB_DIR};${PADDLE_YAML_LIB_DIR};${PADDLE_OPENVINO_LIB_DIR};${PADDLE_TBB_LIB_DIR};${PADDLE_ONEDNN_LIB_DIR};${PADDLE_MKL_LIB_DIR};${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/status;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/strings;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/profiling;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/synchronization;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/time;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/types;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/debugging;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/base;${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp/absl/numeric;${CMAKE_CURRENT_BINARY_DIR}/clipper;$ORIGIN/..;${CMAKE_INSTALL_PREFIX}/lib;${CMAKE_CURRENT_SOURCE_DIR}/camera_driver/lib"
)

install(TARGETS arithmetic DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY camera_driver/camera_init DESTINATION share/${PROJECT_NAME}/camera_driver)
install(DIRECTORY include/ DESTINATION include)
install(FILES package.xml DESTINATION share/${PROJECT_NAME})

ament_package()
