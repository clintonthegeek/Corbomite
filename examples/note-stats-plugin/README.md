# Note Statistics — Corbomite Reference Plugin

Reads every markdown file in the vault; shows counts in the right
sidebar. Demonstrates `VaultProxy`, `SearchProxy`, `MetadataCacheReader`,
sidebar-view registration, and reactive refresh via Qt signals.

## Build

```sh
cmake -B build -DCORBOMITE_BUILD_EXAMPLES=ON
cmake --build build --target note-stats
```

## Install

```sh
cmake --install build --component note-stats
```

Or, for a distro package, see `docs/plugin-development/DISTRIBUTION.md`.

## First enable

The plugin declares `vault.read`, `vault.events`, `metadata.read`,
`ui.views`. On first enable, Corbomite prompts you to approve these —
click "Allow". On subsequent launches, no prompt fires until you disable
and re-enable.

## Code walkthrough

See `docs/plugin-development/TUTORIAL.md`.
