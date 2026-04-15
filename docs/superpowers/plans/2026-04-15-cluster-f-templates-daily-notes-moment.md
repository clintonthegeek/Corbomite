# Cluster F — Templates / Daily Notes / Moment

> **Living-status note:** This file is the *plan*. Live status (Not started / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file. Edit this file only when the plan itself changes.

**Plan written:** 2026-04-15. Expanded from the 2026-04-14 stub after Cluster A (frontmatter) and Cluster I (MetadataCache) landed, unblocking this cluster.

**Covers:** P1.5 (Moment.js format compat), P2.15 (Daily Notes), P2.16 (Templates), P4.6 (`MomentFormatComponent` widget).

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` to implement this plan phase-by-phase. Each phase ends in a green `ctest` + a commit.

**Goal:** Make Corbomite's template and daily-note substitution byte-identical to Obsidian's. The load-bearing design decision — *how* to achieve Moment.js format-string compatibility in a Qt6 C++ project — is resolved in Phase 1 (**Strategy B: hand-translate tokens to QDateTime + custom helpers**). Everything else is mechanical: wire `MomentFormatter` into `TemplateService`, migrate `DailyNoteService` config from KConfig to `.obsidian/daily-notes.json`, extend `VaultConfig` with typed accessors, add a live-preview widget to Settings.

**Architecture:** New `libs/core/MomentFormatter` (pure function + helpers) translates Moment format strings to QDateTime output. Zero new dependencies; ~95% token fidelity with Obsidian. Existing `libs/models/TemplateService` + `libs/models/DailyNoteService` grow to use `MomentFormatter`, gain `{{folder}}` / `{{cursor}}` placeholders, and migrate config off KConfig onto vault-scoped `.obsidian/*.json`. `libs/storage/VaultConfig` grows typed accessors for `daily-notes.json` / `templates.json`. New `MomentFormatPreview` widget in `src/dialogs/` powers a live-preview label in the existing `SettingsDialog` Daily Notes / Templates tabs.

**Tech Stack:** C++20, Qt6 (`QDateTime`, `QLocale`, `QDate::weekNumber`, `QRegularExpression`), KDE KConfig (existing, for backwards-compat migration path), `Corbomite::VaultConfig` (Cluster B).

---

## Goal (expanded)

Today's gap: Obsidian users who copy a template from elsewhere (GitHub gist, forum post, other vault) and drop it into their Corbomite vault see garbage output. They expect `{{date:YYYY-MM-DD dddd Do [at] HH:mm}}` to render `2026-04-15 Wednesday 15th at 14:30`. Corbomite's current `TemplateService` uses *Qt format tokens* (`yyyy-MM-dd dddd d` etc.), silently producing wrong output — or more often, doing nothing because Qt doesn't recognise `YYYY`, `Do`, `ww`, etc.

Three independent problems bundled into one cluster because they share the format-token decision:

1. **Moment-format compat.** Templates and daily-note filenames both use Moment tokens.
2. **Vault-portable config.** Daily Notes config lives in `.obsidian/daily-notes.json` in Obsidian; currently Corbomite stores it in `~/.config/corbomiterc` (KConfig). Opening a vault in Obsidian *then* Corbomite should produce identical daily-note paths/formats.
3. **Extended placeholders.** `{{folder}}`, `{{cursor}}`, `{{title}}` (already present) are the common non-date substitutions. `{{cursor}}` specifically needs cursor-marker preservation through the editor insertion path.

## Audit references

- **Moment.js exposure:** `domains/leaf-utilities.md §1` (`utils/moment.js`) — Obsidian re-exports `window.moment` verbatim; plugins import from the `"obsidian"` package and expect global locale. Format-token compat with Moment is required.
- **MomentFormatComponent widget:** `domains/ui-bundle.md §1` (sub-section `components/MomentFormatComponent`) — settings widget that lets users preview their format string against a live date.
- **Template placeholders:** `01-markoff-gaps.md` Per-document interactivity section — `{{date:YYYY-MM-DD}}` placeholders rely on Moment formatting.
- **Daily Notes / Templates as internal plugins:** `domains/core.md §7` — both are entries in the 31-plugin internal-plugin loadPlugin registry. `.obsidian/daily-notes.json` and `.obsidian/templates.json` are the canonical config files.
- **`VAULT-FORMAT.md §1`** — VaultConfig unknown-key preservation principle (apply to both new file types).
- **`testvaults/obsidian-help/en/Plugins/Daily notes.md` + `Templates.md`** — user-facing behavioural spec (folder, date format, template file location, time format).

## Moment strategy decision (resolved)

**Strategy B — hand-translate tokens to QDateTime + custom helpers.** Rationale from the Explore report:

- **QDateTime covers ~79% of Moment tokens natively.** YYYY, MMM, MMMM, dddd, ddd, HH, mm, ss, a/A etc. all map cleanly to `QDate/QTime::toString` format tokens (case-adjusted: `yyyy`, `MMM`, `MMMM`, `dddd`, `ddd`, `HH`, `mm`, `ss`, `ap`/`AP`).
- **5 tokens + 1 syntax feature need custom helpers:**
  - `Do` — ordinal day (1st, 2nd, 3rd, 21st). Locale-aware. Hand-written table for EN; fallback to suffix rules for other locales.
  - `ww` — ISO week of year, padded 2 digits.
  - `w` — ISO week of year, unpadded.
  - `[literal]` — escape-bracket syntax. Parser preprocesses format string.
  - Unrecognized tokens pass through verbatim (matches Obsidian behaviour — `YYYY-foo` renders `2026-foo`).
- **Zero new runtime dependency.** No QJSEngine, no moment.min.js bundle.
- **Fits existing codebase pattern** — `TemplateService` already does regex-based token replacement with `QDateTime::toString`. This is the same pattern, expanded to cover the Moment tokenspace.
- **Effort: ~1 week** for MomentFormatter + 100% token test coverage.

Strategy A (QJSEngine + moment.js bundle) rejected: adds 65KB resource + ~1ms per call + a new architectural seam (JS engine lifetime, locale state, engine init cost) for no fidelity gain beyond the `Do` / `ww` / `[literal]` gaps that Strategy B closes anyway.

## Target classes

| Class | File | Notes |
|---|---|---|
| `Corbomite::MomentFormatter` | `libs/core/src/MomentFormatter.{h,cpp}` (new) | Free-function `format(const QDateTime &, const QString &momentFormat, const QLocale &locale = QLocale()) → QString`. Plus file-local helpers: `ordinalSuffix(int day, QLocale)`, `isoWeekOfYear(QDate)`, `expandEscapeBrackets(QString)`. ~300 LOC |
| `Corbomite::TemplateService` (extend) | `libs/models/src/TemplateService.{h,cpp}` (existing) | Route `{{date:FMT}}` + `{{time:FMT}}` through MomentFormatter instead of `QDate::toString`. Add `{{folder}}` (from note's parent folder relative to vault root), `{{cursor}}` (preserved literal marker the editor-insert code locates and removes). Config source stays `.obsidian/templates.json` via VaultConfig |
| `Corbomite::DailyNoteService` (extend) | `libs/models/src/DailyNoteService.{h,cpp}` (existing) | Read config from `.obsidian/daily-notes.json` (via VaultConfig) instead of `CorbomiteSettings`. Format date via MomentFormatter. Handle nested-folder date formats (e.g. `YYYY/MMMM/YYYY-MM-DD` → auto-created directory tree). On first-open of a vault with no `.obsidian/daily-notes.json`, fall back to CorbomiteSettings values + write them to vault config |
| `Corbomite::VaultConfig` (extend) | `libs/storage/src/VaultConfig.{h,cpp}` (existing) | Add `readDailyNotesJson() / writeDailyNotesJson()` and `readTemplatesJson() / writeTemplatesJson()`. Unknown-key preservation matches existing typed accessors |
| `Corbomite::MomentFormatPreview` | `src/dialogs/MomentFormatPreview.{h,cpp}` (new) | `QWidget` with a `QLabel` showing live-preview of a format string applied to `QDateTime::currentDateTime()`. Updates on format-string edit. Used by SettingsDialog |
| `Corbomite::SettingsDialog` (extend) | `src/dialogs/SettingsDialog.{h,cpp}` (existing) | Daily Notes tab + Templates tab grow a `MomentFormatPreview` below each format-string QLineEdit |

`CorbomiteSettings` (`corbomite.kcfg`): keep existing daily-note fields for backwards-compat; add a one-shot migration at `DailyNoteService::open(vaultRoot)` that reads KConfig values if `.obsidian/daily-notes.json` is missing and writes them through. Once the vault has a JSON file, KConfig is ignored.

## KDE / GPL3-compatible prior art

**Local KDE source convention:** the KDE source tree is checked out locally at `~/src/kde/src/<repo>`. **Always grep there first; never clone from `invent.kde.org` unless a repo is genuinely missing locally.**

| Target | Local path | What we're looking for |
|---|---|---|
| **Ordinal suffix rules** | `~/src/kde/src/kcalendarcore/` + `~/src/kde/src/messagelib/` | Any place KDE renders "1st / 2nd / 3rd" day-of-month labels. Locale-aware suffix tables |
| **ISO week-of-year** | `~/src/kde/src/kcalendarcore/src/icalformat_p.cpp` | iCal format emits ISO week numbers — direct reference for Qt-based ISO-week extraction |
| **Live preview widget** | `~/src/kde/src/kate/addons/` (session switcher with live filter) + `~/src/kde/src/kdevelop/` (code template preview) | Pattern for "edit field + updated preview label" — standard Qt but existence in KDE confirms idiom |
| **Atomic file mutation for config** | Already harvested — `Corbomite::VaultConfig` (Cluster B) owns this |

## Work breakdown

**Phase 1 — MomentFormatter:**

1. Create `libs/core/include/corbomite/core/MomentFormatter.h`. Declare `QString Corbomite::MomentFormatter::format(const QDateTime &dt, const QString &momentFormat, const QLocale &locale = QLocale())` as a static method on a `class MomentFormatter` (file-local helpers are `.cpp`-only).
2. Implement `libs/core/src/MomentFormatter.cpp`:
   - Tokenizer pass 1 — handle `[literal]` escape brackets. Replace each `[...]` substring with a placeholder sentinel (e.g. `\x01<index>\x02`); keep a parallel list of the escaped contents. After format expansion, substitute sentinels back.
   - Tokenizer pass 2 — walk the format string left-to-right with a longest-match-first token table. Known tokens and their QDateTime-format equivalents:
     ```
     YYYY → yyyy, YY → yy,
     MMMM → MMMM, MMM → MMM, MM → MM, M → M,
     DDDD → {ordinal day-of-year}, DDD → {day-of-year}, DD → dd, D → d, Do → {ordinal day},
     dddd → dddd, ddd → ddd, dd → {2-char day name, locale},
     ww → {ISO week 2-digit}, w → {ISO week unpadded},
     HH → HH, H → H, hh → hh, h → h,
     mm → mm, m → m,
     ss → ss, s → s,
     SSS → zzz, SS → {2-char ms}, S → {1-char ms},
     a → ap, A → AP,
     X → {Unix timestamp seconds}, x → {Unix timestamp ms}
     ```
   - Non-token characters pass through verbatim. Unknown multi-char sequences pass through verbatim (Obsidian behaviour).
   - Dispatch: for tokens that map to QDateTime format, build a single `QDateTime::toString(fmt, locale)` call per contiguous run. For tokens needing helpers (`Do`, `ww`, `w`, `X`, `x`, `DDDD`, `DDD`, `dd`, `SS`, `S`), call the helper inline.
3. File-local helpers:
   - `QString ordinalSuffix(int day, const QLocale &locale)` — EN table `{1→"st", 2→"nd", 3→"rd", 21→"st", 22→"nd", 23→"rd", 31→"st", default→"th"}`. For non-EN locales, fall back to bare number (document as a known gap; Obsidian's fidelity here is also locale-sensitive).
   - `QString isoWeekOfYear(const QDate &d, bool padded)` — wraps `QDate::weekNumber()`.
   - `QString dayOfYear(const QDate &d, int width)` — `d.dayOfYear()` with padding.
   - `QString dayOfYearOrdinal(const QDate &d, const QLocale &locale)` — DDDD (ordinal day-of-year; rarely used but in spec).
   - `QString unixTimestamp(const QDateTime &dt, bool millis)` — seconds or ms since epoch.
4. Create `libs/core/tests/tst_momentformatter.cpp`. Tests (each as a `private Q_SLOT`):
   - `testEmpty` — empty format → empty output.
   - `testLiteralPassThrough` — `"hello"` → `"hello"` (no tokens).
   - `testYearTokens` — fixture date 2026-04-15, assert `YYYY` → `"2026"`, `YY` → `"26"`.
   - `testMonthTokens` — all four `M` / `MM` / `MMM` / `MMMM` with EN locale.
   - `testDayTokens` — `D`, `DD`, `Do` (assert 15 → "15th"), `dddd` (Wednesday).
   - `testOrdinalDaySuffixes` — 1st, 2nd, 3rd, 4th, 11th, 21st, 22nd, 23rd, 31st.
   - `testWeekOfYear` — `ww` and `w` for a date where ISO week is known (e.g. 2026-01-05 → week 2).
   - `testHourTokens` — 14:30 → `HH`:"14", `h`:"2", `a`:"pm", `A`:"PM".
   - `testMinuteSecondMs` — `mm`, `ss`, `SSS`, `S`, `SS`.
   - `testEscapeBrackets` — `"YYYY [at] HH:mm"` → `"2026 at 14:30"`.
   - `testNestedEscape` — `"[hello [world]]"` → `"hello [world]"` (greedy inner-match; verify against Obsidian docs behaviour — if ambiguous, document chosen semantics).
   - `testUnknownTokenPassesThrough` — `"ZZ"` → `"ZZ"` (not a Moment token; not consumed).
   - `testComplexRealistic` — `"YYYY-MM-DD dddd Do [at] HH:mm"` for fixture 2026-04-15T14:30:00 → `"2026-04-15 Wednesday 15th at 14:30"`.
   - `testUnixTimestamp` — `X` → known Unix seconds; `x` → known milliseconds.
   - `testMsTokens` — `SSS` / `SS` / `S` with millisecond fixture.
   - `testDayOfYear` — `DDD` / `DDDD` for Jan 15.
   - `testLocaleAwareMonthName` — `MMMM` with `QLocale(QLocale::French)` → `"janvier"`.
   - `testDateOnlyFormat` — no hour tokens → time part absent from output.
5. Register `tst_momentformatter` in `libs/core/tests/CMakeLists.txt`. Build + run: `cmake --build build && cd build && ctest -R tst_momentformatter --output-on-failure` — all 18 pass.
6. Commit: `feat(core): add MomentFormatter for Obsidian Moment.js format compat`.

**Phase 2 — VaultConfig typed accessors for daily-notes/templates.json:**

7. Audit schema. Open `~/src/` or any real Obsidian vault; if unavailable, reverse-engineer from `testvaults/obsidian-help/en/Plugins/Daily notes.md` + `Templates.md` + cross-reference `docs/obsidian-audit/domains/core.md §7`. Expected keys (document in VAULT-FORMAT.md addendum):
   - `.obsidian/daily-notes.json`: `{"format": "YYYY-MM-DD", "folder": "Daily Notes", "template": "templates/daily.md", "autorun": false}`.
   - `.obsidian/templates.json`: `{"folder": "templates", "date_format": "YYYY-MM-DD", "time_format": "HH:mm"}`.
8. Create addendum at `docs/obsidian-audit/addenda/2026-04-15-daily-notes-templates-schemas.md` documenting both schemas + unknown-key preservation expectation. Link from `docs/obsidian-audit/00-taxonomy.md` Addenda section.
9. Extend `libs/storage/include/corbomite/storage/VaultConfig.h`:
   ```cpp
   std::optional<QJsonObject> readDailyNotesJson() const;
   bool writeDailyNotesJson(const QJsonObject &obj);
   std::optional<QJsonObject> readTemplatesJson() const;
   bool writeTemplatesJson(const QJsonObject &obj);
   ```
10. Implement in `libs/storage/src/VaultConfig.cpp`. Trivial: delegate to the existing generic `readJson(fileName)` / `writeJson(fileName, ...)` methods. Unknown-key preservation inherited.
11. Extend existing `tests/storage/tst_vaultconfig.cpp` (or wherever VaultConfig tests live) with 4 tests:
    - `testReadDailyNotesJsonMissingFileReturnsNullopt`
    - `testRoundTripDailyNotesJsonPreservesUnknownKeys`
    - `testReadTemplatesJsonMissingFileReturnsNullopt`
    - `testRoundTripTemplatesJsonPreservesUnknownKeys`
12. Build + run. Commit: `feat(storage): VaultConfig typed accessors for daily-notes.json + templates.json`.

**Phase 3 — TemplateService migration to MomentFormatter + new placeholders:**

13. Modify `libs/models/src/TemplateService.cpp` `expandTemplate(...)`:
    - Replace `QDate::currentDate().toString(fmt)` with `MomentFormatter::format(QDateTime::currentDateTime().date().startOfDay(), fmt)` for `{{date:FMT}}`. Same for `{{time:FMT}}` → `MomentFormatter::format(...time(), fmt)`.
    - Default formats: `{{date}}` without a `:FMT` suffix → use `m_defaultDateFormat` (but now it's a Moment format, not a Qt format — update defaults from `"yyyy-MM-dd"` to `"YYYY-MM-DD"`).
    - Add `{{folder}}` substitution — derive from `noteTitle` (if the caller passed `"Daily Notes/2026-04-15"`, folder is `"Daily Notes"`) OR from a new `expandTemplate(content, noteTitle, folder)` overload; pick the simpler fit. Prefer the overload.
    - Add `{{cursor}}` substitution. Don't substitute here — leave the literal marker `{{cursor}}` in the output. Callers (MainWindow `insertTemplate`) will scan the inserted text for the marker and position the cursor there after insertion.
14. Modify `libs/models/include/corbomite/models/TemplateService.h` to add the overload + `{{cursor}}` behaviour.
15. Modify `TemplateService` config loading to read from `.obsidian/templates.json` via VaultConfig. New `initFromVaultConfig(VaultConfig &)` method. On init:
    - `readTemplatesJson()` → if present, pull `folder`, `date_format`, `time_format`.
    - If absent: fall back to current `CorbomiteSettings` values. On first save of any template-related setting, write to `.obsidian/templates.json` (migration).
16. Modify `src/app/MainWindow::insertTemplate` to:
    - Scan inserted text for `{{cursor}}` marker.
    - If found, remove the marker AND position the editor cursor at its location.
17. Update `tests/models/tst_templateservice.cpp`:
    - Existing tests still pass (old Qt tokens in the defaults? Rewrite if needed to use Moment tokens per new default).
    - Add `testDateFormatUsesMomentTokens` — `{{date:YYYY-MM-DD}}` → correct output.
    - Add `testFolderPlaceholder` — `{{folder}}` with `expandTemplate(content, "Daily/2026", "Daily")` → folder subsumed.
    - Add `testCursorPlaceholderPreserved` — `{{cursor}}` stays in the output (the template service itself doesn't remove it; MainWindow does).
    - Add `testInitFromVaultConfigReadsTemplatesJson` — write a `.obsidian/templates.json` into a temp vault, construct TemplateService, call `initFromVaultConfig`, assert settings loaded.
    - Add `testInitFromVaultConfigFallsBackToKConfig` — when `.obsidian/templates.json` missing, reads KConfig defaults.
18. Build + run. Commit: `feat(models): TemplateService uses MomentFormatter + adds {{folder}}/{{cursor}} + .obsidian/templates.json config`.

**Phase 4 — DailyNoteService migration to `.obsidian/daily-notes.json`:**

19. Modify `libs/models/src/DailyNoteService.cpp`:
    - New `initFromVaultConfig(VaultConfig &)` method — reads `.obsidian/daily-notes.json`, pulls `format`, `folder`, `template`. Falls back to `CorbomiteSettings` if missing.
    - `todayNotePath()` uses `MomentFormatter::format(QDateTime::currentDateTime(), m_dateFormat)` for the date portion instead of `QDate::toString`. This enables nested-folder formats like `YYYY/MMMM/YYYY-MM-DD` → `2026/April/2026-04-15`.
    - `openOrCreateToday()`: when the nested date format includes `/` separators (e.g. `YYYY/MM/DD.md`), auto-create parent directories via `QDir::mkpath`. Document this behaviour.
    - On settings change (e.g. from SettingsDialog), write back to `.obsidian/daily-notes.json` via VaultConfig (and also to KConfig for non-vault-scoped fallback).
20. Modify `libs/models/include/corbomite/models/DailyNoteService.h` to add `initFromVaultConfig`.
21. Modify MainWindow vault-open path to call `dailyNoteService->initFromVaultConfig(m_vaultConfig)` after `m_vaultConfig` is loaded.
22. Update `tests/models/tst_dailynoteservice.cpp`:
    - `testTodayPathUsesMomentFormat` — set format to `YYYY-MM-DD`, assert path matches today's date in that format.
    - `testNestedFolderFormatAutoCreatesDirectories` — format `YYYY/MM/DD`, open today note, assert parent dirs exist.
    - `testInitFromVaultConfigReadsDailyNotesJson` — seed `.obsidian/daily-notes.json`, init, assert format/folder/template loaded.
    - `testInitFromVaultConfigFallsBackToKConfig` — no JSON file → KConfig defaults.
    - `testOpenOrCreateTodayAppliesTemplateFromVault` — set template path → `openOrCreateToday()` creates with that template's content expanded.
23. Build + run. Commit: `feat(models): DailyNoteService reads .obsidian/daily-notes.json + uses MomentFormatter`.

**Phase 5 — MomentFormatPreview widget + SettingsDialog wiring:**

24. Create `src/dialogs/MomentFormatPreview.{h,cpp}`:
    - `QWidget` with a `QLabel m_previewLabel` and a `QTimer m_currentTimeTimer` (optional — fires every second to keep the preview "live" against the current time).
    - Public: `setFormatString(const QString &)` — re-renders the preview label via `MomentFormatter::format(QDateTime::currentDateTime(), format)`.
    - Optional: `setSampleDate(const QDateTime &)` — preview against a fixed date instead of `now()`. Useful for templates tab (preview a sample date, not "right now").
    - ~60-80 LOC.
25. Modify `src/dialogs/SettingsDialog.cpp` `setupDailyNotesPage`:
    - Add a `MomentFormatPreview` below the "Daily note date format" QLineEdit.
    - `connect(lineEdit, &QLineEdit::textChanged, preview, &MomentFormatPreview::setFormatString)`.
    - Seed preview with current format value.
26. Add a `setupTemplatesPage` method (if not already present) with a preview for each of `templates.date_format` and `templates.time_format`.
27. Create `tests/dialogs/tst_momentformatpreview.cpp` (minimal — it's a thin widget):
    - `testPreviewUpdatesOnFormatChange` — set format `YYYY-MM-DD`, assert label text matches today's date.
    - `testPreviewHandlesInvalidFormat` — set format to a weird input, assert no crash.
    - `testSampleDateOverride` — setSampleDate(known), setFormatString, assert output matches.
28. Build + run. Commit: `feat(dialogs): MomentFormatPreview widget + wire into SettingsDialog Daily Notes/Templates`.

**Phase 6 — End-to-end manual smoke + documentation:**

29. Manual smoke test (document for human to run):
    - Open Corbomite dev build on a vault.
    - Settings → Daily Notes. Change date format to `YYYY/MMMM/YYYY-MM-DD`. Preview shows `2026/April/2026-04-15`. Save.
    - Action → Open Daily Note. Note created at `2026/April/2026-04-15.md` with auto-created folder hierarchy.
    - Open the note's file manager location; confirm the path matches.
    - Open a second vault that has a pre-existing `.obsidian/daily-notes.json` with different format → Corbomite uses that format (not the KConfig value from the first vault).
    - Create a template at `templates/Meeting.md` containing `# Meeting {{date:YYYY-MM-DD}}\n\n{{cursor}}\n\n## Attendees\n`.
    - Action → Insert Template → pick Meeting. Note body has `# Meeting 2026-04-15`, cursor is positioned on the empty line after the heading, no `{{cursor}}` literal remains.
30. Update `docs/obsidian-audit/VAULT-FORMAT.md` — add an "Implementation additions — 2026-04" heading listing the two JSON files Corbomite now reads/writes.
31. Commit (if any doc changes): `docs(audit): add daily-notes.json and templates.json to VAULT-FORMAT`.

## Explore-agent dispatch prompts

Both prompts were run during plan expansion; findings already integrated into the plan. If Phase 3-4 need a freshening during execution, re-dispatch with:

> **Prompt — Moment token edge cases.** Read `testvaults/obsidian-help/en/Plugins/Templates.md` + `Daily notes.md`. Extract a list of every Moment format token mentioned in examples. Cross-reference with my Phase 1 token table; flag any in the help docs that I haven't covered. Report gaps + suggested test cases.

## Definition of done

- `MomentFormatter::format` handles all ~24 Moment tokens + escape brackets + unknown-token pass-through. 18 unit tests green.
- `.obsidian/daily-notes.json` and `.obsidian/templates.json` round-trip via `VaultConfig` with unknown-key preservation.
- `TemplateService` supports `{{title}}`, `{{date:FMT}}` (Moment), `{{time:FMT}}` (Moment), `{{folder}}`, `{{cursor}}` (marker preserved for editor to position cursor).
- `DailyNoteService` reads config from `.obsidian/daily-notes.json` with KConfig fallback + migration; handles nested date formats with auto-directory-creation.
- `MomentFormatPreview` widget integrated into Settings → Daily Notes + Settings → Templates tabs; live-updates on format-string edit.
- All unit tests green. Manual smoke on a real vault (template insertion with `{{cursor}}`, daily note with `YYYY/MMMM/YYYY-MM-DD` auto-folder format) passes.
- Audit addendum at `docs/obsidian-audit/addenda/2026-04-15-daily-notes-templates-schemas.md` documents the two JSON schemas.

## Blocks / enables

- **Depends on:** Cluster A (frontmatter round-trip via `Markoff::YamlValue`), Cluster B (VaultConfig), Cluster I (MetadataCache — `{{folder}}` substitution ultimately reads from MetadataCache-maintained path indices).
- **Blocks:** nothing critical. User-facing feature parity.
- **Enables:** Templates and Daily Notes as Obsidian users use them. Removes the muscle-memory friction. A knock-on: plugins that ship templates become trivially portable between Obsidian and Corbomite.
- **Estimated effort:** 6 phases, ~2 weeks sequential or 1 week with subagent-driven parallelism (Phase 1 + 2 can run in parallel; Phase 3 + 4 serial after; Phase 5 last).

## Compat quirks preserved

- **Unknown tokens pass through verbatim.** `"YYYY-foo"` → `"2026-foo"`. Matches Obsidian behaviour (Moment's parser also passes unknowns through).
- **`Do` ordinals are EN-only** for Phase 1. Other locales fall back to bare number. Matches Obsidian's observed behaviour (Moment's locale files cover EN well; other locales are variable).
- **Escape brackets are greedy inner-match.** `[a[b]c]` → `a[b]c`. Document; verify against Obsidian's moment.js behaviour during Phase 1 testing.
- **`{{cursor}}` is preserved verbatim from TemplateService and removed by MainWindow.** Separation of concerns: TemplateService is side-effect-free; cursor positioning is UI responsibility.
- **Vault-config precedence over KConfig.** On vault open: if `.obsidian/daily-notes.json` exists, use it; else fall back to `CorbomiteSettings` AND write the current config to JSON (one-shot migration). After migration, KConfig is ignored for per-vault values.

## Notes on expansion and execution

- **Single-engineer sequential:** Phases 1 → 2 → 3 → 4 → 5 → 6. ~1-2 weeks.
- **Subagent-driven (preferred):** Phases 1 + 2 dispatchable in parallel (MomentFormatter has no dependency on VaultConfig changes; VaultConfig typed accessors don't depend on MomentFormatter). Phases 3 + 4 run sequentially after 1 + 2 (both consume MomentFormatter + VaultConfig). Phase 5 last (consumes both services). Phase 6 is pure manual + doc.
- **Audit-doc cross-checks before implementation:** re-read `testvaults/obsidian-help/en/Plugins/Daily notes.md` + `Templates.md` before Phase 2 (schema extraction fidelity).
- **Back-out plan:** if MomentFormatter's token coverage proves insufficient mid-Phase-3, pivot to Strategy A (QJSEngine + moment.js). Added cost: ~1 week extra + 65KB bundle. Unlikely — the Explore report's 95% coverage is concrete.
