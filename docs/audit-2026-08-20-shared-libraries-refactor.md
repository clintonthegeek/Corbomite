# Audit — STATIC → SHARED internal library refactor

**Date:** 2026-08-20. **Type:** Research report (not a plan — see stub cluster
below for dispatch). **Author context:** produced during a Cluster N
post-merge session after the user noticed `build-dev/` directories
ballooning to 10+ GB per worktree and asked for root cause; investigation
surfaced a second, more important finding along the way.

**Bottom line up front:** this is not just a disk-space cleanup. `libs/vault`
was *already* flipped from `STATIC` to `SHARED` in Cluster Q (2026-04-17),
specifically because plugins `.so`-loaded at runtime got their own private
copy of vault's RTTI/`QMetaObject`s when it was static, silently breaking
`qobject_cast<Plugin *>` across the host/plugin boundary. **Every other
internal library — `corbomite-core`, `corbomite-storage`, `corbomite-models`,
`corbomite-bases`, `canvas`, `corbomite-search`, `forcegraph`, and every
markoff-family/graffodil library — has the identical latent bug today,
just not yet triggered**, because current in-tree plugins happen to route
their host interaction through the one library that's already fixed
(`PluginContext` lives in `libs/vault`) and otherwise only cast their own
plugin-local types. Any plugin — in-tree or third-party — that ever
`qobject_cast`s a `Corbomite::Core`/`Storage`/etc. type the host handed it
will hit this and get a silent `nullptr`, not an error.

---

## 1. Problem statement

Two independent but related problems, both traced to the same root cause
(pervasive `STATIC` linking of internal libraries):

