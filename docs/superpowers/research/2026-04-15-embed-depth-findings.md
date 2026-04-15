# Obsidian embed-depth cap — findings for Cluster J

**Date:** 2026-04-15
**For:** Cluster J Phase 1 Task 1.3 — `EmbedDepthGuard::kMaxDepth` constant
**Plan reference:** `docs/superpowers/plans/2026-04-15-cluster-j-embed-rendering.md` Task 0.2
**Source grep:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/_internal.js`

## Constant value

**`5`** — the integer cap is hardcoded as the literal `5`.

## Comparison semantics

**Inclusive on the passing side; exclusive on rejection.**

Obsidian's `sJ.load({depth})` logic:
- Depth is incremented before the comparison: `e.depth = e.depth + 1` (line 627918).
- Condition: `if (e.depth <= 5)` → use the configured custom-embed creator.
- Otherwise → fall back to the generic `oJ` class (placeholder).

In Corbomite terms, `EmbedDepthGuard::allow(currentDepth)` should return `true` for `currentDepth < kMaxDepth` where `kMaxDepth = 5`. This matches the plan's original contract shape (reject at `>= kMaxDepth`); the increment-then-compare convention of Obsidian means the *first* embed (a direct `![[Note]]` in a root-level note) runs with `depth = 1`; the deepest reachable embed runs with `depth = 5`; any deeper nested embed runs with `depth = 6` and hits the fallback. Our `EmbedDepthGuard` using `allow(d) = d < 5` with the caller incrementing before the call matches this exactly.

## Placeholder behaviour at the cap

When `depth > 5`, Obsidian renders via the `oJ` class (lines 622458-622541 and 627764-627847), which produces a DOM element with:

- CSS classes `file-embed` plus either `mod-empty` (for missing notes) or `mod-empty-attachment` (for missing non-note files).
- Text content from the localised i18n bundle: `gm.plugins.pagePreview.labelEmptyNote()` or `labelEmptyAttachment()`.
- An `onClick` handler (line 622463) that opens the link target in a new pane.

Functionally: a clickable "broken embed" placeholder — users can tap to follow the link and open it as a standalone note. This matches what the Corbomite plan currently calls `EmbedDepthGuard::placeholder(targetLabel)`, but the Corbomite version should also make the placeholder *clickable* (navigate to the target on click) to match Obsidian's UX. This is a small scope addition to Task 1.3 that the current test (`testPlaceholderShape`) doesn't exercise — add a `testPlaceholderIsClickable` assertion during implementation.

## Source locations

| Concern | File | Lines |
|---|---|---|
| Depth increment | `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/_internal.js` | 627918 |
| Comparison check | same | 627926 |
| `JZ(containerEl)` ancestor counter | same | 609073-609079 |
| `oJ` fallback placeholder class | same | 622458-622541, 627764-627847 |
| `sJ.load()` method body | same | 627910-627937 |

## Configurability

**Not configurable.** The literal `5` is hardcoded; no `.obsidian/app.json` or other config key toggles it. A user-configurable cap is a valid post-parity follow-up for Corbomite; the plan already documents this as out-of-scope.

## Implementation recommendation

In `libs/core/include/corbomite/core/EmbedDepthGuard.h`:

```cpp
static constexpr int kMaxDepth = 5;
bool allow(int currentDepth) const { return currentDepth < kMaxDepth; }
```

Caller contract: increment `depth` before passing to `allow()` or to `EmbedRenderer::render()`. First-level embed has `depth = 1`, cap at `depth = 5`, reject at `depth >= 5` (the attempt to render the sixth level). This mirrors Obsidian's semantics exactly.

Also extend the plan's `Task 1.3` test set with:
- `testCap` asserting `kMaxDepth == 5` (literal — audit-confirmed).
- `testPlaceholderIsClickable` asserting the `MarkdownRenderChild` produced by the placeholder has a clickable target string accessible via `targetPath()`.
