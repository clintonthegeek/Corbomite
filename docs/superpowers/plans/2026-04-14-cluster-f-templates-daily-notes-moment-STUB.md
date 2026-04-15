# Cluster F — Templates / Daily Notes / Moment (STUB)

> **Living-status note:** This file is the *plan* (stub). Live status (Stub plan / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file. Edit this file only when the plan itself changes (or when expanded to a full plan — at which point rename to drop `-STUB` suffix and update INDEX).

**Plan written:** 2026-04-14 (stub). Expand to full plan when Cluster A and Cluster I have landed (this cluster depends on both).

**Covers:** P1.5 (Moment.js format compat), P2.15 (Daily Notes), P2.16 (Templates), P4.6 (`MomentFormatComponent` widget).

## Goal

Match Obsidian's date-formatting and template-substitution muscle memory. Templates and Daily Notes are how Obsidian users build muscle memory around `{{date:YYYY-MM-DD}}`-style placeholders; getting the format-token interpretation wrong (e.g. translating `Do` to `dd` instead of an ordinal day) breaks every existing template. Moment.js compat is the load-bearing decision; Templates and Daily Notes are mostly mechanical once it's resolved.

## Audit references

- **Moment.js exposure:** `domains/leaf-utilities.md §1` (`utils/moment.js`) — Obsidian re-exports `window.moment` verbatim; plugins import from the `"obsidian"` package and expect global locale. Format-token compat with Moment is required.
- **MomentFormatComponent widget:** `domains/ui-bundle.md §1` (sub-section `components/MomentFormatComponent`) — settings widget that lets users preview their format string against a live date.
- **Template placeholders:** `01-markoff-gaps.md` Per-document interactivity section — `{{date:YYYY-MM-DD}}` placeholders rely on Moment formatting.
- **Daily Notes / Templates as internal plugins:** `domains/core.md §7` — both are entries in the 31-plugin internal-plugin loadPlugin registry. Their canonical config keys (`templates` folder, daily-note format, daily-note location) live in `.obsidian/daily-notes.json` and `.obsidian/templates.json`. Schemas need extraction during planning.

## Target classes

| Class | File | Notes |
|---|---|---|
| `Corbomite::MomentFormatter` | `libs/core/src/MomentFormatter.{h,cpp}` | Format-token compat layer; either bundles moment.js via `QJSEngine` or hand-translates tokens to `QDateTime::toString` + custom ordinal/locale extensions |
| `Corbomite::TemplateService` (extend) | already exists at `libs/core/src/TemplateService.{h,cpp}` | Add `{{date:...}}`, `{{time:...}}`, `{{title}}` substitution using `MomentFormatter` |
| `Corbomite::DailyNoteService` (extend) | already exists at `libs/core/src/DailyNoteService.{h,cpp}` | Open-or-create today's daily note at the configured path with optional template applied |
| `Corbomite::MomentFormatPreview` | `src/dialogs/MomentFormatPreview.{h,cpp}` | Settings widget showing a live preview of a format string applied to "now" |

`TemplateService` and `DailyNoteService` already exist (referenced in `MainWindow.cpp` `insertTemplate` / `openDailyNote`); this cluster makes their formatting Obsidian-faithful.

## Sub-tasks (when expanded)

1. **Decide Moment-format strategy** (gated by Explore prompt below). Options: (A) bundle moment.js via `QJSEngine` — accurate, ~50KB+ runtime cost; (B) hand-write a token translator producing `QDateTime::toString` calls + custom helpers for tokens Qt lacks (ordinal `Do`, locale-aware day-name `dddd`, week-of-year `ww`/`WW`, escape `[literal]`).
2. **Extract `.obsidian/daily-notes.json` and `.obsidian/templates.json` schemas** from a real Obsidian-written sample vault (Cluster B Phase 6 produces these). Document in `VAULT-FORMAT.md` §3.
3. **Implement `MomentFormatter::format(QDateTime, QString formatString) → QString`** with the chosen strategy. Comprehensive unit tests covering every token Obsidian docs document.
4. **Wire `TemplateService` substitution** to use `MomentFormatter`. Add `{{title}}` (note basename), `{{folder}}`, `{{cursor}}` (cursor placement marker — Obsidian-compat).
5. **`DailyNoteService::openOrCreateToday()`** — read `.obsidian/daily-notes.json` for folder + format + template, compute today's path, create-from-template if missing.
6. **`MomentFormatPreview` widget** — drop into Settings → Daily Notes / Templates tabs.
7. **Migration**: detect existing Corbomite daily-note / template config keys (if any) and migrate to `.obsidian/`-shape on vault open.

## Explore prompt (one — Moment strategy decision)

> Evaluate the two strategies for Moment.js format-string compatibility in a Qt6 C++ project: (A) bundle moment.js (or moment.min.js) via Qt's `QJSEngine` JavaScript runtime and call `moment(date).format(token)` from C++; (B) hand-write a token translator from Moment format tokens to `QDateTime::toString` patterns with helpers for tokens Qt lacks (`Do`, `dddd` localised, `ww`, escape `[literal]`). For each, report: (a) runtime overhead estimate, (b) accuracy fidelity (which Moment tokens fail), (c) implementation effort estimate, (d) maintenance burden over time, (e) any precedent in `~/src/kde/src/` for a similar JS-bridge or format-translator pattern. Recommend one. Under 700 words.

## Definition of done

- `MomentFormatter` produces output byte-identical to Obsidian's `window.moment().format(...)` for every token in Obsidian's documented support list.
- `.obsidian/daily-notes.json` and `.obsidian/templates.json` round-trip via `VaultConfig` (Cluster B).
- Insert-template command applies `{{date:...}}`/`{{time:...}}`/`{{title}}` substitutions correctly; cursor lands at `{{cursor}}` marker if present.
- Open-daily-note creates today's note from configured template at configured path.
- Settings → Daily Notes / Templates tabs render with live `MomentFormatPreview`.

## Blocks / enables

- **Depends on:** Cluster A (`FrontMatter`, used by template files that have frontmatter), Cluster B (`VaultConfig` for the two new JSON files), Cluster I (`MetadataCache` for `{{folder}}`/`{{title}}` substitution context — partial dependency).
- **Blocks:** nothing critical; user-facing feature parity.
- **Enables:** Templates and Daily Notes as Obsidian users use them. Removes the muscle-memory friction.
- **Estimated effort:** 1–2 weeks if (A) JSEngine route; 2–3 weeks if (B) hand-translator route. Decision is the largest single cost driver.

## Notes on expansion

When expanding to full plan: read TemplateService/DailyNoteService current implementations first (`grep -l TemplateService src/`) — they may already cover most non-formatting logic, in which case this cluster collapses to "just MomentFormatter + schema integration."
