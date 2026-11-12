cmake_minimum_required(VERSION 3.16)
project(ballcore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

find_package(Threads REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio)
find_package(nlohmann_json REQUIRED)
find_package(httplib REQUIRED)

add_executable(ballcore src/main.cpp src/server/http_server.cpp)
target_include_directories(ballcore PRIVATE src ${OpenCV_INCLUDE_DIRS})
target_link_libraries(ballcore PRIVATE
  ${OpenCV_LIBS} nlohmann_json::nlohmann_json httplib::httplib Threads::Threads)
target_compile_options(ballcore PRIVATE -Wall -Wextra)
target_compile_definitions(ballcore PRIVATE BALLCORE_WEB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/../UI")
