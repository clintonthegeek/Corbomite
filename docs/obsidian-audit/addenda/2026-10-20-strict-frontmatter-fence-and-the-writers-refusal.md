# The shared frontmatter reader is now Obsidian's *writer* rule, and the writer refuses what it cannot splice

**Date:** 2026-10-20
**Discovered during:** the Markoff vNext card-editor integration (trajectory step 1, the re-pin to `e0b96bf8`)
**Extends:** [`2026-10-17-frontmatter-single-scanner-deliberate-divergence.md`](2026-10-17-frontmatter-single-scanner-deliberate-divergence.md) (the decision this amends) and [`2026-10-03-parsing-frontmatter-info-offsets-and-parser-divergence.md`](2026-10-03-parsing-frontmatter-info-offsets-and-parser-divergence.md) (the Obsidian writer/indexer split it rests on)
**Relevant plans:** [`superpowers/plans/2026-10-17-markoff-vnext-finish.md`](../../superpowers/plans/2026-10-17-markoff-vnext-finish.md)

## Finding

Markoff's shared reader (`Markoff::Document::extract`, used by the indexer, the
writer *and* the vNext document) is now **Obsidian's writer rule**: a closing
fence must be a complete `---` line (upstream `5c0e5c6a`, "require a complete
closing frontmatter fence"). The permissive rule the 2026-10-17 decision was
built on is gone, and it is gone for a stated reason — the plain prefix search
also accepted `----`, `--- ` and `---foo` as closers, "which silently cut the
body short on round-trip". The rule is now byte-identical to Obsidian's
`getFrontMatterInfo` (`^---(\r?\n)` opener, a line of exactly `---` as the
closer).

The 2026-10-17 decision made its *splice* conditional on the reader finding a
span ("splice in place … where the reader found a span"). Under the strict
reader there is no span for the trailing-fence class, so there is nothing to
splice and the writer's other standing invariant — **never create a second
`---` block** — has to be kept by *refusing* the write and keeping the bytes.
`FileManager::frontmatterUnusable` now refuses that class the way it already
refused an unparseable YAML block.

What changes for the user, per class:

| Class | Obsidian's indexer | Obsidian's panel | Corbomite before | Corbomite now |
|---|---|---|---|---|
| `---\n…\n---\n` | frontmatter | shows | frontmatter | frontmatter (unchanged) |
| `---\r\n…\r\n---\r\n` | **not** frontmatter | shows | frontmatter (deliberate divergence) | unchanged |
| `---\n…\n--- \n` | frontmatter | **shows nothing** | frontmatter + splice | **not frontmatter** — the panel now matches Obsidian's |
| `---\n…\n---foo\n` | frontmatter | shows nothing | frontmatter + splice | not frontmatter |

So the properties panel is now **exact** against Obsidian's panel for both
classes (it reads the indexer path, Obsidian's reads the writer path), and two
costs appear, both of them the ones the 2026-10-17 addendum already named as
the price of a strict reader: the frontmatter tags and links of a
trailing-fence note leave the search index and the graph, and a rename leaves
a stale link inside such a block.

## Why noticed now

The card-editor swap needed the Markoff re-pin, and the re-pin brought the
strict fence with it. The host's own test caught the collision immediately: the
pinned "splices the trailing-space fence instead of prepending" case went red
because the reader no longer finds a block to splice.

## Action taken

One shared reader, still upstream's — no host-side fork of the fence rule, so
the 2026-10-17 "one reader" invariant stands. The writer refuses the write it
cannot splice (`carriesRejectedFenceBlock` in `libs/vault/src/FileManager.cpp`),
which is a guard, not a second scanner: it never computes a span and never
parses YAML, it only asks whether a fence-like line follows the opening fence
that the reader rejected. Pinned by
`tst_file_manager_frontmatter::refusesTrailingSpaceFenceInsteadOfPrepending`
(the bytes are kept exactly, and there is still only one block) and
`::prependsToNoteWhoseBodyStartsWithThematicBreak` (the guard's boundary: a
note whose body opens with a thematic break has no block to reject, so the
writer still prepends one).
