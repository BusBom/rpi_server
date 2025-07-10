cmake_minimum_required(VERSION 3.10)
project(yfv2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ncnn 패키지 찾기
set(ncnn_DIR "/home/cty0613/ncnn/build/install/lib/cmake/ncnn")
find_package(ncnn REQUIRED)

# OpenCV 찾기 (pkg-config 활용)
find_package(PkgConfig REQUIRED)
pkg_check_modules(OpenCV REQUIRED opencv4)
include_directories(${OpenCV_INCLUDE_DIRS})
# include_directories(/usr/include/tesseract)

# Threads
find_package(Threads REQUIRED)

# --- TensorFlow Lite 직접 include 및 static lib 지정 ---

# include 및 링크 설정
include_directories(${TFLITE_INCLUDE_DIR})
include_directories(src/include)

add_executable(yfv2 demo.cpp src/yolo-fastestv2.cpp)

# 라이브러리 링크
target_link_libraries(yfv2
    PRIVATE
    ${OpenCV_LIBRARIES}  # OpenCV 라이브러리
    ncnn                # ncnn 라이브러리
    Threads::Threads    # CMake Threads 방식
    rt
    dl
    m
)
