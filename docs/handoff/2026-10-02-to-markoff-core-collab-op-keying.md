# Handoff → markoff-core devs: a collab op-keying bug that loses content, plus a lifetime gap, a missing undo query, and two APIs that always return empty

**From:** Corbomite (downstream consumer of `markoff-core`)
**Date:** 2026-10-02
**Corbomite HEAD:** `57e79fad`
**Markoff pin:** `5d8850369c91175fe51e3738602d5893a7db21b3`
**Severity (our view):** **P0** for §1 — reproduced content loss on the
collaboration path. **P1** for §2 (a crash class we have already been bitten by),
§3 and §4 (they force workarounds in our code, quoted inline). §5–§8 are P1/P2.
Nothing here blocks your roadmap.

We are your largest consumer, so this is a long list. It is one document because
the items share a theme: **several of them are cases where we had to build a
workaround in Corbomite, and we have quoted our workaround in each case** so you
can see the cost from the outside.

---

## 0. How we verified §1

We did not report it from a code skim. We built a two-document harness against
the compiled `libmarkoff_core.so` — the same shape as your existing
`tst_d5_two_doc_convergence.cpp` — and routed ops through
`localOpsProduced` → `applyRemoteOps`. The reproduction, the output, and the
minimal test we would like added are all in §1. If the reasoning is wrong we
would much rather know.

---

## 1. (P0) Collab op payloads are keyed without the `BlockId` — multi-block pastes lose and mis-attribute content

### The reproduction

Document A pastes 6 paragraphs via `applyFlatEdit`. Ops are routed to document B
through the normal op path. A and B both report 6 blocks:

```
A blocks=6   B blocks=6
   0  A='chunk0'  B='chunk0'
   1  A='chunk1'  B='chunk5'   <-- wrong content in the wrong block
   2  A='chunk2'  B=''
   3  A='chunk3'  B=''
   4  A='chunk4'  B=''
   5  A='chunk5'  B=''
```

`B.serializeForSave()` → `"chunk0\n\nchunk5\n\n\n\n\n\n"`. Two-thirds of the
pasted content is gone, and `chunk5` landed in block 1.

### The mechanism

`src/MarkoffDocumentPrivate.h:162`
```cpp
using PendingOpKey = QPair<quint8, Markoff::OpId>;   // no BlockId
QHash<PendingOpKey, QByteArray> pendingOpPayloads;
```

`src/MarkoffDocument.cpp:1380-1382`
```cpp
t.registerOp(UndoCrdtTarget::buffer(block), lamportToOpId(ts));  // target HAS the BlockId
d->pendingOpPayloads[{6, lamportToOpId(ts)}] = ...               // key drops it
```

`src/MarkoffDocument.cpp:285-288`
```cpp
QByteArray payload = d->pendingOpPayloads.take({targetTypeIndex(cop.target), cop.opId});
if (payload.isEmpty()) {
    continue;                       // silent drop, no warning
}
```

The BlockId is right there in `UndoCrdtTarget::buffer(block)` and is discarded
before the key is built. The key has two components and neither identifies the
block.

**Root cause:** every per-block `CollabText::Crdt::Buffer` has its **own**
`m_clock` starting at 0, and `OpId` is `(replicaId << 48) | lamportCounter`
(`:36-39`). So the *first* local edit in *every* block yields the identical
`OpId`. Two fresh blocks touched in one transaction collide on the key, and
`QHash::take` returns only one payload; the other is dropped by the
`isEmpty()` → `continue` path.

### Why no test catches it

All four cases in `tst_d5_two_doc_convergence.cpp` do exactly **one
`d2ApplyBufferEdit` per transaction** (`:60, :67, :76-82, :99, :114, :131, :139,
:156, :161`). One Buffer op per OpId, so the collision is structurally
unreachable. We suspect this is the single highest-value test you could add
today, because it would also protect anything else that multi-block edits touch.

### What we would like

1. Key on the target's full identity — e.g. `{targetTypeIndex, blockId, opId}`,
   or a per-`(targetIndex, blockId)` sub-map.
