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
find_package(Threads REQUIRED)

option(SET_RPATH "Set runtime path to the ${CMAKE_INSTALL_PREFIX}/lib." ON)
option(SWFLIB_STATIC "Link swflib statically instead of dynamic linking." ON)
if(SWFLIB_STATIC)
  find_package(ZLIB REQUIRED)
  find_package(LibLZMA REQUIRED)
endif()
find_package(swflib CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG v2.4)

FetchContent_GetProperties(argparse)
if(NOT argparse_POPULATED)
  FetchContent_Populate(argparse)
  add_subdirectory(${argparse_SOURCE_DIR} ${argparse_BINARY_DIR}
                   EXCLUDE_FROM_ALL)
endif()

# Link dependencies
target_link_libraries(detfm PRIVATE argparse::argparse)
target_link_libraries(detfm PRIVATE fmt::fmt)
target_link_libraries(detfm PRIVATE yaml-cpp)
target_link_libraries(detfm PRIVATE Threads::Threads)

if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.4)
  target_link_libraries(detfm PRIVATE stdc++fs)
endif()

if(SWFLIB_STATIC)
  target_link_libraries(detfm PRIVATE swflib::swflib-static)
else()
  target_link_libraries(detfm PRIVATE swflib::swflib)
  if(SET_RPATH)
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
  endif()
endif()

# Install
install(TARGETS detfm RUNTIME DESTINATION bin COMPONENT bin)
