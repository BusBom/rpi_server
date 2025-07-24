cmake_minimum_required(VERSION 3.10)
project(onvif_streamer VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(/usr/include)
link_directories(/usr/lib/x86_64-linux-gnu)


# FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
    libswscale
)

set(TFLITE_INCLUDE_DIR "/home/cty0613/tensorflow_src/bazel-tensorflow_src")         # 예: 빌드된 TFLite의 헤더 경로
set(TFLITE_STATIC_LIB "/home/cty0613/tensorflow_src/bazel-bin/tensorflow/lite/libtensorflowlite.so")  # 정적 라이브러리 경로
message(STATUS "Found TF-LITE Includes:" ${TFLITE_INCLUDE_DIR})
message(STATUS "Found TF-LITE Libs:" ${TFLITE_STATIC_LIB})

include_directories(${TFLITE_INCLUDE_DIR})

# TinyXML2
# set(CMAKE_PREFIX_PATH "/usr/lib/x86_64-linux-gnu/cmake/tinyxml2")
# find_package(tinyxml2 REQUIRED)

# OpenCV
find_package(OpenCV REQUIRED)

add_executable(onvif_streamer main.cpp ocr.cpp video.cpp metadata.cpp)

# include paths
target_include_directories(onvif_streamer PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
    ${TFLITE_INCLUDE_DIR}
)


# link libraries
target_link_libraries(onvif_streamer PRIVATE 
    PkgConfig::LIBAV
    tinyxml2
    ${OpenCV_LIBS}
    ${TFLITE_STATIC_LIB}
    rt  # For shared memory functions
    pthread # For std::thread
)


# macOS specific settings
if(APPLE)
    target_link_libraries(onvif_streamer PRIVATE "-framework CoreFoundation" "-framework VideoToolbox" "-framework CoreVideo" "-framework CoreMedia")
endif()