2. **Turn the silent `continue` into a `qCWarning`.** A dropped op is never
   acceptable in a collaboration path, and today it is invisible in every build.
3. Consider deriving one monotonic Lamport across the whole document in
   `applyFlatEdit` / `applyInteractiveNewline` / `replaceMatches` rather than
   relying on per-buffer clocks. That would close the class rather than the
   instance.
4. **Test:** one `applyFlatEdit` producing ≥2 blocks in a single transaction,
   routed through the ops path, asserting `assertConverged`.

### A related cost we are paying

`MarkoffDocument(quint16{1}, …)` sets `collabConfigured = true`
(`:342`) and installs the `onCommit` hook that encodes every op into
`pendingOpPayloads` (`:1382`, `:1432`, …) so it can be dropped again at
`:273-277`. Corbomite constructs exactly that
(`libs/core/src/NoteDocument.cpp:41-42`), so we pay the encode/decode on every
keystroke for a wire we never use — and we run this bug on every keystroke for
nothing.

We are aware `libs/core/src/NoteDocument.cpp:38-42` carries a
`TODO(port-foundation-exploration)` about the ctor shape. We would like to split
the flag from the replica id — e.g. `enum class CollabMode { Off, EmitOps }` plus a
ctor overload taking both, with `pendingOpPayloads` writes conditional on it.
That way a single-user consumer can keep a real replica id with op-emission off.

---

## 2. (P1) `MarkdownView` holds a raw `MarkoffDocument *` — and the fix already exists, in the retired leaf

### The gap

| Holder | Declaration | Crash path |
|---|---|---|
| `MarkdownView` | `MarkdownView.h:105` | `undo()`/`redo()` deref it — `MarkdownView.cpp:39-46` |
| `FindController` | `FindController.h:85` | `FindController.cpp:102-113`; the `if (!m_doc)` guard only catches null |
| `StyleApplier` | `StyleApplier.h` | `StyleApplier.cpp:60-64` `rerender()` |
| `StyledTableRenderer` | `StyledTableRenderer.h:23` | `isOpaque`/`renderOpaque` during any re-seed |
| `SourceTextDocumentBinding` | `.h:161,165` | `:160-163` disconnects from a freed sender |

None is a `QPointer`. None connects to `QObject::destroyed`.

### The fix is already in your tree

`markoff-live/src/EditorWidget.cpp:268-279` — and note the comment explains
exactly this:

```cpp
// Retire-on-destroy (INVARIANTS #3): the base MarkdownView holds the
// document by raw pointer (m_document). If the document is destroyed
// while still attached, null the base pointer so base accessors
// (cursorPosition/undo/redo/…) don't dereference freed memory.
d->docDestroyedCon = QObject::connect(doc, &QObject::destroyed, this, [this] {
    Markoff::MarkdownView::setDocument(nullptr);
});
```

`markoff-live` is retired (`MARKOFF_BUILD_LIVE=OFF`) and the two production
leaves never received this.

### Why we care so much

This is the exact crash class Corbomite hit on 2026-06-10: a first-run SIGSEGV
where a double `vaultRequested` tore the vault down while a live editor was
attached, held by a never-detaching raw pointer. We fixed it in our code and in
`markoff-live`. The production leaves still have it.

Any consumer that deletes a `MarkoffDocument` while a view is attached —
completely ordinary Qt — is one Ctrl+Z, one theme switch, or one mouse-move away
from a use-after-free.

### What we would like

1. Lift the `EditorWidget` pattern into `MarkdownView::setDocument` itself, so
   all leaves are fixed in one place rather than per-leaf.
2. **And** change `m_document` to `QPointer<MarkoffDocument>`, so `document()`
   returns null rather than dangling even if something misses step 1.
3. Same for `FindController::m_doc`,
   `SourceTextDocumentBinding::{m_markoffDocument, m_subscribedDoc}`,
   `StyleApplier::m_markoffDocument`.

