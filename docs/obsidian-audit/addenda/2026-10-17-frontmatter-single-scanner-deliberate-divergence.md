# Frontmatter: Corbomite keeps **one** permissive reader rather than reproducing Obsidian's two disagreeing scanners

**Date:** 2026-10-17
**Discovered during:** resolving the punch-list P0 that quotes
`2026-10-03-parsing-frontmatter-info-offsets-and-parser-divergence.md`.
**Supersedes / extends:** confirms the facts of that addendum (Corrections 1–4) but
**rejects its "Action taken" build requirement** — "a note-file frontmatter reader and an
AST frontmatter block rule must be implemented as two separate scanners … matching the
tabulated agreement/disagreement." Corbomite implements one scanner, deliberately.
**Relevant cluster plans:** none. The fix landed directly against the punch-list P0
(`FileManager.cpp`, `tst_file_manager_frontmatter.cpp`).

## Finding

`2026-10-03-…-parser-divergence.md` Correction 3 is correct about Obsidian: the writer
(`getFrontMatterInfo`) and the indexer (the hand-rolled AST block rule) are two scanners
that disagree. The table there stands:

| Input | writer (`getFrontMatterInfo`) | indexer (AST rule) |
|---|---|---|
| `---\r\ntitle: x\r\n---\r\n…` | frontmatter | **not frontmatter** |
| `---\ntitle: x\n---   \n…` | **not frontmatter** | frontmatter |
| `---\na: 1\n---b\n…` | **not frontmatter** | frontmatter |

The P0 that came from that addendum read as an instruction to reproduce both scanners in
Corbomite. That instruction was never reconciled with a decision already recorded in the
tree: `archive/2026-10-17/punch-list-2026-10-17-full.md:72` is a **closed `[x] P1** —
"Corbomite accepts CRLF frontmatter through one parser … unlike Obsidian, Corbomite does
not have a separate index scanner that rejects CRLF", pinned by
`tst_metadataparser::testCrlfFrontmatter`.

### Why reproducing the divergence is the wrong call here

1. **Reproducing it makes Corbomite's own properties panel diverge *more*, not less.**
   `PropertiesView` reads frontmatter from `MetadataCacheReader::frontmatterFor` — the
   **indexer** (`src/plugins/properties/PropertiesView.cpp:285`). Obsidian's properties UI
   reads through the **writer** scanner. Rejecting CRLF in the indexer would therefore
   make Corbomite show *no* properties for a CRLF note where Obsidian shows them — the
   opposite of parity. Matching Obsidian would need a *third* change (split the
   properties-read path onto a writer scanner), which is a cluster, not a punch-list box.
2. **The writer half is destructive.** Obsidian's writer returns `exists:false` for a
   fence with trailing text or spaces, so `processFrontMatter` takes its no-frontmatter
   branch and **prepends a second `---` block** (`app/own/1313-wR.js:7`). Copying that
   corrupts a note on a property edit. The repo already refuses this class of "match the
   bug" fidelity where it produces corruption (see the sanitiser divergence,
   `2026-10-03-rendering-sanitiser-deliberate-divergence.md`).
3. **A single permissive reader is self-consistent and lossless.** Accepting CRLF and a
   trailing-character fence everywhere means Corbomite never disagrees with itself and
   never destroys the user's block.

## The decision

- **One reader.** `Markoff::Document::extract` stays the only frontmatter scanner, for
  both indexing (`MetadataParser`) and writing (`FileManager`). It accepts `---\r\n` and
  a closing fence followed by trailing characters. This is a documented divergence from
  Obsidian's indexer.
- **The writer splices; it never rebuilds the fence.** `FileManager::processFrontMatter`
  and `setFrontMatter` now replace **only the YAML payload** and leave the pre-existing
  delimiter lines and every body byte identical (`spliceFrontmatter()` in
  `libs/vault/src/FileManager.cpp`). A note with a trailing-space closing fence is spliced
  in place, **not** duplicated.

## The real bug this exposed (fixed here)

The old writer path went through `Markoff::Document::withFrontmatter()`, which always
rebuilds `---\n…\n---\n`, and `markdownContent()` does not skip the `\r` after a CRLF
closing fence. So a property edit on a CRLF note rewrote its delimiters to LF **and**
injected a blank line:

```
in : ---\r\ntitle: x\r\n---\r\nbody\r\n
out: ---\ntitle: x\n---\n\r\nbody\r\n     ← before the fix
out: ---\r\ntitle: x\n---\r\nbody\r\n     ← after
```

This is Correction 2 of the 2026-10-03 addendum ("pre-existing delimiters are never
rewritten; only the YAML between them is re-serialised with LF"). It was unpinned by any
test. The three new cases in `libs/vault/tests/tst_file_manager_frontmatter.cpp`
(`preservesCrlfDelimitersAndBody`, `stripsCrlfBlockLeavingBodyByteIdentical`,
`splicesTrailingSpaceFenceInsteadOfPrepending`) fail against the old code and pin the
decision above.

## What this costs, stated plainly

Corbomite reads a CRLF vault's frontmatter where Obsidian's *indexer* would not — so a
CRLF note's frontmatter tags and links appear in Corbomite's search and graph but not in
Obsidian's. That is the deliberate divergence. It is not a format contract: nothing
Corbomite **writes** depends on it, because the splice preserves whatever delimiters were
on disk, so a Corbomite round-trip of a CRLF note is byte-identical apart from the YAML
payload Obsidian's own writer would also re-emit.

If strict fidelity is ever wanted, the cost is a cluster: implement both scanners **and**
split the properties-read path onto the writer scanner. It is not a punch-list fix.
