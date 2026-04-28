# Cluster A — Vault-format compatibility sweep

> **Created 2026-04-26 from audit reset.** Stub plan; needs brainstorm + full plan expansion before dispatch. Aggregates the P0 vault-format-corruption items from the audit into a single coordinated cluster because they share invariants ("byte-faithful round-trip with Obsidian-authored vaults") and want a shared test fixture suite.

## Goal

Eliminate every silent on-disk format divergence between Corbomite and Obsidian. After this cluster, a vault round-tripped through Corbomite must be byte-identical to the same vault round-tripped through Obsidian (modulo intentional content edits).

## Audit references

- [audit-2026-04-26/parsing.md](../../audit-2026-04-26/parsing.md) §"Critical round-trip bug" — `processFrontMatter` reorders YAML keys
- [audit-2026-04-26/vault.md](../../audit-2026-04-26/vault.md) §"Notable concerns / suspected bugs" — folder rename, link rewrite, BOM strip, `raw`/`config-changed`
- [audit-2026-04-26/settings.md](../../audit-2026-04-26/settings.md) §"On-disk schema compatibility matrix" — two parallel JSON writers (4-space vs 2-space)
- [audit-2026-04-26/core-and-addenda.md](../../audit-2026-04-26/core-and-addenda.md) §"Vault-format-critical utils" — `resolveSubpath` block-id case sensitivity
- [audit-2026-04-26/bases.md](../../audit-2026-04-26/bases.md) §"On-disk `.base` format compatibility" — `.base` YAML emitter alphabetises keys
- [audit-2026-04-26/workspace.md](../../audit-2026-04-26/workspace.md) §"Layout JSON compatibility risks" — split flattening, `currentTab` global (cross-references with Cluster C)

## Scope (in scope)

1. `processFrontMatter` insertion-order preservation
2. Consolidate `Vault::writeConfigJson` and `VaultConfig::serializeObsidianStyle` to one writer; route plugin paths through the byte-faithful one
3. `.base` YAML emitter key-order preservation (shares root cause with #1 — likely one fix)
4. `resolveSubpath` block-id lowercase normalization
5. `Document::withFrontmatter` empty-frontmatter shell elimination
6. Folder rename descendant `m_fileMap` rekeying + cascading `renamed` events
7. Link rewrite fidelity in `FileManager::renameFile` (markdown links, full-path wiki forms, `useMarkdownLinks=true` mode)
8. BOM strip on read; preserve / restore on write
9. `Vault.raw` and `Vault.config-changed` events emission + `.obsidian/` watcher inclusion
10. Case-sensitivity collision detection (`CaseSensitivityProbe` is dead code; wire it)

## Out of scope

- Workspace serializer fidelity → **Cluster C** (related but multi-phase, has its own scope)
- Frontmatter parsing edge cases beyond round-trip → punch list

## Phases

(Plan never expanded — items closed inline through the post-reset P0 sweep + 2026-04-27 closeout. See per-item status below.)

## Status

**Closed 2026-04-27.** Drained inline rather than as a coordinated cluster.

### Disposition

| # | Item | Status |
|---|---|---|
| 1 | `processFrontMatter` insertion-order preservation | Closed (P0 punch-list, libs/storage front-matter parser) |
| 2 | Consolidate `Vault::writeConfigJson` ⇄ `VaultConfig::serializeObsidianStyle` | Closed (P0 punch-list) |
| 3 | `.base` YAML emitter key-order preservation | Closed (P0 punch-list — top-level + per-view canonical order; user-keyed dicts P2 follow-up also closed) |
| 4 | `resolveSubpath` block-id case-insensitivity | Closed (P0 punch-list, `LinkUtils.cpp`) |
| 5 | `Document::withFrontmatter` empty-frontmatter shell elimination | Closed (P0 punch-list) |
| 6 | Folder rename descendant rekey + cascading `renamed` events | Closed (P0 punch-list, `Vault::rename` recursive walk) |
| 7 | `FileManager::renameFile` link rewrite fidelity | Closed (P0 punch-list — markdown-style + full-path forms via `MetadataCache` snapshot + shared `rewriteLinkLiteral` helper) |
| 8 | BOM strip on read | **Closed 2026-04-27** — `Vault::read` + `Vault::readRaw` strip leading U+FEFF; `readBinary` preserves bytes verbatim (test: `tst_vault_read::readStripsLeadingUtf8Bom`). Preserve-on-write was descoped — Obsidian itself normalizes away the BOM on save, so matching that behaviour is correct. |
| 9 | `Vault.raw` + `Vault.config-changed` events + `.obsidian/` watcher | **Reassigned to Cluster B** (items #15–#16) — really plugin event-surface work, not on-disk format. |
| 10 | Wire `CaseSensitivityProbe` (was dead code) | Closed (`Vault.cpp:85` — used in vault load probe) |