**Test:** a document-destruction test for the production leaves, mirroring the
`markoff-live/tests/tst_live_render_doc_destroyed.cpp` that already exists for
the retired one. We note that `grep -rn "destroyed()"` across
core/source/styled finds **no** such connection outside `EditorWidget` — which is
why this survived.

---

## 3. (P1) There is no undo-availability query, so Corbomite's Undo/Redo lie

`MarkdownView` exposes `undo()`/`redo()` (`MarkdownView.h:65-66`) with **no**
availability query. `MarkoffDocument::undoDepth()` (`MarkoffDocument.h:105`,
impl `:414-417`) reads `d->buffer.undo_depth()` — the **legacy** stack, always 0
on a D2 document. The D2 `UndoLog::entryCount()` (`UndoLog.h:69`) exists but is
not surfaced.

### Our workaround, quoted so you can see the cost

`Corbomite/src/editor/MarkdownView.cpp:133-141`:

```cpp
bool MarkdownView::canUndo() const
{
    // Markoff::MarkdownView's contract exposes undo()/redo() actions but
    // no undo-stack-depth query — hasEditing() (editable at all) is the
    // best available proxy until an upstream API addition.
    return canEdit();
}
```

So our Undo and Redo actions are **enabled whenever the view is editable, even
with an empty stack, and they never grey out.**

### What we would like

```cpp
// MarkoffDocument
bool canUndoD2() const;
bool canRedoD2() const;
qsizetype d2UndoDepth() const;

// MarkdownView
virtual bool canUndo() const;   // default: document() && !isReadOnly()
virtual bool canRedo() const;
```

---

## 4. (P1) Three public accessors are permanently dead on a D2 document, with no deprecation

`MarkoffDocumentPrivate.h:66-69` — `latestParse`, `latestBlockAnchors`,
`latestBlockRanges` are **never assigned anywhere in the library**. Consequently:

- `parsedDocument()` (`:369-372`) always returns `nullptr`
- `blockAnchorAt()`, `blockByteRange()`, `blockAt()` (`:832-861`) always return
  `std::nullopt` after `loadFromMarkdown()`

The header documents them as ordinary working APIs. `MarkoffDocument.h:136` says
*"Returns the BlockAnchor for the top-level block at index `i` in the most-recent
parse"*; `:141-149` describes `blockByteRange` and `blockAt` as normal
operations.

This class of problem **did** get `[[deprecated]]` on
`toMarkdownUtf8()`/`toMarkdown()` (`:81-84`), so we think the treatment is
simply inconsistent rather than deliberate.

### Evidence of the cost

Your own `markoff-styled/CLAUDE.md` carries a section titled *"**D2-broken core
APIs to avoid**… When in doubt about a core API, search `latestBlockRanges`
first"*. A downstream implementer reading only the public header gets confident
wrong answers.

### Also

`blockByteRange` (`:842-845`) bounds its loop on `latestBlockAnchors.size()` but
indexes `latestBlockRanges[i]`. Both are empty today so it is inert, but the
guard is wrong — it should be `std::min`.

### What we would like

Either populate the three members in `materializeBlocksFromParsedDoc`, or mark
all four accessors `[[deprecated("…D2 documents are unindexed; use
findBlockAtSepByte / blockKind(blockAttrs())…")]]` and add
`bool isBlockIndexValid() const`. The deprecation route is smaller and matches
what you already did twice.

---

## 5. (P1) `onSaveComplete` waits forever for acks a single-user consumer never sends

`MarkoffDocument.cpp:2598-2609` only compacts when `ackedWatermark >= W`;
otherwise, for a `collabConfigured` document with no peers, it emits
`wantsAcksAtWatermark(W)` and **waits forever**.

Corbomite lands in exactly that branch: we construct `MarkoffDocument(quint16{1},…)`,
write files ourselves via `serializeForSave()`, and never call
`notifyAcksAtWatermark` or `triggerGc`.

**Consequence:** orphan per-block CRDT Buffers, BufferProxies,
`blockLoadTimeBytes` and `blockEditSequences` accumulate for every block ever
created, **forever**. `collectGarbage()` and `compact()` do not help — they touch
only `d->buffer`, the dead legacy flat buffer (`:969-977`).

