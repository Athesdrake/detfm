# detfm
A Transformice tool that helps you deobfuscate it.

## Usage
The most basic usage, which will download the file:
```sh
detfm Transformice-clean.swf
```

You can also use a file from disk:
```sh
detfm -i Transformice.swf Transformice-clean.swf
```

It's also possible to use stdin as the input and stdout as the output by defining the input and output to `-` respectively.

```sh
# Using stdin as the input:
detfm -i - Transformice-clean.swf
# Using stdout as the output:
detfm -i Transformice.swf -
```

This tool renames variables/classes/... to make it readable for a human being.
Most names are dynamics and can be configured using a config file with `--config <file>` (or `-c`).
You can dump the default config using `--dump-config <file>` to the specified file.

By default, this utility uses multiple threads in order to speed up the process. You can specify the number of threads to use the `-j` or `--jobs` argument.
A value of 0 will use the appropriate number of threads available and a value of 1 will disable the multithreading and use a sequential approach instead.

## User-defined class definitions (DEPRECATED)
You can define your own rules that matches a certain class using YAML files. You can find examples in the folder [`classdef`](./classdef/).
To enable this feature, you need to provide the tool the path to these files using the option `--classdef`.
```sh
detfm --classdef ./classdef -i Transformice.swf Transformice-clean.swf
```
However, this is deprecated, and will be replaced in the future with a better alternative.

## Building from source
Few libraries are needed in order to this project to compile.
 - [`argparse`](https://github.com/p-ranav/argparse) - Command-line parser
 - [`cmakerc`](https://github.com/vector-of-bool/cmrc) - Bundle files into the executable
 - [`fmt`](https://github.com/fmtlib/fmt) - Python-like formatter
 - [`nlohmann`-json](https://github.com/nlohmann/json) - JSON parser
 - [`yaml`-cpp](https://github.com/jbeder/yaml-cpp) - YAML parser
 - [`unpacker`](https://github.com/Athesdrake/unpacker) - Unpack Transformice SWF file
 - [`swflib`](https://github.com/Athesdrake/swflib) - SWF parser

The dependencies will be installed automatically using [`vcpkg`](https://vcpkg.io/en/index.html)

However, it's required to build `unpacker` and `swflib` beforehands.

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
