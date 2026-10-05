# Cluster F — Internal-plugin gap fill

> **⚠ SPLIT 2026-10-01 — this stub is superseded. Do not dispatch from here.**
>
> Every one of the 10 items was audited against the current tree on
> 2026-10-01. The headline: **six of the ten are already shipped as host
> features**, not as plugins, and one (item 4's "register as a real internal
> plugin" framing for page preview) is now moot because Cluster E — the
> plugin-facing editor surface this cluster deferred to — was retired the same
> day. What genuinely remains splits cleanly into two self-contained
> clusters:
>
> - **[Cluster F-a — File Recovery & Note Composer](../2026-10-01-cluster-f-a-file-recovery-and-composer.md)** — items 7 + 8. Self-contained, no substrate dependency, ~5–8d.
> - **[Cluster F-b — Named Workspaces & Sidedock substrate](../2026-10-01-cluster-f-b-workspaces-and-sidedock.md)** — items 9 + 10. **Needs its own spec first**; both items also lost their home class to Cluster L3. ~6–9d.
>
> **Item-by-item audit (2026-10-01, evidence in the two F plans):**
>
> | # | Item | Verdict | Key evidence |
> |---|---|---|---|
> | 1 | Command palette | **SHIPPED** — `KCommandBar`, Ctrl+P, and it *is* plugin-inclusive (it lists `CommandRegistry` commands alongside host KActions) | `src/app/MainWindow.cpp:2038-2080`, `:1361-1365` |
> | 2 | Quick switcher | **SHIPPED, filename-only** — fuzzy match runs against the note name only; no frontmatter-alias, content, or heading search | `src/dialogs/QuickSwitcher.cpp:47-48`; gap acknowledged in-code at `src/dialogs/QuickSwitcherModel.h:36` |
> | 3 | Daily notes | **MOSTLY SHIPPED at host level** — service, action, folder/format/template settings, `.obsidian/daily-notes.json` read, template applied on create. Missing: open-on-startup (no settings key exists) | `libs/models/src/DailyNoteService.cpp:82-133`; `src/app/MainWindow.cpp:1386,2689`; `src/dialogs/SettingsDialog.cpp:188-214`; `src/app/corbomite.kcfg:116-124` |
> | 4 | Templates | **MOSTLY SHIPPED at host level** — `{{title}}/{{date}}/{{time}}/{{folder}}/{{cursor}}` substitution, picker dialog, `.obsidian/templates.json` read. Missing: insert-on-note-creation hook | `libs/models/include/corbomite/models/TemplateService.h:30-54`; `src/app/MainWindow.cpp:2660-2687` |
> | 5 | Page preview | **SHIPPED and live** — `HoverPopover` created once and attached to every markdown view; re-lit onto the styled headless render path 2026-06-11. The "register as a toggleable internal plugin" ask is **moot** (Cluster E retired) | `src/app/MainWindow.cpp:418,1033`; `src/editor/HoverPopover.h:14-37` |
> | 6 | Word count | **SHIPPED as a host status-bar label** — not via `StatusBarRegistry` (that is the *plugin* hook). No character count | `src/app/MainWindow.cpp:2698-2702`; `src/editor/NoteEditorWidget.cpp:487-496` |
> | 7 | File recovery | **PARTIAL — dispatch moved to Cluster F-a** — backup *destination* is correct (outside the vault) but the only trigger is save **failure**; no periodic snapshot, no Version History modal. The disabled "Version history" menu slot to activate already exists | `libs/core/src/TextFileView.cpp:67-71,137-165`; `libs/core/src/EditableFileView.cpp:117-136`; `tests/core/tst_editable_file_view_menu.cpp:243-264` |
> | 8 | Note Composer | **ABSENT — dispatch moved to Cluster F-a** — zero code. Addendum has the full modal + pipeline spec | `docs/obsidian-audit/addenda/2026-04-19-merge-file-modal.md` |
> | 9 | Named Workspaces | **ABSENT — dispatch moved to Cluster F-b** — zero references to `workspaces.json` outside docs. Schema is *inferred*, not extracted (an open audit question) | `docs/obsidian-audit/VAULT-FORMAT.md:362-368`; `docs/obsidian-audit/domains/workspace.md §13 Q5` |
> | 10 | Sidedock-as-workspace-tree | **LOST ITS HOME CLASS — dispatch moved to Cluster F-b.** Cluster L3 deleted `WorkspaceSidedock` as a nullptr shell; there is nothing left to refactor, and `Workspace.h` explicitly hands the job to Cluster F | `libs/core/include/corbomite/core/Workspace.h:248-253`; `src/mdi/CorbomiteMDI.h:212` |
>
> The old "plugins touching the editor API wait on E's rework" dependency in
> this stub is now **void** — Cluster E was retired 2026-10-01
> ([INDEX](INDEX.md) closed table). Nothing in F-a or F-b touches the editor
> API. Body below is kept verbatim as the historical scope.

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
6. **Word count** (status bar widget — gate cleared 2026-04-28: Cluster B closed, `StatusBarRegistry` exists)
7. **File recovery** (Version History modal + backup store; addendum has full spec; significant work)
8. **Note composer** (merge-file modal + split-file commands)

**Pulled in from Cluster C on closeout (2026-04-27):**
9. **Workspaces** internal plugin — named-layout snapshots persisted to `.obsidian/workspaces.json` (Record<workspaceName, LayoutJson>). Schema at [obsidian-audit/VAULT-FORMAT.md](../../obsidian-audit/VAULT-FORMAT.md) §`.obsidian/workspaces.json`. Owns the "save / load / delete named workspace" command set. Cluster Y γ-scope deferral; pre-reset legacy `cluster-retros/cluster-y.md`.
10. **Sidedock-as-workspace-tree** substrate refactor — `WorkspaceSidedock` is a stub; `Workspace::leftSplit() / rightSplit()` return nullptr. Today sidebars live outside the tree in `CorbomiteMDI::Sidebar`. Required for plugin-API parity with Obsidian (`workspace.leftSplit.collapse()` etc.) and for round-tripping the `left`/`right` subtrees in `workspace.json` rather than passing them through with the dirty-bit hack from P1 #4. Cross-cuts Workspaces plugin — work the substrate before / alongside #9.

## Out of scope

- Plugin-system itself → already shipped (legacy Cluster Q/N)
- Bookmarks → already shipped (legacy Cluster S)

## Dependencies

- #6 (word count) — gate cleared 2026-04-28: **Cluster B** closed; `StatusBarRegistry` exists
- #7 (file recovery) wants `Cluster R`'s "Open version history" hamburger slot (already present per legacy retro)

## Phases

TBD — brainstorm. Likely 4 phases ordered by complexity: (1) audit + verify already-present #1-2, (2) build #3-5 (small), (3) build #6 + #8, (4) build #7 (largest).

## Status

**SPLIT 2026-10-01 — superseded by [F-a](2026-10-01-cluster-f-a-file-recovery-and-composer.md) (items 7+8) and [F-b](2026-10-01-cluster-f-b-workspaces-and-sidedock.md) (items 9+10).** Items 1–6 are already live as host features — see the audit table in the banner for per-item evidence. Do not dispatch from this stub.
