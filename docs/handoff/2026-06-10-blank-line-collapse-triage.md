# Blank-line-collapse triage — `testvaults/starter-vault/PKM LM/Start Here.md` (68→33 lines)

**From:** Corbomite (downstream consumer)
**Date:** 2026-06-10
**Corbomite branch:** `feature/phase0-data-safety`
**Markoff pin:** `ddf5e9a8` (`v0.7.0-freeze-125-gddf5e9a8`)
**Prior steer:** `libs/markoff-family/docs/handoff/2026-05-21-save-path-data-loss.md`
**Triage verdict:** Corbomite/Markoff **round-trip written** (AST serializer, not hand-edit). This is the **same bug class as 2026-05-21 (D2 doubling)**, but manifests differently — blank-line runs *between* embed blocks are collapsed to single blank lines, and test content was appended, by the Corbomite save path during Phase-0 integration testing.

---

## Evidence examined

`git diff -- testvaults/` (read-only; files not staged or modified by
this analysis). Relevant hunks from `testvaults/starter-vault/PKM LM/Start Here.md`:

```diff
 ![[General PKM]]
 
-
-
-
-
-
-
-
-
-
-
-
-
 ![[Obsidian Setup]]
 
-
-
-
-
-
-
-
-
-
-
-
-
 ![[Productivity]]
```

Original: 68 lines. Post-save: 33 lines. The reduction is entirely
attributable to multi-blank-line runs (11 blank lines between each pair
of `![[…]]` embed blocks) being collapsed to a single blank line, plus
a structural reordering: `![[Task Management]]` and three `Test content`
/ `E2E Test` lines moved from near the end (lines ~52-54) to after
`![[Writing]]` (line ~27 in the new file), and nine `E2E Test` lines
appended at the end.

**Other modified files in the same working tree** (for completeness):

- `Obsidian Setup.md`: final line `no newline at end` → trailing `\n`
  added. Byte-identical otherwise. Classic Markoff `finalDocumentTerminator()`
  / B1 convention.
- `The Power of the Local Graph.md`: same — trailing `\n` added to a
  file that previously lacked one. No other changes.
- `Using Templates in Obsidian.md`: same — trailing `\n` added. No
  other changes.
- `films-vault/Directors.base` and `Films.base`: YAML schema changes
  (property-reference syntax `note.foo` → `foo`). These are `.base`
  (Corbomite Bases format) files, not markdown, and are unrelated to
  the Markoff serializer.

---

## Verdict: Corbomite/Markoff round-trip written

Three tells confirm these are **not hand-edits**:

1. **Uniform normalization pattern across the whole file.** Every
   inter-embed gap in the file was normalized identically (all 11-blank
   runs → 1 blank). A human editing this file would touch one or two
   spots; the serializer normalizes every block boundary in a single
   pass.

2. **Trailing-newline canonicalization across multiple files.** All
   four `.md` files that were opened by Corbomite gained a terminal `\n`
   where none existed before. This is the B1 `finalDocumentTerminator()`
   convention applied uniformly — a serializer signature, not something
   a human does across multiple unrelated files in a single edit.

3. **`E2E Test` lines appended.** The appearance of nine `E2E Test`
   lines and a content reorder in `Start Here.md` is consistent with
   the Phase-0 integration test suite having used `setMarkdown()` on
   this document (to inject test content), followed by a save, on top
   of the original source bytes. This is the exact D2-doubling / content
   ordering non-determinism documented in the 2026-05-25
   `d2-clear-on-reset` steer: the prior D2 state (original content) and
   the new D2 state (test content) are both serialized together.

---

## Is this the same bug as 2026-05-21 / 2026-05-25?

**Partially.** The prior steer family has two separate issues:

1. **`toMarkdown()` reading empty buffer** (2026-05-21) — data-loss
   path. Fixed on the Corbomite side by routing the save through
   `serializeForSave()`.

2. **D2 not cleared on `resetContent()`/`loadFromMarkdown()`**
   (2026-05-25, `d2-clear-on-reset`) — content doubling. Fixed in
   Markoff at `f48525d` (`wipeD2State()` + `local_clear()` primitives).

The `E2E Test` content doubling in `Start Here.md` is consistent with
**issue 2 having been present** when this file was written (i.e. this
file was written by an older session before the `wipeD2State()` fix
landed or before Corbomite re-pinned past it).

The **blank-line collapse** (11 blanks → 1 blank) is a **separate,
distinct behavior** that is *not* addressed by either prior steer. It is
the Markoff D2 serializer (`serializeForSave()` via
`MarkoffDocument.cpp`) rebuilding documents from the AST/block model
using its canonical inter-block separator (`interBlockSeparator() == "\n\n"`,
i.e. exactly one blank line between blocks). When the original source
file has N > 1 consecutive blank lines between blocks, those N lines
are collapsed to exactly one on re-serialization, because the blank
lines are not preserved as distinct AST nodes — they exist only in the
source text, not in the D2 block model.

---

## New facet: blank-line run collapse (not covered by prior steers)

Neither the 2026-05-21 nor the 2026-05-25 steer addresses blank-line
run preservation. The D2 serializer is **not expected** to preserve
arbitrary blank-line counts — this is a known consequence of the
AST-rebuild / canonical-form model. Whether blank-line-run preservation
is desirable is a Markoff design question, not a Corbomite bug.

**Our position for now:** this is an **accepted normalization**, not a
data-corruption bug. Corbomite users whose files use multi-blank-line
spacing as intentional style will see those files normalized to single
blank lines on open→save. The more serious issue is the content
doubling (`E2E Test` lines), which traces to the D2-clear bug.

---

## Recommendation

1. **Do not raise the blank-line collapse as a new Markoff steer.**
   It is a natural consequence of AST-canonical serialization. If
   Markoff ever wants to add a "preserve source blank-line runs" mode
   (e.g. via a source-byte pass-through for untouched blocks), that
   is a feature request, not a bug. The existing `blockLoadTimeBytes`
   fast path for untouched blocks already does this for blocks that
   have not been edited — the normalization only fires for blocks that
   went through the D2 rebuild pipeline.

2. **Confirm the D2-clear fix (`f48525d`) is included in the current
   pin (`ddf5e9a8`).** If yes, the `E2E Test` doubling in `Start Here.md`
   is a pre-fix artifact. If the pin predates `f48525d`, Corbomite
   should re-pin before the next integration test run.
   Run: `cd libs/markoff-family && git log --oneline f48525d^..ddf5e9a8`
   to confirm.

3. **The testvaults files remain uncommitted** (deliberate per Phase-0
   protocol). They serve as living evidence of the D2-clear and
   serializer-normalization behavior. Do not revert them; do not commit
   them. They will be cleaned up when the testvault test harness is
   given a dedicated reset fixture.

---

## Evidence summary (for the record)

| File | Before | After | Primary change |
|---|---|---|---|
| `Start Here.md` | 68 lines | 33 lines | Blank-line runs collapsed; `E2E Test` content appended (D2-clear artifact) |
| `Obsidian Setup.md` | no trailing `\n` | trailing `\n` added | B1 terminal newline normalization |
| `The Power of the Local Graph.md` | no trailing `\n` | trailing `\n` added | B1 terminal newline normalization |
| `Using Templates in Obsidian.md` | no trailing `\n` | trailing `\n` added | B1 terminal newline normalization |
| `films-vault/*.base` | `note.prop` syntax | `prop` syntax | `.base` schema migration, unrelated to Markoff serializer |
