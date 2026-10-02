# Handoff → Markoff devs: three asks — a D2-undo-integrated frontmatter edit, a stale `Origin` doc comment, and `benchmark` labels on two canvas perf tests

**From:** Corbomite (downstream consumer of `markoff-core` / `markoff-canvas`)
**Date:** 2026-10-01
**Corbomite HEAD:** `802dc171`
**Markoff pin (current):** `5d8850369` (`archive/markoff-live-final-43-g5d885036`)
**Severity (our view):** P2 — ask 1 is a UX defect (a user action is silently
non-undoable); asks 2 and 3 are documentation/test-hygiene and cost minutes.
None is a data-loss or security issue. Nothing here blocks your roadmap.

This consolidates three separate items rather than sending three handoffs, and
**corrects two claims we made earlier** — see §5.

---

## 1. Ask 1 (the real one): a local frontmatter edit that participates in D2 undo

### The symptom

In Corbomite, `MarkdownView::insertFrontmatterProperty()` is the "Add file
property" command. It performs a *user-initiated* edit — inserting a blank row
into the frontmatter block — via:

```cpp
doc->setMarkdown(...);   // Corbomite's NoteDocument
  → markoff()->resetContent(bytes, Markoff::Origin::TestFixture)
```

Because `TestFixture` is a stack-clearing origin, adding a property row silently
discards the user's entire undo history for that note. There is no way to Ctrl+Z
back out of it.

### Why we cannot fix it in Corbomite

We tried. `Origin::UserRevertToSaved` looks like exactly the right origin and its
`Origin.h` comment even says *"pushes one mega edit so Ctrl+Z reverses it"* —
but **it does not work on the D2 path**, and we verified this rather than assuming
it. `resetContent()`:

```cpp
case Origin::UserRevertToSaved: {
    // Push one mega-edit that replaces the entire buffer with newContent.
    const auto curLen = d->buffer.visible_length();       // LEGACY buffer
    ...
    d->buffer.apply_local_edit(ranges, texts);
    break;
}
// ...outside the switch, unconditionally:
wipeD2State();
buildD2FromBytes(newContent);
```

So the mega-edit lands on the **legacy** CRDT buffer, while D2 state is torn down
and rebuilt *outside any undo transaction*. Since the D2 reshape, D2 is the only
stack the editing leaves record into — `applyFlatEdit`, `d2ApplyBufferEdit`,
`undoForBlock`, and `markoff-canvas`'s `View` all transact against `d2UndoLog()`,
and Corbomite routes Ctrl+Z to `undoD2()`. Net effect we measured: after such an
edit, `undoD2()` does not restore the previous content.

We wrote that one-line change, its regression test caught it, and we reverted it
rather than ship a placebo. Full record in our `docs/decisions-archive.md`.

The structural blocker: **frontmatter is not in the D2 block space.** It lives in a
separate `FrontmatterMap`, serialised by `serializeFrontmatter()` as
`"---\n" + raw + "\n---\n\n"` *before* the block loop. `applyFlatEdit`'s coordinate
space is "no-separator concatenation of `blockText(id)` for each block in
`iterateBlocks()`" — so there is no offset at which `applyFlatEdit` can reach
frontmatter at all.

The only public frontmatter methods today are `frontmatterValue()` (read) and
`applyRemoteFrontmatterMapOp()` (collaboration replay, not a local edit).

### What we would like

A **local** frontmatter edit that participates in D2 undo, e.g.

```cpp
/// Replace the frontmatter map and record it in the D2 UndoLog so a single
/// undoD2() reverts it. Mirrors the existing
/// MarkoffDocument::toggleListItemChecked(BlockAnchor) precedent, which is how
/// the identically-shaped checkbox-click gap was handled.
bool setFrontmatterMap(const FrontmatterMap &fm);
```

We do **not** need anything more than that. Specifically we do *not* ask for:

- CSS theme import, user `themes/*.json` directories, or the six
  `:/markoff/themes/*.json` built-ins — out of scope on our side (we have since
  reached a working minimum-viable `ThemeService` without them, using
  `defaultLight()`/`defaultDark()` + a QApplication-palette "follow system").
- Any change to the D2 block model or `applyFlatEdit`'s coordinate space.

