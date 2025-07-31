cmake_minimum_required(VERSION 3.10)
project(main_server_matcher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)

add_executable(bus_butt
    main.cpp

    bus_station_manager/bus_station_manager.cpp
    stop_status_fetcher/stop_status_fetcher.cpp
    bus_queue_fetcher/bus_queue_fetcher.cpp
    display_writer/display_writer.cpp
)

target_include_directories(bus_butt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stop_status_fetcher
    ${CMAKE_CURRENT_SOURCE_DIR}/bus_station_manager
    ${CMAKE_CURRENT_SOURCE_DIR}/bus_queue_fetcher
    ${CMAKE_CURRENT_SOURCE_DIR}/display_writer
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(bus_butt PRIVATE
    Threads::Threads
    ${CURL_LIBRARIES}
)