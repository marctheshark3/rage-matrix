# matrix-sim (Rust host twin)

ESP8266 firmware stays C++ (Xtensa + HTTP OTA). This crate is the algorithm
workbench: tank + war, no heap in `step`, integer DDA LOS.

```
export PATH="$HOME/.rustup/toolchains/1.95.0-aarch64-unknown-linux-gnu/bin:$PATH"
cargo test --release
cargo run --release -- bench 80000
cargo run --release -- war --steps 400 --json
```

Binary: `target/release/matrix-sim`

Commands: `bench [N]` · `war` · `tank` · `serve` (stdin: war|tank|step|seed|json|px)
