# Blank-line-collapse triage — `testvaults/starter-vault/PKM LM/Start Here.md` (68→33 lines)

**From:** Corbomite (downstream consumer)
**Date:** 2026-06-10
**Corbomite branch:** `feature/phase0-data-safety`
**Markoff pin:** `ddf5e9a8` (`v0.7.0-freeze-125-gddf5e9a8`)
**Design source (correct):** `libs/markoff-family/docs/specs/2026-05-18-b1-buffer-convention-design.md` §2 ("Save-time normalization")
**NOT the source:** `libs/markoff-family/docs/handoff/2026-05-21-save-path-data-loss.md` — that steer is about `toMarkdown()` reading an empty store (a data-loss bug), and says nothing about blank-line fidelity. See the correction in §"Is this the same bug" below.
**Triage verdict:** Corbomite/Markoff **round-trip written** (AST serializer, not hand-edit). The blank-line-run collapse is **intentional on Markoff's side** (B1 spec §2) **and** a confirmed Obsidian round-trip-fidelity gap that conflicts with a Corbomite dogfood release criterion. That tension is an **open decision** (two branches) flagged for the user/roadmap at the end of this doc — it is not closed here.

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

**No** — and the record needs a correction here. Two earlier steers are
sometimes reached for, but **neither is the design source for the
blank-line behavior**:

1. **`toMarkdown()` reading empty buffer** (`2026-05-21-save-path-data-loss.md`)
   — this steer is entirely about `toMarkdown()`/`toMarkdownUtf8()`
   reading the stale/empty legacy `d->buffer` store, silently writing
   empty files. It says **nothing** about blank-line fidelity. Fixed on
   the Corbomite side by routing the save through `serializeForSave()`.
   Citing it as the authority for "blank-line collapse is by-design" is
   incorrect — it is about a different bug.

2. **D2 not cleared on `resetContent()`/`loadFromMarkdown()`**
   (2026-05-25, `d2-clear-on-reset`) — content doubling. Fixed in
   Markoff at `f48525d` (`wipeD2State()` + `local_clear()` primitives).
   `f48525d` is an **ancestor of the current pin `ddf5e9a8`** (verified:
   `git merge-base --is-ancestor f48525d ddf5e9a8` succeeds), so the
   `E2E Test` doubling in `Start Here.md` is a **pre-fix artifact** —
   the file was written by an older session before the fix landed (or
   before Corbomite re-pinned past it). Not a live behavior at the
   current pin.

**The design source for the blank-line collapse is the B1 buffer
convention spec** —
`libs/markoff-family/docs/specs/2026-05-18-b1-buffer-convention-design.md`
§2 ("Save-time normalization", ≈ lines 114–132). It states the
behavior **explicitly and intentionally**:

> 1. **Runs of 2+ blank lines collapse to a single blank line.** This
>    is a side-effect of the canonical `interBlockSeparator() = "\n\n"`
>    … there is no path by which more than one blank line can be
>    reconstructed between two blocks.
> 2. **The document always ends with a single trailing `\n`.** …
>
> Both are intentional. Other Markdown editors (Pandoc,
> prettier-markdown, remark) do similar normalization on save. The cost
> is that **round-trip is not strictly byte-identical for files with
> irregular blank-line spacing** or no final newline; the gain is that
> the buffer convention is uniform and the serializer is one rule.

So the 11-blanks-→-1-blank collapse is the Markoff D2 serializer
(`serializeForSave()` in `MarkoffDocument.cpp`) emitting its canonical
`interBlockSeparator() == "\n\n"` between every pair of blocks: blank
lines are not AST/block nodes, they exist only in the source text, so
on re-serialization there is no path to reconstruct N > 1 blank lines.
This is by-design per B1 §2 — **and** it is the round-trip-fidelity gap
that the open decision below has to resolve.

---

## The conflict: by-design on one side, release-blocking on the other

Conclusion (c) — what to do about the blank-line collapse — is **not
closeable here**. It sits at the intersection of two facts that are
*both* true:

- **Intentional on Markoff's side.** Per B1 spec §2 (quoted above), the
  collapse is a deliberate, documented normalization, on par with
  Pandoc/prettier/remark, and B1 explicitly accepts that "round-trip is
  not strictly byte-identical for files with irregular blank-line
  spacing." From Markoff's vantage this is working as designed.

