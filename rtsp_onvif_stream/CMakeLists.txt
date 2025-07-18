cmake_minimum_required(VERSION 3.10)
project(rtsp_onvif_ch VERSION 1.0)

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

# TinyXML2
# set(CMAKE_PREFIX_PATH "/usr/lib/x86_64-linux-gnu/cmake/tinyxml2")
# find_package(tinyxml2 REQUIRED)

# OpenCV
find_package(OpenCV REQUIRED)

add_executable(rtsp_onvif_ch hanwha_streamer.cpp metadata_parser.cpp video_processor.cpp main.cpp)

# include paths
target_include_directories(rtsp_onvif_ch PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)


# link libraries
target_link_libraries(rtsp_onvif_ch PRIVATE 
    PkgConfig::LIBAV
    tinyxml2
    ${OpenCV_LIBS}
    rt  # For shared memory functions
)


# macOS specific settings
if(APPLE)
    target_link_libraries(rtsp_onvif_ch PRIVATE "-framework CoreFoundation" "-framework VideoToolbox" "-framework CoreVideo" "-framework CoreMedia")
endif()
