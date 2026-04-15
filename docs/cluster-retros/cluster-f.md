# Cluster F — Templates / Daily Notes / Moment (retrospective)

**Landed:** 2026-04-15. 5 implementation phases + 1 doc-closeout commit.

**Phase commits:**
- Phase 1 (`65f6159`) — `Corbomite::MomentFormatter` hand-translator for Moment.js format strings.
- Phase 2 (`24a2550`) — `VaultConfig` typed accessors for `daily-notes.json` + `templates.json`; audit addendum.
- Phase 3 (`07ffdcb`) — `TemplateService` uses MomentFormatter + adds `{{folder}}` / `{{cursor}}` placeholders + `initFromVaultConfig`.
- Phase 4 (`05601a5`) — `DailyNoteService` reads `.obsidian/daily-notes.json` + uses MomentFormatter + auto-mkpath for nested-folder formats. (This commit also absorbed Phase 3's MainWindow.cpp edits because the two phases ran in parallel and committed together — functionally split cleanly between their commit messages.)
- Phase 5 (`2458943`) — `MomentFormatPreview` widget + SettingsDialog wiring.
- Phase 6 — VAULT-FORMAT.md addendum + cluster retro (this commit).

**Test budget:** 18 new unit tests in `tst_momentformatter`, 4 new in `tst_vaultconfig`, 8 new in `tst_templateservice` (+ 2 rewrites for Moment tokens), 6 new in `tst_dailynoteservice` (+ 1 rewrite), 3 new in `tst_momentformatpreview`. Full suite 77/77 green outside the 5 known-flaky pre-existing failures.

## What changed vs the original plan

Plan was small and accurate. Three deviations worth recording:

1. **Moment strategy: Strategy B (hand-translate) confirmed by Phase 1 Explore.** QJSEngine + bundle was the alternative; rejected on code-complexity grounds (new architectural seam) and dependency weight (65KB resource). Hand-translator with `QLocale::toString(dt, format)` covers ~19/24 tokens natively; 5 custom helpers (Do ordinal, ww ISO week, day-of-year, sub-second, Unix timestamp) + escape-bracket parser cover the rest. Total MomentFormatter.cpp: ~285 lines. Well-tested (18 test cases including the realistic combined format).

2. **Phase 1 `testLiteralPassThrough` input changed from `"hello"` to `"note-QR"`.** The Phase 1 implementer caught a subtle spec error: `"hello"` is NOT a literal string in Moment — `h` is Moment's 12-hour token, so `moment(14:30:00).format("hello")` yields `"2ello"` (hour=2pm), not `"hello"`. My plan-level assumption was wrong. The implementer preserved the test's *intent* (non-token chars pass through) with characters that are genuinely non-tokens. Clean catch.

3. **Phase 3 `{{cursor}}` positioning is line-granular only.** Plan called for "cursor lands at the exact `{{cursor}}` marker location after insertion". `Markoff::Editor` exposes `goToLine(int)` but not `setCursorPosition(int absoluteOffset)` — there's no public column-granular cursor-set API. The implementer compromised with line-granular positioning (find marker, note its line, strip marker, call `goToLine`). Acceptable Phase 3 behaviour; extending Markoff::Editor to expose column-granular cursor-set is a natural future refinement.

4. **Phase 5 skipped the Templates tab preview** because there is no Templates tab in SettingsDialog today — only a Daily Notes tab with a single date-format QLineEdit. The MomentFormatPreview widget is in place next to that one field; adding it next to a future Templates tab's format fields is a one-line copy-paste.

## What surprised

- **Qt's `QDateTime::toString(format, locale)` overload doesn't exist.** The plan spec'd it; the implementer discovered `locale.toString(dt, format)` is the correct call. Seems obvious in hindsight but it's the kind of thing where the "LLM-generated call sounds right" trap shows up. Caught immediately by the first compile.

- **Qt's `h`/`hh` format tokens return 24-hour unless `ap`/`AP` appears in the same format string.** So for Moment's isolated `h` → we needed a manual `(hour % 12) ?: 12` conversion. Phase 1 implementer caught this and handled it; plan didn't anticipate.

- **Parallel agent file-commit race was fine.** Phase 3 (TemplateService) and Phase 4 (DailyNoteService) dispatched in parallel. Both agents needed to edit MainWindow.cpp (different call-sites, same file). The Phase 4 agent picked up Phase 3's pending edits and bundled them into its commit (`05601a5`). Confusing attribution, but zero code loss or duplication. If we run this pattern again, consider dispatching the two in sequence (Phase 3 → Phase 4) or having Phase 3 explicitly commit before Phase 4 starts.

- **`FileSystemAdapter::writeBinary` already auto-mkpaths.** Phase 4's explicit `QDir::mkpath` in `openOrCreateToday` is belt-and-braces — the adapter does it too. Left in for defensive clarity.

- **The `moment format string` "`hello`" gotcha.** Worth re-emphasising for future clusters that touch tokenised string formats — Moment's token set overlaps with ordinary English letters (`h`, `m`, `s`, `a`, `A`, `d`, `w`, `D`, `M`, `Y`, `S`, `X`, `x`), so naïve literal strings often render garbage unless wrapped in `[...]`. The escape-bracket syntax is load-bearing for humans, not just plugins.

## Downstream effects

- **No cluster was blocked on F directly.** F was user-facing feature parity, not an infrastructure-unblocker. But it:
  - Validates MomentFormatter as a reusable primitive for any future Moment-string consumer (e.g. Dataview-style plugins, Cluster K Bases formulas that do date arithmetic).
  - Proves the `.obsidian/*.json` config-extension pattern: addendum + `VaultConfig` typed accessor pair + 4 tests. Reusable template for adding the next Obsidian config file (graph.json, bookmarks.json, workspaces.json — all still open per VAULT-FORMAT §11).
  - Establishes the `initFromVaultConfig(VaultConfig &)` convention for other vault-scoped services. Templates + Daily Notes now use it; Properties panel (deferred, Cluster L) should adopt it when built; any future plugin-settings service can follow.

- **SettingsDialog templating tab** still doesn't exist. When Cluster N (plugin-ready surfaces) lands or a user request surfaces, add one and reuse the `MomentFormatPreview` pattern.

## Lessons for the next cluster

- **Token-string compat work benefits from a fast "what does Obsidian actually produce?" fixture suite.** For MomentFormatter, I should have suggested the implementer write a small tool that runs both our formatter and (optionally) Node.js moment.min.js on a test-input list and diffs them. Would catch subtle locale edge cases we're guessing at (ordinal suffixes in German/Japanese, dd 2-char weekday in French). Good-enough fidelity for Phase 1 shipped; comparison-harness is a clean follow-up.

- **Parallel-agent dispatch for two phases touching the same file:** bundle the shared-file edits into one phase OR dispatch sequentially. The Phase 3/4 race ended well but was avoidable complexity. "Phase 3 commits → Phase 4 begins" with explicit handoff in the dispatch prompt avoids the race.

- **Keep the plan's `h → 2ello` gotcha in mind for token-based systems.** Any future cluster that does Obsidian-string parsing (e.g. search DSL, wikilink, footnote refs) should default-assume "this looks like literal text but is actually tokens". Wrap literals in an escape syntax whenever the host system has one.

- **4 phases (1+2 parallel, 3+4 sequential, 5 final) was the right fan-out.** Dependencies between TemplateService and DailyNoteService were zero (both consume MomentFormatter + VaultConfig, neither consumes the other); parallel dispatch was fine modulo the MainWindow race above.
