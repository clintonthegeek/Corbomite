# Obsidian Audit

Reverse-engineered specs and analysis of the Obsidian source code, for the purpose of building Corbomite — a KDE/Qt-native, Obsidian-compatible knowledge management system.

## Method

Three-pass audit of `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/`. The JS is read *once*, per-domain specs are distilled, then Corbomite planning references the distilled specs (not the JS).

- **Pass 1 — Taxonomy skim.** One agent walks every domain, produces `00-taxonomy.md` (routing table) and seeds two running lists: `01-markoff-gaps.md` and `02-extension-surfaces.md`.
- **Template lock.** The Pass 2 template is designed against what Pass 1 reveals, then piloted on `obsidian/vault/` (small, critical-for-compat). Adjust before fan-out.
- **Pass 2 — Per-domain deep audit.** ~15 parallel subagents, one per in-scope domain. Each reads only its assigned directory, fills the locked template, outputs to `domains/<name>.md`.
- **Pass 3 — Synthesis.** One capable agent reads the distilled domain docs, produces `FEATURE-MATRIX.md`, `VAULT-FORMAT.md`, `GAP-ANALYSIS.md`.

## Scope

**In scope (vanilla Obsidian feature parity):** `obsidian/{vault, workspace, metadata, views, editor, rendering, search, bases, parsing, settings, ui/*, core, utils, platform, secrets}`.

**Lightly in scope (to inform future Corbomite plugin system):** `obsidian/plugin/` — *extension surfaces only*, not plugin content.

**Out of scope:** `vendor/codemirror/` (we use Markoff), `obsidian/publish/` (online sync service), `obsidian/network/` (except where it exposes extension hooks), `uncategorized/`, `_internal.js` raw bundle.

## Translation principle

Corbomite is not a visual clone. It is a *translation* into the KDE/Qt idiom:

- Where Obsidian uses HTML/CSS widgets, Corbomite uses Qt widgets or `Markoff::Editor` / `Markoff::ReadingView`.
- Where Obsidian uses its own modal/menu/popover machinery, Corbomite uses `KMessageBox`, `QMenu`, `KActionCollection`, KDE standard dialogs.
- Where Obsidian defines its own keybinding layer, Corbomite uses `KStandardShortcut`.
- Where Obsidian stores vault config under `.obsidian/`, Corbomite must read and write **the same on-disk format** — this is the non-negotiable compatibility contract.

Features must behave familiarly to an Obsidian user. UX need not match pixel-for-pixel.
