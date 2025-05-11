cmake_minimum_required(VERSION 3.10)
project(MotionDetectorPI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# Знаходимо Mosquitto
find_path(MOSQ_INCLUDE_DIR mosquitto.h PATHS /usr/include)
find_library(MOSQ_LIB mosquitto PATHS /usr/lib/aarch64-linux-gnu /usr/lib)
include_directories(${MOSQ_INCLUDE_DIR})

add_executable(motion_detector
    src/Scanner.cpp
    src/main.cpp
)

target_link_libraries(motion_detector PRIVATE ${MOSQ_LIB} Threads::Threads)