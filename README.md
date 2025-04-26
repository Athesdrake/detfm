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

This tool renames variables/classes/... to make it readable for a human being.
Most names are dynamics and can be configured using a config file with `--config <file>` (or `-c`).
You can dump the default config using `--config <file> --dump-config` to the specified file.

## Building from source
Detfm is written in Rust. You'll need to install the [Rust toolchain](https://www.rust-lang.org/tools/install) for development.

To build to project, run:
```sh
cargo build --release --bin detfm
```

By default, `detfm` is built with a vendored lua 5.4, but it can be changed to have a different version of Lua.
```sh
# Use Lua 5.2 from your system (requires to install Lua headers)
cargo build --release --bin detfm --no-default-features --features lua52
# Use a vendored LuaJIT
cargo build --release --bin detfm --no-default-features --features luajit,lua-vendored
```

## Building for the web
Detfm can be compiled to Web Assembly using `wasm-pack`.
More info in [detfm-wasm](./detfm-wasm)

## Docker images
Docker images are provided in the [`docker`](./docker) folder.
There are two different images:
1. `debian-slim` - good compromise between size and features
2. `alpine` - minimal size but has several hacks to make the tool work correctly (and with good performances)

It's also a good example for building steps.
