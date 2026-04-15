# Audit addenda

> **Append-only directory.** When implementation reveals a fact about Obsidian that the audit didn't capture — or contradicts what the audit said — write a new file here rather than editing the original audit doc. The 15 Pass 2 domain docs and 5 Pass 3 synthesis docs are *snapshots* of what we believed at audit time; they cost ~25 hours of agent compute to produce and must remain stable as historical record.

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
