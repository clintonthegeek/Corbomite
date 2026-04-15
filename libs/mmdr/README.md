# mmdr -- Mermaid Diagram Renderer

Pre-built static library from [mermaid-rs-renderer](https://github.com/1jehuang/mermaid-rs-renderer) (MIT license).

## Original Author

Copyright (c) 2024 1jehuang

## Rebuilding

```bash
cd ~/dev/mermaidclones/mermaid-rs-renderer
cargo build --release --no-default-features
cp target/release/libmermaid_rs_renderer.a /path/to/Corbomite/libs/mmdr/
```

## What This Is

A pure Rust mermaid diagram renderer compiled as a C-callable static library.
Renders mermaid text to SVG. No browser, Node.js, or external tools required.
