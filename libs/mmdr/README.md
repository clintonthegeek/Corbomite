# mmdr — Mermaid Diagram Renderer

Pre-built static library from
[mermaid-rs-renderer](https://github.com/1jehuang/mermaid-rs-renderer)
(MIT licence). Renders mermaid text to SVG in pure Rust — no browser,
no Node.js, no external tools.

## Original author

Copyright (c) 2024 1jehuang. Full licence text: [`../../LICENSES/MIT.txt`](../../LICENSES/MIT.txt).

## The tracked artefact

| | |
|---|---|
| Path | `libs/mmdr/libmermaid_rs_renderer.a` |
| Size | 34,857,128 bytes (598 object members) |
| SHA-256 | `8d8eb53d779a533aa2bc3e96030048dfe77d8bc0cc0e0e30dccd689721c741b6` |
| Added in | `e245a1f0` — the only commit that has ever touched this file |
| Consumed by | `libs/mmdr/CMakeLists.txt` as an unconditional `STATIC IMPORTED GLOBAL` |

## ⚠ Provenance is INCOMPLETE — this is not reproducible

**Stated plainly, because the alternative is a README that implies a stranger
could rebuild this and silently fails when they try.**

What is recorded: upstream URL, MIT licence, copyright holder, and a build
command.

What is **missing**, and was **attempted and failed** to recover:

- **Upstream commit.** No commit is pinned anywhere. It cannot be recovered
  from the artefact: `ar t` shows every member named
  `mermaid_rs_renderer-<hash>-cgu.NN.rcgu.o`, and that hash is cargo's
  **crate-metadata** hash, **not** a git SHA. Reading it as a commit would be
  wrong.
- **Toolchain version.** No `rustc/<version>` string is embedded — `strings`
  over the binary yields only Rust std symbol names. The machine that built it
  had no `rustup`, so there was never a mechanism to select or record one.
- **Dependency versions.** No `Cargo.lock` exists anywhere in this repository.
  A rebuild today would resolve a different transitive crate graph, so it would
  almost certainly produce different bytes.
- **Build flags.** Only `cargo build --release --no-default-features` is known,
  inferred from the CMake comment. Target triple, linker arguments, profile
  overrides and feature selection are all unrecorded.

### The documented rebuild path does not work

The commands previously recorded here began `cd ~/dev/mermaidclones/mermaid-rs-renderer`.
**That directory does not exist on any current machine.** The Rust source was
built out-of-tree on a machine where the checkout is now gone, and it is not
vendored, fetched, or otherwise reachable from this repository. There is no
download script, no `Cargo.toml`, no submodule, no `FetchContent`, no CI step,
and no `-DCORBOMITE_MMDR_LIB` escape hatch.

**So: a rebuild is currently impossible, not merely undocumented.**

## Why this blocks a feature

The FFI header (`mmdr_ffi.h`) takes only the diagram source and **no theme
parameter**, so Mermaid rendering cannot follow Corbomite's theme. Fixing that
needs a rebuild with a theme parameter — which is impossible until the source
is obtained and pinned. That is the blocker recorded on the punch list's
Mermaid dark-theme item.

## What would make this reproducible

In rough dependency order:

1. **Obtain and vendor the upstream source** at a pinned commit, with its
   `Cargo.lock`. This is the prerequisite for everything else; without it no
   amount of documentation helps.
2. **Add a `rust-toolchain.toml`** pinning `rustc`/`cargo`. This requires
   installing `rustup`, which is not present.
3. **Build in-tree** — either via Corrosion, or by checking the produced `.a`
   and comparing against the SHA-256 above.
4. **Untrack the blob.** Once (3) works, drop `.a` from git and generate it at
   build time, or make it an optional external dependency that auto-disables
   Mermaid when absent. Option (b) in the release-hygiene plan (a
   `-DCORBOMITE_MMDR_LIB=...` mirror of the existing `qt6keychain` pattern) is
   the cheapest way to satisfy the plan's DoD and would unblock B.2
   immediately.

Decisions (1)–(4), and in particular whether to accept an unreproducible blob
for the 0.1.0/0.2.0 releases already shipped, are **the user's to make** — see
the release-hygiene plan's Phase B and the open punch-list item. Nothing in this
file should be read as having been decided.

## Release note

This file previously contained a five-line "Rebuilding" section with a `cd` into
a directory that does not exist, and no statement that the artefact was
unreproducible. That was worse than useless: it invited someone to try, fail,
and lose time. Corrected 2026-10-02.