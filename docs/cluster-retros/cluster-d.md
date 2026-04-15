# Cluster D — Search / suggester parity (retrospective)

**Landed:** 2026-04-15, 5 commits (70f7d64 → f3367f7), single afternoon.

## What changed vs the original plan

The plan called for a `SearchPlan` class + a hand-written DSL parser. We landed both, but `SearchPlan` collapsed into a free `SearchDSL::compile()` function returning a plain `CompiledPlan` struct — no class needed once we saw how the executor and SearchPanel both wanted the same flat shape (`fts5Query`, `requiredTags`, `excludedTags`, `unsupported`). The `ResultHighlighter` deliverable shipped as a `drawHighlighted` paint helper rather than a `toDocument` `QTextDocument` factory; both candidate delegates (QuickSwitcher / CompletionPopup) already drew text char-by-char, so the cheaper paint helper was an exact fit. The KCommandBar palette wiring (Phase 3 plan item) was deferred — KDE's built-in fuzzy is serving it acceptably and the swap requires hooking into KCommandBar internals; deferred to PROJECT-STATE §follow-ups.

## What surprised

- **The "mid-word retry" reading was wrong.** A literal reading of the search.md §8 invariant ("strict-mode forces retry instead of penalty") made `mdf` → `getMarkdownFiles` impossible because the `d` in "markdown" is mid-word. The right reading turned out to be "prefer-boundary, fallback-to-first" — scan ahead for a boundary occurrence, and only fall back to the mid-word match if none exists. Confirmed by the audit's own characteristic-input example.
- **Compound operator names broke the tokenizer.** `match-case`/`task-todo` etc. all contain `-`, which the spec table classifies as `Tok::Not`. Only after `match-case:foo` parsed as `match` + NOT(`case:foo`) and threw "Operator 'case' not recognized" did the workaround become obvious: greedy-merge `-` into the text token when it's between two text chars (preserves `foo -bar` because space-then-`-` doesn't qualify).
- **The `searchCompiled` parameter shape.** I started designing a polymorphic `SearchPlan::execute(SQLiteIndex&)` interface and almost added the executor on top of an abstract result-fetcher. Talking myself out of that and just adding `SQLiteIndex::searchCompiled(QString fts5, QStringList required, QStringList excluded, int max)` saved ~80 LOC and one round of bikeshedding.
- **Test data containing the search term.** Lost 5 minutes to an integration-test failure where a "negative-control" note's content was `"#project but no foo"` — the literal "foo" substring made it match. Subtle reminder that test data needs to be checked against every assertion.

## Downstream effects

- **Cluster H (Menus / hover / suggester UI)** is now strongly enabled — the suggester substrate is shared and matchers are uniform. Quick-Switcher mode-switching (#/^/[[) belongs there.
- **Cluster I (MetadataCache parity)** stub-expansion is now timely; the `[prop:val]` operator follow-up wants to land alongside Cluster I's note_properties cache.
- **Cluster J (Embed/rendering)** unblocked for the `line:`/`block:`/`section:` operator follow-ups (those want markdown-AST post-filter).
- **Cluster G (Views hierarchy)** scouting doc unchanged; D doesn't touch view registration.

## Lessons for the next cluster

When porting a documented algorithm, do at least one characteristic-input dry-run on paper before committing to an interpretation. The "prefer-boundary, fallback-to-first" reading would have been obvious from a 90-second whiteboard pass; instead it cost a test-failure round-trip. Also: when designing an executor that needs to coordinate two libraries, prefer adding a method on the consumer side (here, `SQLiteIndex::searchCompiled`) over building a polymorphic protocol with a Fetcher interface — the protocol is easy to design and hard to throw away later.