If a general `applyMapOp` covering frontmatter + linkRef + footnoteDef is more
natural than a frontmatter-specific setter, that is fine too — we would consume it
either way.

**Acceptance we would write against:** set a frontmatter key, assert one
`undoD2()` restores the previous frontmatter byte-for-byte.

---

## 2. Ask 2: `Origin.h`'s `UserRevertToSaved` comment is stale

`Origin.h:14` documents `UserRevertToSaved` as *"pushes one mega edit so Ctrl+Z
reverses it"*. As shown in §1 that is true only of the **legacy** undo stack.
Against the D2 undo log — the one the editor leaves actually use — it is
inert. Please either correct the comment to say so, or make the implementation
match it. Today a downstream consumer can read that comment, do the obvious
thing, and ship a broken undo path; we did exactly that.

Related, from the same investigation: `Origin::ExternalReloadClean` and
`ExternalReloadResolved` are documented *"stack cleared"* and `Vault.cpp:273`
relies on that, but they too only clear the legacy stack — `wipeD2State()`
explicitly preserves `d->undoLog` (`MarkoffDocument.cpp:2178-2179`). We checked
whether this causes harm and **it does not**: `buildD2FromBytes()` re-parses into
fresh `BlockId`s and resets `d2EditSequence()`, so the preserved entries are stale
and `undoD2()` is a silent no-op. We pinned that behaviour in a test rather than
reporting it as a bug. It is a **low-severity** item on our side (Ctrl+Z does
nothing after an external change rather than undoing it), but it is another place
where the documented intent and the implementation have diverged, so it may be
worth a look alongside Ask 2. **No action required from us either way.**

---

## 3. Ask 3: `LABELS "benchmark"` on two canvas perf tests

`libs/markoff-canvas/tests/CMakeLists.txt` registers two wall-clock timing tests
with no label:

- `tst_canvas_perf_500` (:72-75)
- `tst_canvas_perf_formatted` (:77-80)

`libs/forcegraph`'s `tst_benchmark_layout` does the right thing
(`set_tests_properties(... LABELS "benchmark")`), so the difference looks like an
oversight rather than a policy. Under `ctest -j10` these two flake red from
machine load alone while passing standalone every time, which makes "the suite is
green" an unreliable signal for downstream consumers.

We cannot fix this from Corbomite: CTest test properties are directory-scoped, so
a parent-directory `set_tests_properties` cannot relabel a test defined in a
subdirectory, and patching the submodule would be lost on our next re-pin. Adding
the label is a two-line change on your side.

---

## 4. Status of our earlier `2026-06-10-to-markoff-frontmatter-fence-eol.md`

That handoff asked for an end-of-line check on the frontmatter closing-fence
search in `markoff-parser`'s `Document.cpp:42`. **It has not landed** — the pin
moved from `69162236` to `5d8850369` (~2 months) and the line is unchanged:

```cpp
source.indexOf(QStringLiteral("\n---"), 3)   // no EOL check
```

We are not asking you to prioritise it differently; we are flagging that our
"awaiting upstream fix + re-pin" status was stale — the re-pin happened and the fix
was not in it. If it was declined, a one-line "not doing this" would save us from
re-checking on every pin bump. (`----` and `---xyz` still falsely close
frontmatter.)

---

## 5. Corrections to two things we asserted earlier

Recording these because we were wrong and you should not take our earlier claims
at face value:

1. **We said the Properties add-row bug was fixable with `Origin::UserRevertToSaved`.**
   It is not — see §1. The request stands; the proposed mechanism did not.
2. **We previously reported that a clean external reload lets Ctrl+Z "resurrect the
   pre-reload buffer and silently discard the other tool's write."** The mechanism
   is real (the D2 log survives) but the consequence is inert (stale targets), so
   that was a UX wart, not data loss. Withdrawn as a bug; see §2.

---

## 6. Not asking, for the record

So the boundary is unambiguous:

- `markoff-source` and `markoff-styled` — untouched by this handoff. Your standing
  rule is bug-fix only for those, and we are not asking for anything.
- `markoff-live` — retired on our side (Cluster K Phase 5); we have no consumers.
- The E-arc regrouping, G1 accessibility, `pinned` at two levels, `group` sibling
  of `state` — all handled on our side; nothing needed from you.