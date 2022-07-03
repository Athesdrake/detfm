cmake_minimum_required(VERSION 3.14)
project(detfm CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(detfm "")
add_subdirectory(src)
target_include_directories(
  detfm PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:include>)

# Find dependencies
find_package(fmt CONFIG REQUIRED)
# find_package(argparse CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(swflib CONFIG REQUIRED)
find_package(Threads REQUIRED)
# Packages required by swflib-static find_package(ZLIB REQUIRED)
# find_package(LibLZMA REQUIRED)

include(FetchContent)
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG v2.4)

FetchContent_MakeAvailable(argparse)

# Link dependencies
target_link_libraries(detfm PRIVATE argparse::argparse)
target_link_libraries(detfm PRIVATE fmt::fmt)
target_link_libraries(detfm PRIVATE yaml-cpp)
target_link_libraries(detfm PRIVATE swflib::swflib)
target_link_libraries(detfm PRIVATE Threads::Threads)
