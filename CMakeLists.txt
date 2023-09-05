cmake_minimum_required(VERSION 3.14)

project(
  detfm
  LANGUAGES CXX
  VERSION "0.3.5")
set(CMAKE_CXX_STANDARD 17)

add_executable(detfm)
add_subdirectory(src)
target_include_directories(
  detfm PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:include>)

# Find dependencies
find_package(fmt CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(CMakeRC CONFIG REQUIRED)
find_package(swflib CONFIG REQUIRED)
find_package(athes_unpack CONFIG REQUIRED)

# Add resources
cmrc_add_resource_library(
  packets-names
  ALIAS rc::packets # cmake name to the resource
  NAMESPACE pktnames # c++ namespace
  WHENCE packets # open without packets/
         packets/clientbound.json packets/tribulle_clientbound.json
         packets/serverbound.json packets/tribulle_serverbound.json)

# Link dependencies
target_link_libraries(
  detfm
  PRIVATE argparse::argparse
          athes::unpack
          fmt::fmt
          nlohmann_json::nlohmann_json
          rc::packets
          Threads::Threads
          yaml-cpp)

if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.4)
  target_link_libraries(detfm PRIVATE stdc++fs)
endif()

target_link_libraries(detfm PRIVATE swflib::swflib)

# Install
install(TARGETS detfm RUNTIME DESTINATION bin COMPONENT bin)
