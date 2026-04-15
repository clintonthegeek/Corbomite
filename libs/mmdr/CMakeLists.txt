cmake_minimum_required(VERSION 3.19)

# Pre-built Rust static library for mermaid diagram rendering
# Source: https://github.com/1jehuang/mermaid-rs-renderer (MIT license)
# Build with: cd ~/dev/mermaidclones/mermaid-rs-renderer && cargo build --release --no-default-features

add_library(mmdr STATIC IMPORTED GLOBAL)
set_target_properties(mmdr PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libmermaid_rs_renderer.a"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Rust static libraries need these system libraries on Linux
target_link_libraries(mmdr INTERFACE pthread dl m)