1. **Disk usage.** `build-dev/` runs 10–11 GB per git worktree. This
   project routinely works across 2-4 worktrees at once (main + per-cluster
   isolation branches, per the project's own worktree convention — see
   `docs/superpowers/plans/2026-08-20-cluster-n-rich-clipboard.md`'s
   worktree setup for a recent example). Three simultaneous worktrees
   during this session's Cluster N/O integration work pushed the machine's
   `/home` partition to 99–100% full and caused real link failures ("No
   space left on device").
2. **Plugin-boundary type-identity correctness.** Described above. This is
   the more important finding — it's a design defect independent of disk
   space, already proven to bite in this exact codebase (vault's Cluster Q
   fix), and currently latent (not yet triggered) in every other internal
   library plugins link against.

## 2. Evidence — disk usage breakdown

Measured against a fresh full build (`build-dev/`, `CORBOMITE_DEV_BUILD=ON`,
`CMAKE_BUILD_TYPE=Debug`, `CORBOMITE_PORT_BUILD_TESTS=ON`) on 2026-08-20:

| | Size |
|---|---|
| `build-dev/` total | 11 GB |
| `build-dev/bin/` (332 executables: 1 app + ~320 tests + a couple of plugin/demo binaries) | 8.9 GB |
| Average per test binary | 27 MB |
| **33 app-level test binaries** (`tst_view_*`, `tst_note_editor_widget_*`, `tst_completion_controller`, `tst_mainwindow_*`, `tst_action_context`, etc. — anything that transitively links `CorbomiteApp`) | **131–147 MB each, 8.6 GB combined** |
| `build-dev/libs/` (object files) | 693 MB |
| `build-dev/lib/` (the `.a` archives themselves) | 613 MB |

Largest static archives (`build-dev/lib/*.a`):

| Library | Size | `add_library` site |
|---|---|---|
| `corbomite-bases` | 51 MB | `libs/bases/CMakeLists.txt:10` |
| `CorbomiteApp` | 47 MB | `src/CMakeLists.txt:11` |
| `corbomite-core` | 45 MB | `libs/core/CMakeLists.txt:15` |
| `jkqtmathtext` | 41 MB | `libs/jkqtmathtext/CMakeLists.txt:60` |
| `markoff_core` | 35 MB | `libs/markoff-family/libs/markoff-core/CMakeLists.txt:14` |
| `collabtext` | 33 MB | `libs/markoff-family/libs/collabtext/libs/collabtext/CMakeLists.txt:25` |
| `corbomite-storage` | 21 MB | `libs/storage/CMakeLists.txt:8` |
| `canvas` | 20 MB | `libs/canvas/CMakeLists.txt:10` |
| `markoff_canvas` | 19 MB | `libs/markoff-family/libs/markoff-canvas/CMakeLists.txt:22` |
| `graffodil-core` | 15 MB | `libs/graffodil/src/core/CMakeLists.txt:2` |

The 33 huge test binaries are large because each independently embeds a
full private copy of every static library it transitively links —
`CorbomiteApp` + `corbomite-core` + `markoff_core` alone is ~127 MB of code
duplicated once per binary, ~50-70 times over.

**Compounding factors** (verified, not present): no `ccache`/`sccache`
installed anywhere on the machine (`which ccache sccache` → both not
found), so every worktree's `build-dev/` rebuilds and re-inflates from
zero rather than sharing compiled object code. Debug build type (no
optimization, full unstripped DWARF symbols) multiplies every one of the
duplicated copies further — this is a correct choice for a dev build
(gdb-testability) and shouldn't change; it just means the duplication cost
per copy is higher than it would be in Release.

## 3. Evidence — the plugin type-identity bug (already proven, already fixed once)

`docs/decisions-archive.md`, **2026-04-17 — Cluster Q closed**:

> "libvault flipped from STATIC to SHARED so `qobject_cast<Plugin *>` works
> across host/.so boundaries"

Confirmed independently by reading the current code:

- **Plugins are real `dlopen`-loaded modules.** `cmake/CorbomitePlugin.cmake`,
  `corbomite_add_plugin()`, line 58: `add_library(${TARGET} MODULE ${ARG_SOURCES})`.
  Every built-in plugin under `src/plugins/*` is built this way (confirmed
  via `src/plugins/backlinks/CMakeLists.txt` and seven siblings).
- **Every plugin links `Corbomite::Core` directly.** Same file, `LINK_LIBRARIES`
  is passed straight to `target_link_libraries(${TARGET} PRIVATE ... ${ARG_LINK_LIBRARIES})`
  (line 74-76); `src/plugins/backlinks/CMakeLists.txt` passes
  `Corbomite::Core Corbomite::Storage Corbomite::Vault`. `corbomite-core` and
  `corbomite-storage` are both still `STATIC` — so every plugin's `.so`
  today embeds its own private copy of both.
- **Why it hasn't bitten yet:** the host↔plugin interface class,
  `PluginContext`, lives in `libs/vault/include/corbomite/vault/PluginContext.h`
  — i.e. in the one library that's already `SHARED`. The two plugins spot-
  checked (`search`, `backlinks`) only `qobject_cast` to their *own*
  plugin-local view classes (`SearchView`, `BacklinksView` — compiled once,
  inside that same `.so`, so no cross-boundary identity mismatch is
  possible). No current in-tree plugin was found casting a raw
  `Corbomite::Core`/`Storage`/`Models`/`Bases` type handed to it by the
  host. That's a property of what plugins *currently happen to do*, not a
  structural guarantee — the moment one does, it breaks silently (a failed
  `qobject_cast` returns `nullptr`, no exception, no log line, easy to
  misdiagnose as "the object doesn't exist" rather than "two copies of the
  same class exist and don't agree they're the same type").
- **Singleton/registry state forks the same way.** Any library-level
  singleton (e.g. registry patterns — `markoff-core` has at least one,
  `BuiltinBlockSerializerRegistry::instance()`) would independently
  initialize once per static-linked copy. A plugin registering into "the"
  registry and the host reading from "the" registry would silently be
  talking to two different objects if that registry's library is ever
  statically duplicated across the boundary.

## 4. Evidence — feasibility (better than expected; low mechanical cost)

- **No export-macro work needed.** `grep -rn "fvisibility\|CXX_VISIBILITY_PRESET"`
  across `CMakeLists.txt` + every `libs/*/CMakeLists.txt` → zero hits. The
  project never sets `-fvisibility=hidden`, so GCC/Clang's default
  (`-fvisibility=default`, everything exported) already applies on this
  project's actual target platform (Linux/KDE — no Windows/macOS build is
  attempted anywhere in the tree). Confirmed empirically: `libs/vault`'s
  headers (e.g. `Vault.h`) have **no** `VAULT_EXPORT`-style macro anywhere
  — it was flipped to `SHARED` with zero header annotation. The same
  should hold for every other library. (Caveat: if Windows support is ever
  wanted, this changes completely — MSVC requires explicit
  `__declspec(dllexport/dllimport)`, no default-export equivalent. Not a
  concern given the project's current scope.)
- **RPATH already proven working, zero configuration.** `ldd
  build-dev/bin/tst_backlinks_plugin` already resolves
  `libvault.so => /home/.../build-dev/bin/libvault.so` correctly today,
  with no explicit `RPATH`/`BUILD_RPATH`/`INSTALL_RPATH` lines anywhere in
  the tree — CMake's default same-output-directory behavior already
  handles it for the one shared lib that exists. Should scale to N
  libraries the same way (all land in `build-dev/lib`/`build-dev/bin`
  together).
- **Packaging is already generic, not per-library special-cased.**
  `packaging/arch/PKGBUILD` has zero mentions of `libvault` or any
  Corbomite-internal `.so` by name — it rides CMake's standard
  `install(TARGETS ... LIBRARY DESTINATION ...)` rules (see
  `libs/vault/CMakeLists.txt:93-96` for the exact pattern to replicate per
  library). `packaging/ubuntu/build-deb.sh`'s only special-casing is for
  the *external* `kddockwidgets` system dependency, not for anything
  Corbomite builds itself. The AppImage pipeline
  (`packaging/appimage/build-appimage.sh`) uses `linuxdeploy`, which
  auto-discovers and bundles a binary's shared-library dependencies via
  its own dependency-walking (already confirmed bundling `libvault.so`
  plus a dozen third-party `.so`s the same way — `ryml`, `c4core`, `fmt`,
  `spdlog`, etc., all present under `packaging/appimage/AppDir/usr/lib/`).
  More internal `.so`s means linuxdeploy bundles more files automatically;
  no new packaging logic anticipated, but **not independently verified by
  an actual packaging build in this session** — flag as a real
  verification step for whoever plans this, not an assumed-safe skip.

## 5. Cross-repo scope — the one thing a planning session could miss

**Every internal library across all three repos this project pulls in is
`STATIC`, not just Corbomite's own `libs/*`:**

| Repo | Libraries confirmed `STATIC` |
|---|---|
| Corbomite (this repo) | `corbomite-core`, `corbomite-storage`, `corbomite-models`, `corbomite-bases`, `canvas`, `corbomite-search`, `forcegraph`, `CorbomiteApp`, `jkqtmathtext` (vendored) |
| `libs/markoff-family` (submodule) | `markoff_core`, `markoff_canvas`, `markoff_source`, `markoff_styled`, `markoff-parser`, `ts-markdown-parser`, `collabtext` |
| `libs/graffodil` (submodule) | `graffodil-core`, `graffodil-batch`, `graffodil-circular`, `graffodil-force`, `graffodil-spatial`, `graffodil-sugiyama` |
| `libs/mmdr` | Special case: `add_library(mmdr STATIC IMPORTED GLOBAL)` — a **prebuilt Rust staticlib** (`libmermaid_rs_renderer.a`, built via Cargo, not CMake). Converting this to shared means changing the Rust crate's `crate-type` to `cdylib` and rebuilding it — a different toolchain, out of scope for a pure-CMake pass, and small (not among the large contributors above). |

`libs/vault` is the sole exception (`SHARED` since Cluster Q) — confirmed
correct, no action needed.

**This means the biggest disk-usage contributors — the 33 huge test
binaries — link `CorbomiteApp` + `corbomite-core` + `markoff_core`
together.** Converting only Corbomite's own libraries and leaving
`markoff_core`/`markoff_canvas`/etc. `STATIC` would capture *some* of the
disk win but **not the full amount**, since those test binaries would
still embed a private copy of the markoff-family code. Getting the full
benefit requires the same `STATIC` → `SHARED` change inside the
`markoff-family` submodule (and, to a lesser extent since it's smaller,
`graffodil`) — which means **editing a different repository**, subject to
that repo's own conventions and (per this project's established
cross-repo discipline — see `decisions-archive.md`'s 2026-05-25 foundation
port entry, "Markoff-first ordering satisfied") probably landing the
Markoff-side change and re-pinning the submodule, not editing the
submodule in place from Corbomite.

**Scope decision for the planning session:** Corbomite-only (smaller, one
repo, partial disk win, closes the correctness gap for Corbomite's own
libraries) vs. full cross-repo (bigger win, touches Markoff and Graffodil
too, needs their own CLAUDE.md conventions respected and their own
maintainer hat — even though it's the same user, treat it as a distinct
repo with its own build/test/commit discipline per those repos'
CLAUDE.md files).

## 6. Tradeoffs (full detail — condensed version already given to the user in chat)

**Benefits:**
- Disk: the dominant lever, bigger than installing ccache (though both are
  worth doing — they compound, not compete).
- Correctness: closes the `qobject_cast`/RTTI-duplication risk across the
  whole plugin surface, matching the fix already validated for vault.
- Incremental build speed: today, touching any header in a static lib
  forces every consumer test binary to fully relink; as a shared lib, only
  the library itself relinks. Should measurably shorten the edit-build-test
  loop, independent of the disk win. Not benchmarked in this session —
  worth a before/after measurement once implemented.

**Costs:**
- **ABI stability commitment**, but *only* if/when third-party plugin
  authors want to ship prebuilt `.so` binaries against a specific Corbomite
  release. Today host + all plugins rebuild together in one repo/one CI —
  ABI fragility (ordinary C++ rebuild-everything-together behavior) doesn't
  matter. This only becomes a real ongoing cost the day someone wants
  binary plugin compatibility across Corbomite point releases; the
  alternative (perfectly normal, what KDE/Kate plugins do) is "recompile
  your plugin against the Corbomite version you target." Don't invest in
  a stable/versioned interface boundary (pimpl, pure-virtual factory) until
  there's an actual third-party plugin author asking for it.
- Link-time errors move to load/dlopen-time for anything mis-declared
  (missing symbol, wrong `PRIVATE`/`PUBLIC` link visibility). Caught
  immediately by the test suite in practice (per the vault precedent), but
  a different failure mode than today's link-time errors.
- **Not a security/sandboxing boundary — worth being explicit about this
  so it isn't conflated.** Untrusted plugins already run fully in-process
  today (`corbomite_add_plugin`'s `TRUSTED` flag is a metadata/permission
  concept, not a process-isolation one). Switching static→shared changes
  nothing about the trust model in either direction.

## 7. Recommendation

Proceed. The vault precedent already proves the mechanics work cleanly in
this exact codebase (no export macros, RPATH already works, packaging
already generic), it fixes a real latent correctness bug shared by every
plugin-linked library (not just a disk nice-to-have), and — per the user's
own framing — there are no other developers to inconvenience and nothing
external depends on today's static-linking behavior, so this is the
cheapest this change will ever be to make. Recommend scoping to the full
cross-repo change (Corbomite + Markoff + Graffodil) rather than
Corbomite-only, specifically because a partial fix leaves the single
biggest disk contributor (the `markoff_core`-linking app-level tests)
mostly unaddressed and leaves the identical correctness bug live in
`markoff_core`'s and `graffodil-core`'s classes.

## 8. Explicitly out of scope for this report

This document is research, not a plan — per the project's stub-plan
convention, brainstorm + full phased plan expansion happens in a follow-up
session. Not decided here:
- Phase breakdown / ordering (e.g. leaf libraries before libraries with the
  most dependents, or vice versa; whether to do all libraries in one pass
  or incrementally with a working build at every step).
- Whether to install ccache/sccache as a companion fix (raised in the same
  conversation that produced this report; complementary, not a substitute
  — worth a brainstorm note but a separate, much smaller task).
- Named regression tests / acceptance gate for "the switch didn't break
  anything" (link resolution, plugin load, `qobject_cast` across the
  boundary for at least one previously-static library to positively prove
  the fix, a real packaging build — AppImage/PKGBUILD/.deb — to close the
  "not independently verified" gap noted in §4).
