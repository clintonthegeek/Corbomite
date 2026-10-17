# Rename link-fixup: Corbomite splices literals instead of re-dumping frontmatter through js-yaml

**Date:** 2026-10-17
**Discovered during:** reconciling the `leaf-utilities` P1 (heading/block-id rename).
**Supersedes / extends:** the Corbomite-side half of
[`2026-10-03-leaf-utilities-subpath-rename-and-leaf-bundle-corrections.md`](2026-10-03-leaf-utilities-subpath-rename-and-leaf-bundle-corrections.md)
§NEW 3, which records Obsidian's `ZF` applier and asks Corbomite to decide the frontmatter
case and document it. Extends
[`2026-10-03-vault-filemanager-content-rewrites.md`](2026-10-03-vault-filemanager-content-rewrites.md)
(link-rewrite syntax preservation), which does not cover the frontmatter re-serialisation.
**Relevant cluster plans:** none — no production code changed; one regression test added.

## Finding

Obsidian's link-fixup applier `ZF` (`app/own/1304-ZF.js:2-32`) does two different things to
the two ref kinds it discriminates by shape:

- **body refs** (`hasOwnProperty("position")`): offset splices, applied descending.
- **frontmatter refs** (`hasOwnProperty("key")`): `yL` (js-yaml load) → `$F` set-at-dotted-path
  → `bL` (js-yaml dump) → splice over `[from, to)`. The **whole frontmatter block is
  re-serialised**, so every property the rename did not touch is re-emitted by the dumper.

Three consequences of that re-dump, all recorded in the 2026-10-03 addendum §NEW 3: a
top-level key containing a literal `.` silently no-ops (the dotted-path walk never reaches a
value); an unparseable block silently leaves the frontmatter links stale while the body
splices still apply; and the dumper's options govern untouched properties.

## What Corbomite does instead

`FileManager::renameFile` (`libs/vault/src/FileManager.cpp`) rewrites **both** kinds by literal
splice:

- body links/embeds: `LinkCache.position` spans, applied descending.
- frontmatter links (`frontmatterLinks`, which carry no position): the entry's `original`
  literal is located by searching the frontmatter span and every occurrence is spliced.

So Corbomite **does not re-dump** the frontmatter. Untouched properties are preserved
byte-for-byte, and a top-level dotted key is rewritten correctly (the literal is found
regardless of how the key is spelled). This is a deliberate divergence, in the same family as
the `processFrontMatter` payload-splice decision
(`2026-10-17-frontmatter-single-scanner-deliberate-divergence.md`): fidelity would reproduce
*reformatting*, which the divergence avoids at no observable cost.

## Where the behaviours still agree

An **unparseable frontmatter block** still yields the same partial update as Obsidian's
`ZF` catch path: the body link splices apply, the frontmatter links are left stale, and no
diagnostic is shown. Corbomite reaches this by a different route — `MetadataParser` produces
no `frontmatterLinks` for a block that will not parse, so the rewrite pass has nothing to
splice for the frontmatter while the body refs remain — but the disk result matches.
Pinned by `tst_file_manager_rename::renameWithUnparseableFrontmatterRewritesBodyOnly`, so a
future "fix" does not change the contract silently.

## Also relevant to the item

The item's headline flow — **renaming a heading or block id** and rewriting every matching
`#fragment` across the vault — **does not exist in Corbomite**. `stripHeading` is used only
for `resolveSubpath` navigation (the match side, correct); `stripHeadingForLink` is defined
and unit-tested but has no production producer. The `leaf-utilities` item is therefore a port
contract for an unbuilt feature, not a defect in existing code; it is re-scoped to P2.
