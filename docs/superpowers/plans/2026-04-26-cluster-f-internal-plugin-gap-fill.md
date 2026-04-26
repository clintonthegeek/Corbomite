# Cluster F — Internal-plugin gap fill

> **Created 2026-04-26 from audit reset.** Stub plan; needs brainstorm + full plan expansion before dispatch. Obsidian ships ~25 internal core plugins; Corbomite ships 9 (under `src/plugins/`). This cluster ships the 8 highest-user-expected missing ones.

## Goal

A user opening Corbomite for the first time has the same out-of-box surface as Obsidian for the most-used built-in tools: command palette, switcher, daily-notes, templates, page-preview, word-count, file-recovery, note-composer.

## Audit references

- [audit-2026-04-26/plugin.md](../../audit-2026-04-26/plugin.md) §"Internal-plugin inventory"
- [audit-2026-04-26/core-and-addenda.md](../../audit-2026-04-26/core-and-addenda.md) §"File Recovery" + §"Daily Notes + Templates"
- [obsidian-audit/addenda/2026-04-19-file-recovery-plugin.md](../../obsidian-audit/addenda/2026-04-19-file-recovery-plugin.md)
- [obsidian-audit/addenda/2026-04-15-daily-notes-templates-schemas.md](../../obsidian-audit/addenda/2026-04-15-daily-notes-templates-schemas.md)

## Scope (in scope, prioritized)

1. **Command palette** (currently `KCommandBar` is in-app; verify it's plugin-shaped — may already qualify, audit needs a closer look)
2. **Quick switcher** (likely already present; audit it for parity gaps with Obsidian's filename-fuzzy + alias-aware switcher)
3. **Daily notes** (read `.obsidian/daily-notes.json` already; need the plugin: command + folder template + date-format + open-on-startup)
4. **Templates** (read `.obsidian/templates.json` already; need the insert-template command + variable substitution + folder picker)
5. **Page preview** (HoverPopover over `[[link]]` already works; needs to register as a real internal plugin so it's toggle-able)
6. **Word count** (status bar widget — gated on Cluster B `addStatusBarItem`)
7. **File recovery** (Version History modal + backup store; addendum has full spec; significant work)
8. **Note composer** (merge-file modal + split-file commands)

## Out of scope

- Plugin-system itself → already shipped (legacy Cluster Q/N)
- Bookmarks → already shipped (legacy Cluster S)

## Dependencies

- #6 (word count) is gated on **Cluster B** delivering `addStatusBarItem`
- #7 (file recovery) wants `Cluster R`'s "Open version history" hamburger slot (already present per legacy retro)

## Phases

TBD — brainstorm. Likely 4 phases ordered by complexity: (1) audit + verify already-present #1-2, (2) build #3-5 (small), (3) build #6 + #8, (4) build #7 (largest).

## Status

**Plan-needed** (stub). Several items are gated on other clusters; sequencing matters.
