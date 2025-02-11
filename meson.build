project(
    'detfm',
    'cpp',
    default_options: ['cpp_std=c++17'],
    version: '0.5.1',
)

swflib = dependency('swflib')
unpacker = dependency('unpack')
argparse = dependency('argparse')
fmt = dependency('fmt')
json = dependency('nlohmann_json')

yaml_dep = dependency('yaml-cpp', version: '>= 0.8.0', required: false)
if not yaml_dep.found()
    # when yaml-cpp is not found, compile from source using cmake
    cmake = import('cmake')
    yaml_cpp_defines = {
        'CMAKE_BUILD_TYPE': 'Release',
        'CMAKE_POSITION_INDEPENDENT_CODE': 'ON',
        'YAML_CPP_BUILD_TOOLS': 'OFF',
        'YAML_CPP_BUILD_TESTS': 'OFF',
        'YAML_CPP_BUILD_CONTRIB': 'OFF',
        'BUILD_SHARED_LIBS': 'OFF',
    }
    options = cmake.subproject_options()
    options.add_cmake_defines(yaml_cpp_defines)
    yaml_dep = cmake.subproject('yaml-cpp', options: options).dependency('yaml-cpp')
endif

prog_python = find_program('python3')
packets_hpp = custom_target(
    'packets.hpp',
    output: 'packets.hpp',
    input: [
        'packets/compile_packets.py',
        'packets/clientbound.json',
        'packets/serverbound.json',
        'packets/tribulle_clientbound.json',
        'packets/tribulle_serverbound.json',
    ],
    command: [prog_python, '@INPUT@', '-o', '@OUTPUT@'],
)

incdir = include_directories('include')
sources = []
subdir('src')
executable(
    'detfm',
    sources,
    packets_hpp,
    include_directories: incdir,
    dependencies: [swflib, unpacker, argparse, fmt, json, yaml_dep],
)