### This compounds a per-keystroke cost

`d2EditSequence()` (`:1171-1177`) sums `blockEditSequences` over **every block
ever created**, and `scheduleD2Changed()` calls it on every edit. Because
`blockEditSequences` is only pruned by `disposeOrphans()` — which never runs, per
the above — **our per-keystroke cost grows monotonically for the life of the
document.** `NoteDocument::hasUnsavedEdits()` calls it a second time on every
`d2DocumentChanged`.

### What we would like

- Keep a running `quint64 d2EditSequenceTotal` incremented alongside
  `++d->blockEditSequences[id]`, and return that.
- In `onSaveComplete`, compact immediately when there are no peers; or make the
  single-user `MarkoffDocument(QObject*)` ctor the recommended one and have the
  watermark path apply only under a real collab config.
- Make `collectGarbage()`/`compact()` delegate to `WatermarkCoordinator`.
- **Document** that `save()` can drain Ctrl+Z. `undoLog.compact(all=true)`
  (`WatermarkCoordinator.cpp:53-54`) discards everything, and it is *tested as
  intended* (`tests/d2/tst_d2_gc.cpp:57-62`) — we are not asking for a behaviour
  change. But `save()`'s only doc is "Returns true on success" and
  `triggerGc()`'s is "Trigger GC manually". A consumer has no way to know.

---

## 6. (P1) `SearchEngine::FindFlags` advertises three flags that do not exist

`SearchEngine.h:27-33` declares `WholeWords`, `Regex`, `Backwards`.
`SearchEngine.cpp:25-26` guards them with `Q_ASSERT` — which **vanishes under
`NDEBUG`**. So in every user build:

- a "whole word" search silently returns substring matches
- a "regex" search silently does a literal search
- `Backwards` is not even asserted; it is silently ignored

`FindController.h:54-56` actively misdocuments this: *"Whole-words / regex are
accepted by the underlying SearchEngine but UI work is deferred."* **They are
not accepted.**

A consumer who reads the header and wires a whole-words toggle ships a search
that quietly lies.

**What we would like:** implement them, or remove them from the enum, add
`bool supports(FindFlag)`, and fix the comment. The removal route is smaller and
we would take that.

---

## 7. (P1) `SourceTextDocumentBinding` materialises the whole document ~6× per keystroke

`SourceTextDocumentBinding.cpp:456-462` (`onQtContentsChange`, i.e. per character
typed, plus every syntax-highlighter format-only change):

```cpp
const QByteArray preBytesSep = doc->widgetFlatView();      // iterateBlocks() + blockText() per block
const QString    preTextSep  = QString::fromUtf8(preBytesSep);
const QString postPlain       = m_textDocument->toPlainText();
const auto hitStart = findBlockAtSepByte(doc, sepStart, false);   // again
const auto hitEnd   = findBlockAtSepByte(doc, sepEnd,   true);   // again
```

`widgetFlatView()` is not a cached view: it is `iterateBlocks()` plus a fresh
`QByteArray::fromStdString(buf->text())` per block (`:1159-1164`) — one CRDT
buffer materialisation and heap allocation per block, repeatedly.
`handleStructuralKey` does it again at `:390`; `sepViewPosOf` (`:307`) converts
every block just to get its **length**, and is called from a function that walks
the document three more times.

On a 5000-block note this is O(document bytes) × ~6 allocations per character.

**What we would like:** (a) a cached flat view on `MarkoffDocument` invalidated
on edit-sequence change, used everywhere instead of the raw walkers; (b) a
block-index cache (`QVector<quint32> blockByteOffsets_`) so sep↔no-sep↔block
resolution is a binary search; (c) coalesce `onQtContentsChange` into one queued
handler so a burst per keystroke does one pass.

---

## 8. (P1/P2) Assorted

