cmake_minimum_required(VERSION 3.10)
project(MotionDetectorPI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# --- Mosquitto
find_path(MOSQ_INCLUDE_DIR mosquitto.h PATHS /usr/include)
find_library(MOSQ_LIB mosquitto PATHS /usr/lib/aarch64-linux-gnu /usr/lib)

# --- SQLite3
find_package(SQLite3 REQUIRED)

add_executable(motion_detector
    src/Scanner.cpp
    src/main.cpp
    src/Logger.cpp
    src/SQLiteDB.cpp
 "src/MotionDetector.cpp" "src/Measurement.h")

# include directories for Mosquitto, SQLite3 and our headers
target_include_directories(motion_detector PRIVATE
    ${MOSQ_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

# link libraries: Mosquitto, Threads and SQLite3
target_link_libraries(motion_detector PRIVATE
    ${MOSQ_LIB}
    Threads::Threads
    ${SQLite3_LIBRARIES}
)
