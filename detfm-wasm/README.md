# detfm for the web

## Build instructions

Requires [`wasm-pack`](https://rustwasm.github.io/docs/wasm-pack/) to compile to Web Assembly.
It also requires to install `wasm32-unknown-unknown`. Steps can be found in the [documentation](https://rustwasm.github.io/docs/wasm-pack/prerequisites/non-rustup-setups.html).
```sh
cargo install wasm-pack
```

Then, to compile:
```sh
cd detfm-wasm
wasm-pack build
```

The result `wasm` file can be found in the `pkg` folder, alongside the `.js` binding.