- **A confirmed Obsidian round-trip-fidelity gap on Corbomite's side.**
  A user who authored multi-blank-line spacing in Obsidian, opens the
  file in Corbomite, and saves it, gets a file that differs from the
  original. That conflicts directly with the dogfood release criteria:
  - "Obsidian round-trip: a week of alternating sessions produces no
    unexplained diffs" — `docs/superpowers/plans/2026-06-10-road-to-dogfood.md:147`.
  - "2+ weeks of live-vault dogfooding without a data-loss or interop
    incident" — `:146`.
  It also matches the queued action in `docs/punch-list.md:79`
  (NEEDS-TRIAGE, "Markdown open→save collapses blank-line runs"), whose
  triage precondition — *"confirm the dirty testvault files were
  Corbomite-written … before raising with Markoff"* — is now
  **SATISFIED** by the verdict above. So the queued action resolves to
  **raise it with Markoff / decide the policy**, not to close it.

The two facts do not cancel: "Markoff considers it by-design" does not
make the Obsidian-interop diff disappear, and the release criterion at
`:147` does not bend just because the diff has a documented cause. This
is a decision, presented below.

---

## OPEN DECISION — blank-line collapse (for the user / roadmap)

This is **not** a recommendation to close. It is a fork in the road
with two viable branches; picking one is the user's / roadmap's call.
Both are internally consistent; the difference is whether we spend
Markoff engineering on fidelity or amend the Corbomite release bar.

### Branch 1 — Steer Markoff: add a source-spacing-preserving save mode

Ask Markoff for a save mode that captures inter-block blank-line counts
at load time and re-emits them for **untouched** block boundaries (a
touched boundary may reasonably re-normalize). This restores
byte-identical Obsidian round-trip for the common case (open → read →
save with no edits, or edits localized to a few blocks).

**Correction to a tempting shortcut:** the existing `blockLoadTimeBytes`
fast path does **not** already do this. Reading `serializeForSave()`
(`MarkoffDocument.cpp`): for an untouched, non-setext, non-quoted block
the fast path emits `blockLoadTimeBytes` for byte-identical **block
content** (lines ~2340–2346), but the **inter-block separator is still
`interBlockSeparator() == "\n\n"`** (line ~2377) regardless of whether
the surrounding blocks were touched. So even a document where *no block
was edited* re-serializes every gap to exactly one blank line. A fix
must therefore specifically preserve **inter-block spacing**, not just
block content — e.g. record per-boundary blank-line counts in the load
ingress (`buildD2FromBytes`) and consult them in the separator emit when
both adjacent blocks are untouched. This is a real feature, not a flip
of an existing flag.

### Branch 2 — Accept + document the normalization as a known limitation

Consciously accept the B1 §2 normalization as Corbomite's behavior and:
- Document it as a known interop limitation in `docs/PARITY-MATRIX.md`
  and in the road-to-dogfood interop notes.
- Amend release criterion
  `docs/superpowers/plans/2026-06-10-road-to-dogfood.md:147` from "no
  unexplained diffs" to **"no *unexplained* diffs (blank-line-run
  normalization to a single blank line is an expected, documented
  diff)."**

This costs no Markoff work and keeps the serializer's one-rule
simplicity, at the price of telling dogfood users their irregular
blank-line spacing will not survive a Corbomite save.

### Either way

- **The D2-clear fix (`f48525d`) is confirmed in the current pin
  (`ddf5e9a8`)** — verified via
  `git merge-base --is-ancestor f48525d ddf5e9a8` (succeeds). So the
  `E2E Test` doubling in `Start Here.md` is a **pre-fix artifact**, not
  a live behavior; no re-pin is needed on its account.
- **The testvaults files remain uncommitted** (deliberate per Phase-0
  protocol). They are living evidence of the serializer-normalization
  and the pre-fix D2-clear doubling. Do not revert them; do not commit
  them. They will be cleaned up when the testvault harness gets a
  dedicated reset fixture.

**Status of this triage: OPEN.** The verdict (Corbomite-written) and the
root cause (B1 §2 intentional normalization) are settled. The
*disposition* — Branch 1 (steer) vs Branch 2 (accept + document) — is
deferred to the user / roadmap and is the one thing this doc does not
decide.

---

## Evidence summary (for the record)

| File | Before | After | Primary change |
|---|---|---|---|
| `Start Here.md` | 68 lines | 33 lines | Blank-line runs collapsed; `E2E Test` content appended (D2-clear artifact) |
| `Obsidian Setup.md` | no trailing `\n` | trailing `\n` added | B1 terminal newline normalization |
| `The Power of the Local Graph.md` | no trailing `\n` | trailing `\n` added | B1 terminal newline normalization |
| `Using Templates in Obsidian.md` | no trailing `\n` | trailing `\n` added | B1 terminal newline normalization |
| `films-vault/*.base` | `note.prop` syntax | `prop` syntax | `.base` schema migration, unrelated to Markoff serializer |