| Item | Location | Note |
|---|---|---|
| Process-global `static` BlockId counters | `MarkoffDocument.cpp:1066, 1420, 1338` | Function-local `static std::atomic`s, so **two open notes mint identical BlockIds** for structurally-inserted blocks. `allocateD2BlockId()` (`:1920`) is correctly per-document. Nothing compares IDs across documents today, but any host-side index keyed by BlockId — fold/collapse state, a remote-cursor registry, a persisted ephemeral-state map — will alias. Please move all three onto `Private`. |
| `FindFlags` aside, `findByBlock`'s case-insensitive path is O(len × matches) | `SearchEngine.cpp:53-64` | Every match re-serialises the whole block prefix to UTF-8 **twice** (`blockStr.left(u16idx).toUtf8().size()`). 2 000 matches in a 100 KB paragraph ≈ 4 000 allocations over 200 MB of copying. `FindController` runs this on **every keystroke** and re-runs on every `d2DocumentChanged` with no debounce. Suggest a running byte offset, and a ~150 ms debounce on `recomputeMatches`. |
| O(N²) structural operations | `:1446-1453`, `:1836-1841`, `:1653-1662`, `:1701-1710` | `d2InsertBlock`/`d2RemoveBlock` call `iterateBlocks()` on **every** block insert/remove just to compute a row, so **pasting N paragraphs is O(N²)**. `:1782-1830` — `replaceMatches` calls `applyFlatEdit` once per match, so **Replace-All is O(matches × N)**. Suggest letting `IdList` report the index directly, plus a single-transaction multi-range primitive. |
| `applyFlatEdit` materialises every block's text four times | `:1505-1510`, `:1536-1538`, `:1576`, `:1606`, `:1668-1669` | Each `blockText()` is a fresh CRDT materialisation plus a heap copy. One pass collecting `(blockId, size, cumulativeOffset)` at the top would do. |
| Export macros are no-ops while the libs are `SHARED` | `MarkoffCoreExport.h:4`, `MarkoffStyledExport.h:4` | `#define MARKOFF_CORE_EXPORT` is empty with the comment *"Foundation is built STATIC"* — but `markoff-core/CMakeLists.txt:17` is `SHARED`. Latent on Linux/GCC (everything exports by luck), but **not consumable on MSVC**, and it breaks the moment anyone adds `-fvisibility=hidden`. Also: `markoff-source` has **no** export header at all; `Markoff::UndoLog` — which consumers must instantiate to use the D2 API — has no macro, nor do `FormatOps`, `Cmd::*`, or the `Detail::` helpers. |
| `SearchEngine` is a `QObject` whose only useful member is `static` | `SearchEngine.cpp:10-16` | It exists to host `clearMatches(Session*)`, which has **zero** production callers. |
| `attachFindController`'s base no-op `qWarning`s on every call | `MarkdownView.cpp:32-36` | A consumer probing capability by attaching gets a warning per call. A `supportsFind()` query would be kinder. |
| No `THEME` operator== — restyle on every propagation | `Theme.h`, `MarkdownView.cpp:48-53` | `setTheme` is unconditional. Corbomite loops all three leaves on every theme change, forcing a full `StyleApplier::rerender()` + whole-document `FormatPass::apply` each time. `bool operator==(const Theme&) const` + a change gate would help both sides. |

---

## 9. Build and packaging

1. **Demo apps are built unconditionally.** `add_subdirectory(app)` is ungated
   in `markoff-source/CMakeLists.txt:54`, `markoff-styled:66`, `markoff-live:150`.
   `MARKOFF_BUILD_APPS` only covers `apps/`, so every consumer builds two demo
   apps it never installs.
2. **`markoff_live`'s PUBLIC include dir is a raw source path**
   (`markoff-live/CMakeLists.txt:121-124`). Commit `17a8376f` wrapped exactly
   this for canvas/source/styled and **missed live** — so flipping
   `MARKOFF_BUILD_LIVE=ON` breaks `install(EXPORT)` validation.
3. **No `install(DIRECTORY …/include)`.** The libraries install into Corbomite's
   `CorbomiteTargets` export set but **install no headers**, so there is no
   consumable SDK. Only an in-tree subdir build works.
