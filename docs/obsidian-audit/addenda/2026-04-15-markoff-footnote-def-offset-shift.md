# Markoff strips footnote-def lines before tree-sitter parse — offset shift

**Date:** 2026-04-15
**Discovered during:** Cluster I Phase 2 (`MetadataParser` implementation)
**Supersedes / extends:** `domains/parsing.md` (no prior coverage of this behaviour); `domains/metadata.md §section-offsets`
**Relevant cluster plans:** `superpowers/plans/2026-04-15-cluster-i-metadatacache-parity.md`; relevant to any future Markoff AST-walk consumer (Cluster J embeds, Cluster K Bases, Cluster D property-call operator).

## Finding

`Markoff::Document::fromMarkdown()` removes footnote-def lines (lines matching `[^id]: ...`) from the raw source **before** handing it to tree-sitter. This means content offsets that tree-sitter emits for nodes *after* a footnote-def line are shifted relative to the original raw-source frame: they are correct in the footnote-stripped frame but wrong in the as-written frame.

The existing `frontmatterOffsetShift` mechanism only compensates for the YAML frontmatter block at the top of the file. There is no corresponding `footnoteDefOffsetShift` (plural, per-def) — and footnote defs can appear anywhere in the document, not just at the top or bottom.

Example (schematic):

```markdown
---
title: Foo
---
# First heading

body text

[^1]: This is a definition

# Second heading
```

In the raw-source frame, `# Second heading` begins at offset X. After frontmatter strip + footnote-def strip, Markoff's tree-sitter sees it at offset X - (frontmatter-bytes + footnote-def-bytes). Consumers that reach into the AST to get heading or block offsets and then try to slice the raw source at those offsets will get the wrong bytes.

## Why noticed now

Cluster I Phase 2 (`MetadataParser`) walks `Markoff::Document` to produce `CachedMetadata.headings`, `CachedMetadata.blocks`, `CachedMetadata.sections`. The Phase 2 implementer noticed the shift while writing offset-sensitive tests, and worked around it for the MetadataParser use case by emitting positions in the document-local (stripped) frame rather than the raw frame. Current Phase 2 tests don't exercise interleaved footnote-def + heading content, so no live bug — but the shift is a latent correctness issue for any future consumer that expects raw-source offsets.

Cluster I also noted footnote-def positions themselves are recovered via a regex scan of the raw source (not from tree-sitter), precisely because the parser has already discarded them.

## Action taken

- Addendum written to capture the behaviour.
- No code change yet; Cluster I's tests don't hit the interleaved case.
- **Recommendation for future clusters:** any consumer that needs raw-source offsets (e.g. Cluster J for section-granularity embed rendering, Cluster D for `[key:val]` property-call matching, Cluster K for Bases filter line-column reporting) must either (a) do its offset arithmetic in the stripped frame, or (b) compute `footnoteDefOffsetShift` by regex-scanning for `^[^id]:` lines and accumulating removed bytes.
- **Recommendation for Markoff:** expose a `Document::rawOffsetOf(strippedOffset)` helper, or equivalently a table of `(strippedRange, rawRange)` pairs representing every elision. Out of scope for now; track as a Markoff-API follow-up.
- Entry added to `01-markoff-gaps.md` under `## Implementation additions — 2026-04`.