- Whether `libs/mmdr`'s Rust staticlib conversion belongs in this cluster
  or is punch-listed separately (it's small; likely not worth blocking on).

## Corrections (2026-08-20, plan expansion)

The plan-expansion pass for Cluster P
([`docs/superpowers/plans/2026-08-20-cluster-p-shared-libraries.md`](superpowers/plans/2026-08-20-cluster-p-shared-libraries.md))
verified this report's claims empirically against the built `build-dev/`
tree (`nm`, `nm -D`, `readelf`, `ldd`), not by reading CMake, and found
three corrections. **Where this addendum and the report's body disagree,
this addendum is normative** — see the plan's §1 for the full evidence and
reasoning; only the summary is repeated here.

1. **§3's "why the bug hasn't bitten yet" is wrong.** It is not that
   plugins only cast their own plugin-local types. The real mechanism is
   ELF symbol interposition: `libs/vault` is `SHARED` and statically
   PUBLIC-links `Corbomite::Core`/`Storage`, so `libvault.so` re-exports 37
   `Corbomite::*` `staticMetaObject` symbols into the process's global
   scope, and the executable — which defines no strong symbols of its own
   at all — imports them from there. `libvault.so` is accidentally acting
   as libcore for everything its link graph happens to pull in. This does
   not generalise to libraries outside that graph (see next point).
2. **The divergence is not latent everywhere — it is already live** for
   `Corbomite::Models`/`Search`/`Bases`/`canvas`/`forcegraph`. Verified: the
   executable and `corbomite-file-explorer.so`/`corbomite-search.so` hold
   two different `staticMetaObject` definitions for `Corbomite::
   NotesTreeModel` and `Corbomite::SearchResultsModel` today, not a latent
   risk. Consequence: **the correctness acceptance gate must target one of
   these diverged libraries**, not `Core`/`Storage` — a test against `Core`
   would pass pre-refactor and prove nothing.
3. **BLOCKER this report missed:** `corbomite-core` and `corbomite-storage`
   form a dependency cycle (`storage` links `core` PUBLIC when not
   top-level; `core` links `storage` PRIVATE via `MarkoffAdapters.cpp`).
   CMake only tolerates a cycle when every target in the strongly connected
   component is `STATIC` — introducing `SHARED` anywhere in that SCC is a
   configure-time error. This must be cut (plan Phase P2) before either
   library can flip; it is thin (two files, one of which is a header-only
   pure-abstract interface contributing zero link symbols) but it is not
   the "one-line `add_library` edit" §7 implies for every library.

A fourth, non-contradicting scope correction: §7's recommendation to scope
the full cross-repo change up front undersells the ordering available —
`CorbomiteApp` (the single largest disk contributor, 47 MB, linked by zero
plugins) is Corbomite-only, independent of the cycle above, and carries no
plugin-boundary correctness risk, so it can and should land first
(plan Phase P1) rather than waiting on the cross-repo question.