4. **`markoff_core` links `collabtext` and `Markoff::Parser` as PUBLIC**
   (`:144-152`), and `MarkoffDocument.h:18-20` includes `<crdt/Anchor.h>`,
   `<crdt/Clock.h>`, `<crdt/Operations.h>`. Every consumer therefore inherits
   collabtext's include tree — while `markoff-core/CLAUDE.md` claims
   *"Public-boundary types (no `<crdt/...>` dependency)"*. **The claim is false at
   the header level**, and we suspect that is unintentional.
5. `markoff_live` requires Qt Quick **and Qt6Test** in the library's own
   `find_package` (`:9-11`), so `Qt6Test` becomes mandatory even for a non-test
   build.
6. **`FORCE`s a rapidoaml cache variable into the consumer's cache**
   (`libs/markoff-family/CMakeLists.txt:50`):
   `set(RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS ON CACHE BOOL "" FORCE)`. It is a
   legitimate rapidoaml option and rapidoaml already handles it correctly
   internally (`rapidyaml/CMakeLists.txt:99-100` translates it to
   `target_compile_definitions`), so the cache variable need never be forced —
   and `FORCE` silently overrides an explicit `-D`. Suggest moving it into
   `libs/markoff-parser/CMakeLists.txt` and dropping `FORCE`.

---

## 10. Test infrastructure

- **No `LABELS` on any test in core/source/styled/live.** We checked every
  `tests/CMakeLists.txt`: zero `set_tests_properties(... LABELS ...)`. So
  `ctest -L benchmark` cannot exclude anything. (We understand the canvas perf
  labels were already requested separately — same ask, wider scope.)
- **All four `tst_d5_two_doc_convergence.cpp` cases do one
  `d2ApplyBufferEdit` per transaction.** This is precisely why §1 survived. We
  think the multi-block-paste test is the highest-value test you could add.
- **No document-destruction test for the production leaves.** The retired leaf
  has `tst_live_render_doc_destroyed.cpp`; core/source/styled have nothing, which
  is why §2 survived.
- `perf_load_bench` is not registered with ctest at all
  (`libs/markoff-core/tests/CMakeLists.txt`, last stanza).
- `tst_foundation_search_engine.cpp` covers only plain case-insensitive
  substring — no `CaseSensitive` case — which is why §6's `Q_ASSERT` trap is
  untested.
- `UndoLog::lastEntry()` is public, returns `m_entries.back()` with no
  empty-guard, and hands out a reference into a vector `undo()`/`compact()`
  mutate. Only tests call it today, but it is reachable from any consumer via
  the public `d2UndoLog()`.
- `Markoff::Source::FindBar` is documented at `libs/markoff-source/CLAUDE.md:27`
  but does not exist (`grep -rn FindBar libs/markoff-source/` matches only that
  line). Probably a stale doc; cheap to delete.

---

## 11. One stale doc comment

`Origin.h` carries a comment that no longer matches the code — we flagged this in
our previous handoff (`2026-10-01-to-markoff-frontmatter-undo-and-test-labels.md`,
ask 2) and it is still outstanding. Not repeated in detail here; see that
document.

---

## 12. Suggested triage order

1. **§1** — has a reproduction attached; the multi-block test is the fix's proof.
2. **§2** — smallest safe change with the largest crash-class payoff, and the
   pattern already exists in your own retired leaf.
3. **§3, §4, §6** — each is a small API or deprecation change that removes a
   workaround we are carrying. §6's removal route is smaller than implementation
   and we would take that.
4. **§5** — the GC path and the `d2EditSequence` fix are independent; either
   alone stops the unbounded growth.
5. §7, §8, §9, §10 as capacity allows.

If any of this is wrong, we would rather hear than carry a workaround — we have
been wrong in this area before, most recently about a keyboard shortcut that was
missing a modifier. And if you disagree with §1's severity: the reproduction is
small enough to re-run in a few minutes, and we would welcome being shown we have
misread the op-routing path.