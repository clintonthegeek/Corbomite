# Corbomite

A native Obsidian-compatible knowledge base application built with C++20, Qt6, and KDE Frameworks 6.

> ## ⚠️ Alpha software — back up your vaults
>
> Corbomite is under active, fast-moving development and has **not** had a
> stable release. It reads and writes real Obsidian vaults on disk, including
> destructive operations (rename, delete, trash, link rewriting on move).
> While a great deal of care has gone into vault-format fidelity and
> corruption-safety, this is alpha-quality software: expect rough edges,
> incomplete features, and the occasional bug.
>
> **Do not point Corbomite at a vault you don't have a backup of.** Test
> against a copy first, or use a vault under version control (git, or a
> dedicated backup tool) so you can recover if something goes wrong. This
> applies doubly to the experimental Markoff canvas Live Preview engine
> (opt-in, off by default) and to any `.canvas`/Bases files, which are newer,
> less-exercised code paths.

## License

GPLv3-or-later

---

## Features

Corbomite aims for practical, native-desktop compatibility with Obsidian
vaults — read/writing the same Markdown + YAML frontmatter + `.obsidian/`
config format, not a reimplementation of Obsidian's UI.

- **Three interchangeable editor leaves** for the same document: a live
  WYSIWYG-ish Markdown preview ("Live Preview"), a plain-text source editor,
  and a read-only styled Reading view — instant, lossless switching between
  them, powered by the [Markoff](#relationship-to-markoff-collabtext-and-graffodil)
  editor family.
- **Wikilinks, backlinks, and outlinks** — `[[Note]]` / `[[Note#Heading]]` /
  `[[Note^block-id]]` linking, with live backlink and outlink panels, hover
  previews, and vault-wide link resolution/rewriting on rename.
- **Full-text and property search**, with Obsidian's own search-operator
  syntax (`tag:`, `path:`, `file:`, boolean/quoted terms, etc.), backed by a
  local SQLite index — no cloud, no telemetry.
- **Tags, YAML frontmatter properties, and Bases** — `.base` files (YAML
  schema, formula language, filter builder, table views) for
  spreadsheet-like structured views over your notes.
- **`.canvas` whiteboard support** — Obsidian-compatible canvas files
  (nodes, edges, groups) with round-trip fidelity, including passthrough of
  fields written by other apps/plugins.
- **Graph view** (global and local/note-centered), rendered with a
  Barnes-Hut force-directed layout engine.
- **Daily notes and templates**, configurable folders/date formats/template
  insertion.
- **Docked, tabbed, splittable workspace** (via KDDockWidgets) with
  session persistence, matching Obsidian's pane/tab/split model.
- **An internal plugin system** (file explorer, search, backlinks, outlinks,
  local graph, outline, properties, bookmarks — each a first-class internal
  plugin, not hardcoded UI) with a permission-scoped host API, so the same
  seam that ships these built-ins is available to future third-party
  plugins.
- **Native KDE integration** — KDE Frameworks 6 (`KXmlGui`, icon themes,
  color schemes, standard actions/shortcuts), not a themed Electron/web
  wrapper.
- Runs entirely **locally and offline**; no account, no sync service, no
  network dependency for core functionality.

See [`docs/PARITY-MATRIX.md`](docs/PARITY-MATRIX.md) for a detailed,
continuously-updated breakdown of feature parity against Obsidian.

## Relationship to Markoff, collabtext, and Graffodil

Corbomite is built on top of a small family of sibling projects, developed
alongside it and shared where useful:

- **[Markoff](https://github.com/clintonthegeek/Markoff)** is the Markdown
  editor widget family Corbomite embeds as a git submodule
  (`libs/markoff-family`) — it owns the actual text-editing surface: the
  CRDT-backed document model, the tree-sitter Markdown parser, and the
  interchangeable editor "leaves" (Live Preview, Source, Reading, and an
  experimental Canvas-based rendering engine). Markoff is developed as an
  independent, reusable library — not Corbomite-specific — but Corbomite is
  currently its primary consumer and the two are developed in close
  coordination.
- **[collabtext](https://github.com/clintonthegeek/collabtext)** is a CRDT
  (conflict-free replicated data type) text-editing engine, nested as a
  submodule inside Markoff (`libs/markoff-family/libs/collabtext`). It gives
  Markoff's document model real-time collaborative-editing semantics — local
  edits and (future) remote/multi-user edits converge deterministically —
  even though Corbomite doesn't yet expose any multi-user collaboration
  feature itself.
- **[Graffodil](https://github.com/clintonthegeek/Graffodil)** is a
  standalone force-directed graph visualization project. Corbomite's graph
  view (`libs/forcegraph`) shares lineage and layout-algorithm heritage with
  Graffodil (Barnes-Hut approximation, multilevel layout) but is vendored
  in-tree as its own independent library with zero Corbomite-specific
  dependencies, rather than a live submodule — the two are related by
  shared origin, not a build-time link.

---

## Getting the Source

Corbomite depends on a chain of git submodules — most importantly the
**Markoff** editor family, which in turn nests the **collabtext** CRDT
library. **A plain `git clone` is not enough**: without the submodules the
build fails at configure time with errors like
`Target "..." links to: Markoff::Parser but the target was not found` or
`libs/collabtext is empty`.

### Clone with submodules (recommended)

```bash
git clone --recurse-submodules git@github.com:clintonthegeek/Corbomite.git
cd Corbomite
```

### Already cloned without `--recurse-submodules`?

```bash
cd Corbomite
git submodule update --init --recursive
```

The `--recursive` flag is **required**, not optional: it pulls the nested
`collabtext` submodule inside `libs/markoff-family`. The submodule tree is:

```
Corbomite
└── libs/markoff-family        → Markoff      (git@github.com:clintonthegeek/Markoff.git)
    └── libs/collabtext        → collabtext   (git@github.com:clintonthegeek/collabtext.git)
```

> **Access note:** the submodules are hosted on Codeberg over SSH. You need an
> SSH key registered with a Codeberg account that can read these repositories.
> Test with `ssh -T git@github.com` — it should authenticate successfully.

### Keeping submodules in sync after a pull

Whenever you `git pull` (or check out a different branch), the recorded
submodule commits may have moved. Re-sync the working tree to the commits the
superproject pins:

```bash
git pull
git submodule update --init --recursive
```

If a build suddenly fails with a missing `Markoff::*` target or an "empty
submodule" error after pulling, it almost always means this step was skipped —
the submodule is checked out at the wrong commit. Confirm with:

```bash
git submodule status --recursive
```

A leading `+` on any line means the checked-out commit differs from the one
the superproject pins; `git submodule update --init --recursive` fixes it.

---

## Building

### Dependencies

Corbomite requires the following at build time:

| Dependency | Version | Arch/Manjaro package |
|---|---|---|
| C++ compiler | C++20 (GCC 13+ / Clang 16+) | `gcc` |
| CMake | 3.19+ | `cmake` |
| Ninja (recommended) | any | `ninja` |
| Qt6 | **6.8+** — `Core Widgets DBus Sql Svg PrintSupport` (Declarative still pulled transitively by KDDockWidgets) | `qt6-base qt6-svg qt6-declarative qt6-5compat` |
| Extra CMake Modules (ECM) | 6.0+ | `extra-cmake-modules` |
| KDE Frameworks 6 | `CoreAddons I18n XmlGui WidgetsAddons IconThemes Config ConfigWidgets ColorScheme DBusAddons SyntaxHighlighting` | `kf6-kcoreaddons kf6-ki18n kf6-kxmlgui kf6-kwidgetsaddons kf6-kiconthemes kf6-kconfig kf6-kconfigwidgets kf6-kcolorscheme kf6-kdbusaddons kf6-ksyntaxhighlighting` |
| KDDockWidgets | **2.0+** (Qt6 build) | `kddockwidgets-qt6` (AUR) |
| tree-sitter | any recent (found via `pkg-config`) | `tree-sitter` |
| Qt6Keychain | optional — persistent secret storage | `qt6keychain` (AUR) |

`JKQTMathText`, `mmdr`, and the tree-sitter Markdown grammar are **vendored**
in-tree (`libs/jkqtmathtext`, `libs/mmdr`, and within the Markoff submodule) —
no system package needed. If `Qt6Keychain` is absent the build auto-disables
persistent secret storage and falls back to an in-process store (with a
runtime warning); it is not required.

One-shot install of the official-repo dependencies on Arch/Manjaro:

```bash
sudo pacman -S --needed gcc cmake ninja \
  qt6-base qt6-svg qt6-declarative qt6-5compat \
  extra-cmake-modules \
  kf6-kcoreaddons kf6-ki18n kf6-kxmlgui kf6-kwidgetsaddons kf6-kiconthemes \
  kf6-kconfig kf6-kconfigwidgets kf6-kcolorscheme kf6-kdbusaddons \
  kf6-ksyntaxhighlighting \
  tree-sitter
# AUR (e.g. with paru):
paru -S kddockwidgets-qt6 qt6keychain
```

### Configure and build

Two CMake presets are defined in `CMakePresets.json`:

| Preset | Build dir | Type | Notes |
|---|---|---|---|
| `dev` | `build-dev/` | Debug | `CORBOMITE_DEV_BUILD=ON` — isolated config/data, `[Dev]` window title |
| `release` | `build-release/` | Release | `CMAKE_INSTALL_PREFIX=/usr/local` |

Development build:

```bash
cmake --preset dev
cmake --build build-dev -j"$(nproc)"
```

Run it:

```bash
./build-dev/bin/Corbomite
```

### Release build and install

```bash
cmake --preset release
cmake --build build-release -j"$(nproc)"
sudo cmake --install build-release
```

### Packaging (AppImage / Arch)

More detail lives in [`packaging/README.md`](packaging/README.md). Both formats
are **Release** builds (`CORBOMITE_DEV_BUILD=OFF`) with Canvas-only LivePreview.
Desktop / D-Bus identity is `com.concernednetizen.Corbomite`.

#### AppImage (portable)

Needs network once to fetch `linuxdeploy` / `appimagetool` into
`packaging/appimage/tools/` (cached on later runs), plus the usual Corbomite
build dependencies.

```bash
# From the repository root:
./packaging/appimage/build-appimage.sh
# → packaging/appimage/out/Corbomite-0.1.0-x86_64.AppImage
```

Smoke / run:

```bash
./packaging/appimage/out/Corbomite-0.1.0-x86_64.AppImage --version
./packaging/appimage/out/Corbomite-0.1.0-x86_64.AppImage --help
./packaging/appimage/out/Corbomite-0.1.0-x86_64.AppImage /path/to/vault
```

If FUSE is unavailable, the script extracts the tools automatically. You can
also run with `--appimage-extract-and-run` if the host cannot mount AppImages.

#### Arch package (`makepkg`)

Requires the same system packages as a normal Corbomite build (Qt6, KF6,
`kddockwidgets`, `tree-sitter`, …) — see **Dependencies** above. Submodules
must already be initialized in this checkout.

```bash
cd packaging/arch
makepkg -f
# optional metadata for AUR:
makepkg --printsrcinfo > .SRCINFO
sudo pacman -U corbomite-0.1.0-1-x86_64.pkg.tar.zst
```

The PKGBUILD builds from the enclosing git work tree by default. For a
published AUR package, point `source=` at a Codeberg tag (e.g. `v0.1.0`) and
bump `pkgrel` as needed.

#### CMake presets used by packaging

| Preset | Prefix | Notes |
|---|---|---|
| `release` | `/usr/local` | Local `cmake --install` |
| `appimage` | `/usr` (via `DESTDIR=AppDir`) | RPATH `$ORIGIN/../lib`; plugins under `lib/plugins` |

The dev and release builds use **separate** config and data directories, so an
installed release and a local dev build never interfere:

| | Release | Dev (`CORBOMITE_DEV_BUILD=ON`) |
|---|---|---|
| Config | `~/.config/corbomiterc` | `~/.config/corbomite-devrc` |
| Data | `~/.local/share/corbomite/` | `~/.local/share/corbomite-dev/` |
| Window title | "Corbomite" | "Corbomite [Dev]" |

### Running tests

Tests are disabled when Corbomite builds its submodules (the Markoff/collabtext
suites run from their own repos). To build and run Corbomite's own tests, run
`ctest` from the build directory:

```bash
cd build-dev && ctest --output-on-failure -j"$(nproc)"
```

### Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `Target "..." links to: Markoff::Parser but the target was not found` | Markoff submodule missing or at the wrong commit | `git submodule update --init --recursive` |
| `libs/collabtext is empty — the collabtext submodule has not been initialized` | Nested submodule not initialized | `git submodule update --init --recursive` (note `--recursive`) |
| `Could not read from remote repository` during submodule update | No SSH access to GitHub | Register an SSH key on GitHub; test `ssh -T git@github.com` |
| `find_package(KDDockWidgets-qt6 2.0 REQUIRED)` fails | KDDockWidgets not installed | Install `kddockwidgets-qt6` (AUR on Arch/Manjaro) |
| `Could NOT find ECM` | extra-cmake-modules missing | Install `extra-cmake-modules` |

---

## Third-Party Libraries

| Library | License | Purpose |
|---|---|---|
| [RapidYAML (ryml)](https://github.com/biojppm/rapidyaml) | MIT | YAML frontmatter parsing and emission (10-200x faster than yaml-cpp) |
| [c4core](https://github.com/biojppm/c4core) | MIT | Core library for RapidYAML |
| [fast_float](https://github.com/fastfloat/fast_float) | Apache-2.0 / Boost-1.0 / MIT | Fast float parsing (c4core dependency) |
| [debugbreak](https://github.com/scottt/debugbreak) | BSD-2-Clause | Portable debug break (c4core dependency) |
| [JKQTMathText](https://github.com/jkriege2/JKQtPlotter) | LGPL-2.1-or-later | LaTeX math formula rendering |
| [tree-sitter](https://github.com/tree-sitter/tree-sitter) | MIT | Incremental parsing framework |
| [tree-sitter-markdown](https://github.com/tree-sitter-grammars/tree-sitter-markdown) | MIT | Markdown grammar for tree-sitter |
