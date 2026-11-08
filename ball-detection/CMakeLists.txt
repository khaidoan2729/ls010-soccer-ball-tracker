cmake_minimum_required(VERSION 3.10)
project(ball_detection)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(depthai CONFIG REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(ball_detection src/main.cpp)
target_include_directories(ball_detection PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(ball_detection PRIVATE depthai::opencv ${OpenCV_LIBS})
