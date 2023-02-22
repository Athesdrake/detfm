# detfm
A Transformice tool that helps you deobfuscate it.

## Usage
The most basic usage:
```sh
detfm Transformice.swf Transformice-clean.swf
```

It also possible to use stdin as the input and stdout as the output by defining the input and output to `-` respectively.

Using stdin as the input:
```sh
detfm - Transformice-clean.swf
```

Using stdout as the output:
```sh
detfm Transformice.swf -
```

By default, this utility uses multiple threads in order to speed up the process. You can specify the number of threads to use using the `-j` or `--jobs` argument.
A value of 0 will use the appropriate number of threads available and a value of 1 will disable the multithreading and use a sequencial approach instead.

## User-defined class definitions
You can define your own rules that matches a certain class using YAML files. You can find examples in the folder [`classdef`](./classdef/).
To enable this feature, you need to provide the tool the path to these files using the option `--clasdef`.
```sh
detfm --classdef ./classdef Transformice.swf Transformice-clean.swf
```
Note that this feature is experimental and might break in the future.

## Building from source
Few libraries are needed in order to this project to compile.
 - [argparse](https://github.com/p-ranav/argparse)
 - [fmt](https://github.com/fmtlib/fmt)
 - [yaml-cpp](https://github.com/jbeder/yaml-cpp)
 - [swflib](https://github.com/Athesdrake/swflib)

You can install most `fmt` and `yaml-cpp` using [`vcpkg`](https://vcpkg.io/en/index.html).
`argparse` is downloaded using CMake's `FetchContent` utility.

As for `swflib`, you'll need to get it from [here](https://github.com/Athesdrake/swflib).

Install the dependencies and configure the CMake project. You'll need to provide the toolchain file to cmake in order to find the libraries.
```sh
mkdir build
cd build
cmake .. "-DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake"
```
You can also provide additional directives, such as `-Dswflib_DIR=[path to swflib]` to help cmake finding swflib or `-DCMAKE_BUILD_TYPE=Release` to build in release mode.

Afterward, you'll be able to build the project:
```sh
cmake --build .
```

## Docker images
Docker images are provided in the [`docker`](./docker) folder.
There are two different images:
1. `debian-slim` - good compromise between size and features
2. `alpine` - minimal size but has several hacks to make the tool work correctly (and with good performances)

It's also a good example for building steps.
