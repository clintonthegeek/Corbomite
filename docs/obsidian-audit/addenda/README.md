# Audit addenda

> **Append-only directory.** When implementation reveals a fact about Obsidian that the audit didn't capture — or contradicts what the audit said — write a new file here rather than editing the original audit doc. The 15 Pass 2 domain docs and 5 Pass 3 synthesis docs are *snapshots* of what we believed at audit time; they cost ~25 hours of agent compute to produce and must remain stable as historical record.

## Corrections

The 2026-06-10 batch below is the first set of addenda that **refute** main-doc claims (all prior addenda were additive — new coverage, not contradictions). Per the frozen-doc rule the main docs remain unedited, so the wrong text is still sitting there looking authoritative. **Check this directory before implementing from:** `vault.md` §1/§8, `metadata.md` §2, `workspace.md` §2, `views.md` ViewState, `editor.md` expandText/`lP`, `00-taxonomy.md` QueryController, and the corresponding `VAULT-FORMAT.md` sections.

- [`2026-06-10-vault-path-and-naming-corrections.md`](2026-06-10-vault-path-and-naming-corrections.md) — `getAvailablePath` collision suffix starts at ` 1` not ` 2`; `unresolvedLinks` keys keep original casing; `normalizePath` never strips `./` (and does strip leading slashes); 250-**UTF-16-unit** (not byte) attachment truncation; all CachedMetadata offsets are UTF-16 code units.
- [`2026-06-10-workspace-serialization-corrections.md`](2026-06-10-workspace-serialization-corrections.md) — window nodes serialize bounds flat (no nested `size`); mobile-drawer node has no `width`/`collapsed` and writes `currentTab` unconditionally; `pinned` is written at both leaf-node and ViewState level; `group` is a sibling of `state` (views.md wrong); `hotkeys.json` records may carry a `code` field.
- [`2026-06-10-editor-timing-and-coverage-corrections.md`](2026-06-10-editor-timing-and-coverage-corrections.md) — `expandText` fires from a 10 ms-debounced CM updateListener (not `inputHandler`); the third `lP` regex is redundant with the *first* pattern (editor.md's `？` sentence is garbled); live-preview/table-editor/vim/DnD/find-replace engine is **unaudited** `_internal.js` territory mislocated by editor.md §14.
- [`2026-06-10-taxonomy-and-bases-corrections.md`](2026-06-10-taxonomy-and-bases-corrections.md) — `QueryController` does NOT parse the search DSL (taxonomy refuted; treat `00-taxonomy.md` as a routing index only); Bases `summaries:` key is double-handled into `unrecognizedData` then overwritten at serialize; bases.md Open Q2 stale (QueryController since extracted); 2026-04-17 bases addendum's corpus paths moved to `/home/clinton/bin/ObsidianRAW/audit/`.

## File naming

`YYYY-MM-DD-<short-topic>.md`

Examples:
- `2026-05-12-bases-formula-grammar.md`
- `2026-06-03-frontmatter-eof-edge-case.md`
- `2026-08-19-vault-config-app-json-extra-keys.md`

## File format

```markdown
# <Short title>

**Date:** YYYY-MM-DD
**Discovered during:** Cluster <X> Phase <N> implementation (or "exploratory spike", or "user report")
**Supersedes / extends:** `domains/<file>.md §<section>` (or "no prior coverage in audit")
**Relevant cluster plans:** `superpowers/plans/<file>.md` (list any plans that should cite this addendum)

## Finding

<one or two paragraphs describing the new fact, the corrected fact, or the previously unknown behaviour>

## Why noticed now

<one paragraph: what implementation work or question caused this to surface>

## Action taken

<one paragraph: what cluster plans were updated to cite this addendum, what compat behaviour was chosen, what tests were added>
```

## Linkage

After writing an addendum:

1. Append a one-line bullet to `docs/obsidian-audit/00-taxonomy.md` under the `## Addenda` section, sorted by date (most recent on top).
2. If the addendum **contradicts** a Pass 2 or Pass 3 claim, also update the relevant cluster plan's "Audit references" section to cite both. Do **not** edit the original audit doc.
3. If the addendum reveals a new gap or extension surface, append a bullet to `docs/obsidian-audit/01-markoff-gaps.md` or `02-extension-surfaces.md` under a `## Implementation additions — YYYY-MM` heading.
4. If the addendum surfaces a load-bearing decision, log it in `docs/PROJECT-STATE.md` Recent decisions.

## What is **not** an addendum

- Corbomite implementation choices (those go in cluster plans or `docs/cluster-retros/`).
- Build / test fixes (commit messages cover those).
- General notes-to-self (use a scratch markdown elsewhere).

Addenda are exclusively *new or corrected facts about Obsidian's behaviour*.
