# vault

Corbomite's canonical Vault aggregate. Single-vault-per-process. Owns the
`TFile`/`TFolder` tree, emits signals on every mutation, composes a
`DataAdapter *` for atomic file I/O, and hosts `FileManager` for link-aware
refactoring operations.

## Scope

Collapses what used to be split across `Corbomite::VaultModel` (libs/models),
the Task-7 path-only `Corbomite::Vault` (libs/core, deleted), `VaultProcess`
+ `VaultTrash` + `VaultScanner` (libs/storage), `FileWatchReactor`
(src/reactors), and parts of `VaultService` (src/app). Shape-parity with
Obsidian's `App.vault` + `App.fileManager` — see
`../../docs/superpowers/specs/2026-04-16-vault-architecture-design.md`.

Depends on:
- `Corbomite::Core` (Events mixin, NoteMeta, NoteDocument, FrontMatter types)
- `Corbomite::Storage` (DataAdapter, FileSystemAdapter, VaultConfig,
  MetadataCache — MetadataCache is consumed by FileManager only)

## Conventions

- C++20, Qt6.
- Use `i18n()` for all user-visible strings.
- SPDX header `GPL-3.0-or-later` on every source file.
- Public API in `include/corbomite/vault/`; plugin proxies in
  `include/corbomite/vault/proxies/`; internal types in `src/`.
- Namespace: `Corbomite`.

## Building

Built as part of the parent Corbomite tree via
`add_subdirectory(libs/vault)`. Tests run with `QT_QPA_PLATFORM=offscreen`.

## Testing

Tests live in `tests/`. Tests define expected behavior — when a test fails,
fix the code, not the test.
