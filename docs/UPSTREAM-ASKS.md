# Upstream asks — Markoff family, Graffodil, and vendored deps

**Filed 2026-10-02.** Corbomite is the primary consumer of Markoff and Graffodil
and has write access to both (`clintonthegeek/Markoff`,
`clintonthegeek/Graffodil`). We can therefore ask for changes, not just work
around them.

---

## → The handoff document (this is the one to send)

This file is the **internal index**. The outward-facing artifact — a single,
self-contained document addressed to the Markoff maintainers, carrying every
open ask with its evidence — is:

**[`handoff/2026-10-18-to-markoff-consolidated-open-asks.md`](handoff/2026-10-18-to-markoff-consolidated-open-asks.md)**
— 39 numbered asks plus a table of smaller items, ordered P0 → P3: 7 × P0
(collabtext `assert()` guards and `Global::meet`; markoff-parser frontmatter race
and stale CSTs; Graffodil `BatchRenderer` teardown and edge endpoints;
markoff-core collab op keying), 19 × P1, 12 × P2, 1 × P3.

**Consolidated 2026-10-18 by user decision** — it replaces the per-library
handoffs as the live document. Those earlier files are **kept only as the record
of what was sent on which date**; do not add to them, and read this one to know
what is actually open:

| Sent | Document | Now carried as |
|---|---|---|
| 2026-10-02 | [`handoff/2026-10-02-to-collabtext-assert-guards-and-watermark-meet.md`](handoff/2026-10-02-to-collabtext-assert-guards-and-watermark-meet.md) | M1, M29 (+ M28) |
| 2026-10-02 | [`handoff/2026-10-02-to-markoff-core-collab-op-keying.md`](handoff/2026-10-02-to-markoff-core-collab-op-keying.md) | M2, M8–M13, M22, M26–M28, M34 |
| 2026-10-02 | [`handoff/2026-10-02-to-markoff-parser-frontmatter-race-and-incremental.md`](handoff/2026-10-02-to-markoff-parser-frontmatter-race-and-incremental.md) | M3, M4, M30–M32 |
| 2026-10-02 | [`handoff/2026-10-02-to-markoff-styled-theme-received-but-unused.md`](handoff/2026-10-02-to-markoff-styled-theme-received-but-unused.md) | M7, M20, M21, M23–M25 |
| 2026-10-02 | [`handoff/2026-10-02-to-graffodil-lifetime-theming-and-scale.md`](handoff/2026-10-02-to-graffodil-lifetime-theming-and-scale.md) | M5, M6, M14, M15, M19 |
| 2026-10-02 | [`handoff/2026-10-02-to-jkqtmathtext-and-rapidyaml-vendoring-hygiene.md`](handoff/2026-10-02-to-jkqtmathtext-and-rapidyaml-vendoring-hygiene.md) | smaller-items table |
| 2026-10-17 | [`handoff/2026-10-17-to-markoff-canvas-cards-and-session.md`](handoff/2026-10-17-to-markoff-canvas-cards-and-session.md) | M37, M38, M39 (numbered 2026-10-18); three of its five questions are closed by source evidence, recorded inside the asks |
| 2026-10-01 / 2026-06-10 | the frontmatter handoffs | M33, M34, M35 (swept in 2026-10-18) |

**New in the consolidated document** (not previously in this index — three
recovered from the handoffs above, one filed this week):

- **ASK-M33** — markoff-core: the frontmatter D2-undo edit path.
- **ASK-M34** — markoff-core: the stale undo comment in `Origin.h`.
- **ASK-M35** — markoff-parser: the frontmatter closing-fence match lacks an EOL
  check, so `----` / `--- ` / `---foo` close the block early.
- **ASK-M36** — markoff-parser: `tests/` is added with no gate, so a host cannot
  switch the leaf's 13 tests off; and `MARKOFF_CANVAS_BUILD_TESTS` defaults ON.
  Found by the `USER-TESTING.md` §11b build-hygiene check; Corbomite's half is
  fixed (`CMakeLists.txt` now FORCEs the canvas gate off).

Each ask states its severity, how the finding was verified, what it costs *us*
specifically, and a suggested fix shape — and the document says plainly where we
would rather be shown wrong than carry a workaround.

---

## Internal index (everything, in one place)

Every finding below was **verified against the pinned source** — the P0s were
re-checked by hand after a delegated audit, which is why they are stated with
`file:line` and quoted code rather than paraphrase. Anything a delegated pass
raised but that could not be confirmed is marked **UNVERIFIED** and should not be
filed as-is.

`mermaid` / `mmdr` is **deferred by user decision 2026-10-02** and appears only
in the "not asking" section.

---

## Priority 0 — provable correctness bugs, small diffs

### ASK-M1 · collabtext: `assert()` is the only guard on three anchor-lookup paths

**The single most severe finding. In a release build this is a heap OOB *write*
and a silently-lost delete.**

`libs/collabtext/src/crdt/IdList.cpp`

| Line | Path | Release behaviour |
|---|---|---|
| `:88` | `size_t idx = entries.size();` — "not found" sentinel | — |
| `:96` | `assert(idx < entries.size())` | compiled out |
| `:99` | `Lamport target_origin = entries[idx].origin;` | **OOB read** one-past-end |
| `:113` | `entries[idx].deletions.push_back(del_id);` | **OOB heap write** |
| `:40` | `insert_after`: `assert(found && …)`, then `Locator::between(predecessor, successor)` with a default-constructed predecessor | entry lands at an **arbitrary position** |
| `:478` | `apply_concrete(RemoveOp)`: `assert(found && …)` | — |
| `:480-483` | …then unconditionally `m_version.observe(op.timestamp); m_version.join(op.version);` and `return true` | the version vector now claims a delete that **never happened**, the deferred queue drops it so it is never retried |

The `:478` case is the nastiest: it is **self-declaring divergence**. `version()`
asserts "I have seen delete D" for a delete that was silently discarded, so any
future GC driven off `version()` may reclaim the insert D targeted — making the
divergence permanent and unrecoverable.

**Why it matters to us:** Markoff stores block order in `IdList` and feeds it
anchors from persisted document state. A stale anchor — a truncated autosave, a
merge of two files, a block whose origin never made it into the local list — is
an ordinary user action.

**Ask:**
1. Make the miss a first-class outcome. `std::optional<IdListOperation>
   remove_at(const Anchor&)` returning `std::nullopt` plus an unconditional
   `qCWarning`, checked **before any mutation**. Same for `insert_after`.
2. In `apply_concrete`, reorder so `if (!found) return false;` precedes every
   mutation — the existing deferred queue then retries it.
3. Demote the `assert`s to post-guard invariant assertions.
4. **Tests:** (a) remove an anchor not present; (b) a remove op delivered *before*
   its insert; (c) both under a release build in CI.

---

### ASK-M2 · Markoff core: collab op payloads keyed without the `BlockId` → multi-block pastes lose and mis-attribute content

**Reproduced end-to-end** against the built `libmarkoff_core.so`, using the
same two-document harness as the existing `tst_d5_two_doc_convergence.cpp`:
document A pastes 6 paragraphs via `applyFlatEdit`, ops routed through
`localOpsProduced` → `applyRemoteOps`. Document B converges to:

```
A: chunk0 chunk1 chunk2 chunk3 chunk4 chunk5
B: chunk0 chunk5 '' '' '' ''
```

**Two thirds of the pasted content is gone, and `chunk5` landed in block 1.**

`MarkoffDocumentPrivate.h:162`
```cpp
using PendingOpKey = QPair<quint8, Markoff::OpId>;   // no BlockId
```
`MarkoffDocument.cpp:1380-1382`
```cpp
t.registerOp(UndoCrdtTarget::buffer(block), lamportToOpId(ts));  // target HAS the BlockId
d->pendingOpPayloads[{6, lamportToOpId(ts)}] = ...               // key drops it
```
`MarkoffDocument.cpp:285-288`
```cpp
QByteArray payload = d->pendingOpPayloads.take({targetTypeIndex(cop.target), cop.opId});
if (payload.isEmpty()) { continue; }    // silent drop, no warning
```

**Root cause:** every per-block `CollabText::Crdt::Buffer` has its **own**
`m_clock` starting at 0, so the *first* local edit in *every* block yields the
identical `OpId` (`(replicaId << 48) | 1`). Two fresh blocks in one transaction
collide on the key.

**Why no test catches it:** all four existing `tst_d5_two_doc_convergence.cpp`
cases do exactly **one `d2ApplyBufferEdit` per transaction** — one Buffer op per
OpId, so the collision is structurally unreachable.

**Secondary cost to us:** we construct `MarkoffDocument(quint16{1}, …)`, so
`isCollabConfigured()` is `true` and we pay the payload encode/decode on every
keystroke for a wire we never use — running this buggy path for nothing.

**Ask:**
1. Key on the target's full identity — e.g. `{targetTypeIndex, blockId, opId}`,
   or a per-`(targetIndex, blockId)` sub-map.
2. **Turn the silent `continue` into a `qCWarning`.** A dropped op is never
   acceptable.
3. Consider deriving one monotonic Lamport across the document in
   `applyFlatEdit` / `applyInteractiveNewline` / `replaceMatches` rather than
   relying on per-buffer clocks.
4. **Test:** one `applyFlatEdit` producing ≥2 blocks in a single transaction,
   routed through the ops path, asserting convergence.
5. **Separately:** split `CollabMode` from the replica id so a single-user
   consumer can keep a real replica id with op-emission *off*.

---

### ASK-M3 · markoff-parser: `Document::parsedFrontmatter()` is an unsynchronised cache that publishes an empty result

`libs/markoff-parser/src/Document.cpp:382-394`
```cpp
if (d->parsedOnce) return d->cachedParsed;   // :384
d->parsedOnce = true;                        // :387  flag set FIRST
const QString raw = d->frontmatter;
if (raw.isEmpty()) return {};
d->cachedParsed = YamlValue::parse(raw, &d->cachedParseError);   // :392  filled 5 lines later
return d->cachedParsed;
```

No mutex, no atomics, on `mutable` state. **Two** independent defects:

1. **Data race** — two threads entering concurrently corrupt
   `cachedParsed`/`cachedParseError`.
2. **A guaranteed lost-result window even *with* a mutex** — `parsedOnce` is set
   at `:387` and the value is not written until `:392`. Any second reader in
   that window takes the `:384` branch and receives a default-constructed
   `YamlValue`: **frontmatter appears absent.**

**Why it matters to us:** `MetadataParser.cpp:318` calls this on a worker thread
and documents are shared with the UI thread. The symptom is intermittent,
non-reproducible **"my properties vanished"**.

**Ask:** `std::call_once(d->parseOnce, …)` computing the parse inside the
initializer, plus `std::once_flag parseOnce` in `Private`. Also **document the
threading contract in `Document.h`** — it currently says nothing.

---

### ASK-M4 · markoff-parser: a failed parse leaves the *previous* document's inline trees bound to the *new* byte map

`libs/markoff-parser/src/TreeSitterParser.cpp:254-269`
```cpp
if (m_blockTree) ts_tree_delete(m_blockTree);
m_blockTree = ts_parser_parse_string(...);
if (!m_blockTree) return false;                                  // :257  EARLY RETURN
...
for (TSTree *t : m_inlineTrees) ts_tree_delete(t);               // :268  never reached
m_inlineTrees.clear(); m_inlineRanges.clear();
```

On a failed block parse, `m_utf8`/`m_byteToChar` are already the *new*
document's while `m_inlineTrees`/`m_inlineRanges` still hold the *old*
document's CSTs. `inlineSpansFor` then reads old trees against a new byte map,
producing stale, out-of-range `SourceSpan`s — **wrong highlight ranges, wrong
link/footnote offsets, rather than an error.**

tree-sitter returns null on cancellation, timeout or memory pressure, all of
which are reachable on a large vault file.

**Ask:** move the inline-tree teardown to the **top** of `parse()`, and add
`bool m_valid` set only on success with `inlineSpansFor`/`buildDocumentQueries`
returning empty when `!m_valid`.

---

### ASK-M5 · Graffodil: `BatchRenderer`'s destructor touches items the scene has already destroyed

`libs/graffodil/src/batch/src/BatchRenderer.cpp:23,29-31`
```cpp
BatchRenderer::BatchRenderer(GraphScene *scene) : QObject(scene), d(new Private) {}
BatchRenderer::~BatchRenderer() { setBatchMode(false); delete d; }   // :30 re-shows every item
```

`BatchRenderer` is a **child QObject of the scene**, so destruction order is:
`~BatchRenderer` body → `~QGraphicsScene` body (destroys all items) →
`~QObject` body (destroys children). `setBatchMode(false)` therefore runs
against a destroyed item set, calling **virtual** `node->graphicsItem()` on
freed `IGraphNode`s and `setVisible(true)` on freed `QGraphicsItem`s.

Separately, `synchronize()` has no null guards at `:127-128`, while every other
call site in the library defensively uses the documented-null-safe
`IGraphEdge::sourceNodeId()`.

**Ask:** store a `QPointer<GraphScene>` and early-return in the destructor if
it is null; add null guards in `synchronize()`. **Test:** scene + `BatchRenderer`
in batch mode, delete the scene, assert clean under ASAN.

---

### ASK-M6 · Graffodil: raw-pointer endpoint lifetime has no detach API

`GraphEdgeItem.h:121-122` holds `IGraphNode *m_source, *m_target`. `GraphScene::removeNode`
unregisters connected edges (`GraphScene.cpp:41-47`) but **never clears the
edges' endpoints**. Free a node before its edges and `adjust()` dereferences
freed memory on the next drag frame; survive it and `adjust()`'s early return
(`GraphEdgeItem.cpp:241-242`) leaves a **ghost edge frozen** at the dead node's
last position, with no invalidation.

The same class of bug survives in the tools: `CreateEdgeTool::m_sourceNode`
(`:199-201`, on every `mouseMoveEvent`) and `SelectMoveTool::m_dragNode`
(`:165-186`, plus a `QHash<IGraphNode*, QPointF>` keyed on raw pointers).

**Ask:** add `setEndpoints()` and null them from `removeNode`/`removeEdge`; make
`adjust()`'s null path call `setPath({})` + `prepareGeometryChange()` so the
edge disappears instead of ghosting; convert the tool members to `QPointer`
resolved from `graphicsItem()` at assignment time.

**This is a direct precedent, not a new argument:** the `AnchorHighlight`
dangling-pointer crash report already established the `QPointer` pattern and
fixed one instance of exactly this class, leaving three others.

---

## Priority 1 — user-visible gaps and hot-path cost

### ASK-M7 · Markoff styled leaf: `Markoff::Theme` is plumbed all the way in and then **ignored**

`FormatPass.h:21` declares `const Markoff::Theme *theme = nullptr;` and
`FormatPass.cpp` reads **only** `opts.fontScale`. Every colour is a literal, with
comments naming the Theme slot they *should* use (`FormatPass.cpp:113,133,191,194,197,200,211-212`):
`QColor(245,245,245) // Theme::CodeBlockBackground`, `QColor(100,100,100) // Theme::Quote.`, etc.

**Two independent breaks:** `computeBlockHash` (`:217-221`) takes no theme, so
even a theme-aware `FormatPass` would skip every block on a change; and
`StyleApplier::setTheme` (`:39-43`) early-returns on *pointer* equality while
`Styled::Editor::setTheme` always passes the same address (`Editor.cpp:49`).

**Proof it is a bug, not a choice:** `StyledFindAdapter` *does* use the theme for
its own highlights (`StyledFindAdapter.cpp:150-154`) — so find highlights
re-theme and the body text does not.

**Why it matters to us:** we propagate a full `Markoff::Theme` to all three
leaves. In Reading mode a dark theme and user themes have **no effect on
content**, and the `// Theme::X` comments actively hide it. This is the largest
user-visible gap we have.

**Ask:** read `opts.theme` at every colour site with a light fallback so the
renderer still works headless; add `theme` to `computeBlockHash`'s inputs; make
`StyleApplier::setTheme` store by value and compare with a new `operator==`.
Add `bool operator==(const Theme&)` to `Theme` — its absence currently forces
Corbomite to restyle every leaf on every theme propagation, forcing a full
document re-render each time.

---

### ASK-M8 · Markoff: systemic raw-pointer lifetime gap — fixed only in the **retired** leaf

Raw `MarkoffDocument *` with no `QPointer` and no `destroyed()` connection in:

| Holder | Site |
|---|---|
| `MarkdownView` | `MarkdownView.h:105`; `undo()`/`redo()` deref at `MarkdownView.cpp:39-46` |
| `SourceTextDocumentBinding` | `.h:161,165` |
| `FindController` | `.h:85`; `FindController.cpp:102-113` |
| `StyleApplier` | `StyleApplier.cpp:60-64` |
| `LinkInteraction` | `LinkInteraction.h:44`; `:47` on **every mouse-move** |
| `StyledTableRenderer` | `.h:23` |

**The fix already exists — in `markoff-live`, which is retired.** It says so:
```cpp
// markoff-live/src/EditorWidget.cpp:268-279
// Retire-on-destroy (INVARIANTS #3): the base MarkdownView holds the
// document by raw pointer (m_document). If the document is destroyed
// while still attached, null the base pointer so base accessors
// (cursorPosition/undo/redo/…) don't dereference freed memory.
d->docDestroyedCon = QObject::connect(doc, &QObject::destroyed, this, [this] {
    Markoff::MarkdownView::setDocument(nullptr);
});
```
The two production leaves never received it.

**Why it matters to us:** this is exactly the crash class we hit on 2026-06-10
(first-run SIGSEGV: a double `vaultRequested` tore the vault down under a live
editor). We fixed it in our code *and* in markoff-live; the production leaves
still have it. Any consumer that deletes a `MarkoffDocument` while a view is
attached — completely ordinary Qt — is one Ctrl+Z or one mouse-move from a UAF.

**Ask:** lift the `EditorWidget` pattern into `MarkdownView::setDocument` itself
so all leaves are fixed in one place, **and** change `m_document` to
`QPointer<MarkoffDocument>`. Same for the five members above.

---

### ASK-M9 · Markoff: no undo-availability query, so we must lie

`MarkdownView` exposes `undo()`/`redo()` with **no availability query**;
`MarkoffDocument::undoDepth()` (`MarkoffDocument.h:105`) reads the **legacy**
stack and is always 0 on a D2 document; the D2 `UndoLog::entryCount()` is not
surfaced.

**Our workaround, in our code** (`src/editor/MarkdownView.cpp:133-141`):
```cpp
bool MarkdownView::canUndo() const {
    // Markoff::MarkdownView's contract exposes undo()/redo() actions but
    // no undo-stack-depth query — hasEditing() (editable at all) is the
    // best available proxy until an upstream API addition.
    return canEdit();
}
```

So our Undo/Redo actions are enabled whenever the view is editable, **even with
an empty stack, and never grey out.**

**Ask:** `bool canUndoD2() const; bool canRedoD2() const; qsizetype
d2UndoDepth() const;` on `MarkoffDocument`, plus `virtual bool canUndo() const`
/ `canRedo() const` on `MarkdownView`.

---

### ASK-M10 · Markoff: three public accessors are permanently dead on a D2 document

`MarkoffDocumentPrivate.h:66-69` — `latestParse`, `latestBlockAnchors`,
`latestBlockRanges` are **never assigned anywhere**. Consequently
`parsedDocument()` always returns `nullptr`, and `blockAnchorAt()`,
`blockByteRange()`, `blockAt()` always return `std::nullopt` after
`loadFromMarkdown()`. The header documents them as ordinary working APIs
(`MarkoffDocument.h:136,141-149`).

This class of problem *did* get `[[deprecated]]` on `toMarkdownUtf8()` /
`toMarkdown()` (`:81-84`), so the treatment is inconsistent.

**Downstream evidence of harm:** the styled leaf's `CLAUDE.md` carries a section
titled *"**D2-broken core APIs to avoid**… When in doubt about a core API,
search `latestBlockRanges` first"* — a consumer reading only the public header
gets confident wrong answers.

**Ask:** populate the three members, or mark all four accessors
`[[deprecated("…D2 documents are unindexed")]]` and add
`bool isBlockIndexValid() const`.

Also `blockByteRange` (`:842-845`) bounds its loop on `latestBlockAnchors.size()`
but indexes `latestBlockRanges[i]` — a latent OOB.

---

### ASK-M11 · Markoff: `SearchEngine::FindFlags` advertises Regex / WholeWords / Backwards; only CaseSensitive exists

`SearchEngine.h:27-33` declares all four; `SearchEngine.cpp:25-26` guards the
other three with `Q_ASSERT`, which **vanishes under `NDEBUG`**. A "whole word"
search silently returns substring matches and a "regex" search silently does a
literal search, in every user build. `Backwards` isn't even asserted.

`FindController.h:54-56` actively misdocuments it: *"Whole-words / regex are
accepted by the underlying SearchEngine but UI work is deferred."* **They are
not accepted.**

**Ask:** implement them, or remove them from the enum, add
`bool supports(FindFlag)`, and fix the comment.

---

### ASK-M12 · Markoff: `onSaveComplete` waits forever for acks that a single-user consumer never sends — so GC never runs and memory grows forever

`MarkoffDocument.cpp:2598-2609` only compacts when `ackedWatermark >= W`;
otherwise, for a `collabConfigured` document with no peers, it emits
`wantsAcksAtWatermark(W)` and **waits forever**.

**We land in exactly that branch**: `NoteDocument.cpp:41-42` constructs
`MarkoffDocument(quint16{1}, …)` → `collabConfigured == true`; we write files
ourselves via `serializeForSave()` and never call `notifyAcksAtWatermark` or
`triggerGc`.

Consequence: **orphan per-block CRDT Buffers, BufferProxies,
`blockLoadTimeBytes` and `blockEditSequences` accumulate for every block ever
created, forever.** `collectGarbage()`/`compact()` do not help — they touch only
the dead legacy flat buffer (`:969-977`).

**This compounds a per-keystroke cost:** `d2EditSequence()`
(`MarkoffDocument.cpp:1171-1177`) sums `blockEditSequences` over **every block
ever created**, and `scheduleD2Changed()` calls it on every edit. Because
`blockEditSequences` is only pruned by `disposeOrphans()` — which never runs —
**our typing cost grows monotonically for the life of the document.** We call it
a second time in `NoteDocument::hasUnsavedEdits()` (`:130`).

**Ask:** (a) keep a running total incremented alongside the per-block counter;
(b) in `onSaveComplete`, compact immediately when there are no peers; (c) make
`collectGarbage()`/`compact()` delegate to `WatermarkCoordinator`; (d) document
that `save()` can drain Ctrl+Z — `undoLog.compact(all=true)` is
`WatermarkCoordinator.cpp:53-54` and is tested as intended, but nothing in the
public header says so.

---

### ASK-M13 · Markoff: ~6 full-document materialisations **per keystroke**

`SourceTextDocumentBinding.cpp:456-462` (`onQtContentsChange`, i.e. per
character typed) calls `widgetFlatView()`, then `QString::fromUtf8`, then
`toPlainText()`, then `findBlockAtSepByte` **twice**. `widgetFlatView()` is not
cached — it is `iterateBlocks()` plus a fresh `QByteArray::fromStdString()` per
block (`:1159-1164`), so one CRDT-buffer materialisation and heap allocation per
block, repeatedly. Structural keys do it again (`:390`), and
`sepViewPosOf` (`:307`) converts every block just to get its **length** while
being called from a function that walks the document three more times.

**Ask:** (a) a cached flat view on `MarkoffDocument` invalidated on edit-sequence
change; (b) a block-index cache (`QVector<quint32> blockByteOffsets_`) so
sep↔block resolution is a binary search; (c) coalesce `onQtContentsChange` into
one queued handler.

---

### ASK-M14 · Graffodil: no theming surface at all

Five hardcoded colours: `GraphEdgeItem.cpp:353` (selection `QColor(0,120,215)`),
`CreateEdgeTool.cpp:96`, `SelectMoveTool.cpp:145-146,199`,
`AnchorHighlight.cpp:77`, `GraphMinimap.cpp:56-57,89-90`, `ConnectorItem.cpp:12-13`.
Worse, `BatchRenderer.cpp:112` assigns `QColor(100,150,200)` **unconditionally**,
so a consumer that themes individual nodes gets uniform blue in batch mode with
no API to prevent it.

**Direct evidence this bit us:** our `CanvasPalette.h:12` says canvas had *"zero
theme awareness: ~15 hardcoded literals"* — and our
`ReconnectEdgeTool.cpp:78` **re-hardcodes the identical `QColor(0,120,215)`**
because Graffodil's own connect-preview line is not themeable.

**Ask:** add a `Graffodil::Palette` struct plus `GraphScene::setPalette()` and
`GraphEdgeItem::setSelectionColor()`, threaded to every site above; for
`BatchRenderer`, an optional `std::function<QColor(const IGraphNode*)>` colour
provider so batch mode can honour per-node theming. **We can ship our
`CanvasPalette` as the worked example.**

---

### ASK-M15 · Graffodil: no interaction path is sub-linear, despite shipping `Spatial` for exactly that

Every mouse-move does an O(items × nodes) scan plus an `anchors()` allocation per
node: `CreateEdgeTool.cpp:51-63` and `:175-183`, `SelectMoveTool.cpp:22-46`,
`CompositeTool.cpp:58-75` (`isNearAnchor`, on every move), `GraphScene.cpp:634-640`.
`nodes()` returns `QHash::values()` — a fresh `QList` per call.

**`Graffodil::Spatial` (QuadTree + SpatialGrid) has zero consumers inside
`Graffodil::Core`.** The index was built and never wired to the paths that need
it. (Our own audit nearly deleted `SpatialGrid` as dead code before finding it
was a member of `BatchNodeItem` — see the punch list.)

**Ask:** build a QuadTree over node bounding rects in `GraphScene`, invalidated
on structural change, and route all five call sites through one
`GraphScene::nodesAt(scenePos, radius)`. Cache `anchors()` per node.

---

### ASK-M16 · Markoff: `SearchEngine::findByBlock` is O(len × matches) in the case-insensitive path

`SearchEngine.cpp:53-64` re-serialises the whole block prefix to UTF-8 **twice
per match** (`blockStr.left(u16idx).toUtf8().size()`). Searching `"e"` in one
100 KB paragraph with 2 000 matches is ~4 000 allocations over 200 MB of copying.
`FindController` runs this on **every keystroke** in the find box and
re-runs on every `d2DocumentChanged` with no debounce.

**Ask:** track a running UTF-16→UTF-8 byte offset as you advance, and debounce
`recomputeMatches` behind a ~150 ms timer.

---

### ASK-M17 · Markoff: `LinkInteraction` mixes two coordinate spaces and does O(document) work at mouse-event rate

`LinkInteraction.cpp:53-56` computes a byte offset from `toPlainText()` — which
is the **separator** view — then passes it to `textAnchorAt()`, which accumulates
**no-separator** lengths (`MarkoffDocument.cpp:773-789`). The offset is therefore
**one byte too large for every block boundary already crossed**, so hovering or
clicking a link in block *k* resolves *k* bytes past the target.

Independently, `handleMove` (`:108-125`) runs on **every** `MouseMove`: a full
document copy, another prefix copy, an `iterateBlocks()` walk, `blockText()`, and
`inlineSpansFor()` — O(document) at 60–120 Hz, with no throttle.

**Ask:** map the `QTextBlock` number to a `BlockId` via the existing
`walkBlocks()`, or use `findBlockAtSepByte`; throttle `handleMove` and bail early
when the caret's block is unchanged.

---

### ASK-M18 · Markoff: O(N²) structural operations

`MarkoffDocument.cpp:1446-1453` — `d2InsertBlock` calls `iterateBlocks()` on
**every** block insert just to compute a row; `:1836-1841` does the same for
removal. `applyFlatEdit`'s multi-block path calls both, so **pasting N
paragraphs is O(N²)**. `:1782-1830` — `replaceMatches` calls `applyFlatEdit` once
per match, so **Replace-All is O(matches × N)**.

**Ask:** let `IdList` report the index directly (or cache a `BlockId → index`
map), and give `replaceMatches` a single-transaction multi-range primitive.

---

### ASK-M19 · Graffodil: `SugiyamaLayout` layer assignment is a single pass, so any cycle silently corrupts layering

`SugiyamaLayout.cpp:437-439` — Kahn's algorithm returns a **partial** order when
a cycle exists; the code only handles the all-or-nothing case
(`if (topoOrder.isEmpty())`). A single self-loop or 2-cycle leaves the order
non-empty but incomplete, missing nodes are appended **arbitrarily** (`:478-481`),
and the longest-path pass (`:486-494`) then runs once with
`if (layer.contains(pred))` silently skipping predecessors that appear later —
yielding a **too-small layer**, edges with `span <= 0`, and long edges routed
with no waypoints.

**Canvas files and dependency graphs legitimately contain cycles**, and ours do.

**Ask:** relax layer assignment to a fixed point (`do { … } while (changed);`).
**Test:** one self-loop plus a 3-node chain, asserting the chain's layering is
unaffected by the loop.

---

## Priority 2 — API gaps and smaller correctness

### ASK-M20 · Markoff: `Session` is not on the polymorphic base contract

`MarkdownView.h:14-109` has **no** session accessor. `Styled::Editor` has
`session()`/`setSession()` + a Q_PROPERTY; `Source::Editor` has **neither** — it
manages a private `QPointer<Session>` with no accessor. We hold leaves as
`Markoff::MarkdownView *` (`NoteEditorWidget.h:105`) and so cannot get or set a
session on a Source leaf at all.

**Ask:** add `virtual Session *session() const` / `virtual void setSession(Session *)`
to `MarkdownView`.

---

### ASK-M21 · Markoff: session lifetime leaks

`Source::Editor` creates a `Session` in `setDocument` (`Editor.cpp:150-153`),
destroys it only on a **document swap** (`:116-126`), and `~Editor()` is `= default`.
One leaked session per leaf teardown. **The retired live leaf fixed exactly this**
(`EditorWidget.cpp:210-216`) and says why. Separately,
`Styled::Editor::setSession` (`:412-417`) never emits `sessionChanged` when the
session dies, because the `QPointer` is already null and it early-returns — so a
consumer bound to that signal never learns.

**Ask:** destroy the owned session in `~Editor()`; connect to
`QObject::destroyed` and null+emit. Consider a
`virtual Session *createOwnedSession()` hook on `MarkdownView` so no leaf has to
remember.

---

### ASK-M22 · Markoff: process-global `static` BlockId counters

`MarkoffDocument.cpp:1066,1420,1338` are function-local `static std::atomic`s.
`allocateD2BlockId()` (`:1920`) is correctly per-document; these three are not,
so **two open notes mint identical `BlockId`s** for structurally-inserted blocks.
Nothing in the library compares IDs across documents today, but any host-side
index keyed by `BlockId` — fold/collapse state, a remote-cursor registry, a
persisted ephemeral-state map — will alias.

**Ask:** move all three onto `Private`.

---

### ASK-M23 · Markoff: styled `recomputeContext()` has no table-frame guard

`Editor.cpp:489-527` maps `cursor.position()` (QTextDocument coordinates, which
include frame structure characters) through `doc->widgetFlatView()` (raw markdown
source bytes). Past the first table frame every position resolves to the wrong
block, so `contextChanged` fires with a wrong `blockKind`/`headingLevel`. The
format verbs *do* guard via `selectionInTrustworthyRegion` (`:265-292`) — the
context path just forgot. `tableRow`/`tableCol` are also never populated by any
leaf.

**Ask:** reuse `selectionInTrustworthyRegion`, and populate `tableRow`/`tableCol`.

---

### ASK-M24 · Markoff: source leaf lacks the list/quote marker restore that styled has

`markoff-styled/src/StructuralTextEdit.cpp:44-81` prepends
`listItemDisplayMarker`/`blockQuoteDisplayMarker`; **`Source::InnerEditor.cpp:33-38`
does not.** So Copy-as-Markdown/HTML from **Source** mode emits marker-less
content — which, as styled's own comment says, pastes into LibreOffice as Body
Text rather than a List/Block Quote.

**Ask:** lift it into a shared helper in `markoff-core` and call it from both.

---

### ASK-M25 · Markoff: `StyledFindAdapter` is not re-wired on document swap, and re-walks the document per signal

`Editor.cpp:82-175` re-wires the binding, style applier, `LinkInteraction` and
both context connections — but never touches `m_findAdapter`, whose
`m_docChangedCon` is bound to one `MarkoffDocument` (`:78-81`). After a swap,
find highlights stop refreshing unless the consumer performs a documented
"detach → swap → re-attach" dance. Separately, `renderHighlights`
(`StyledFindAdapter.cpp:136-180`) is wired to `matchesChanged`,
`currentMatchChanged`, `themeChanged` **and** `d2DocumentChanged`, and each fire
walks every block.

**Ask:** re-wire the adapter in `setDocument`; cache walk entries keyed on
`blockEditSequence`.

---

### ASK-M26 · Markoff: export macros are no-ops while the libs are `SHARED`

`MarkoffCoreExport.h:4` and `MarkoffStyledExport.h:4` are `#define … (empty)`
with comments saying *"built STATIC"* — **both are `SHARED`**
(`markoff-core/CMakeLists.txt:17`, `markoff-styled/CMakeLists.txt:11`).
`markoff-source` has **no export header at all**. `Markoff::UndoLog` — which
consumers must instantiate to use the D2 API — has **no** macro, nor do
`FormatOps`, `Cmd::*`, or the `Detail::` helpers.

Latent on Linux/GCC (no `-fvisibility=hidden`, so everything exports by luck),
but the libraries **cannot be consumed on MSVC** and **will fail to link the
moment anyone adds `-fvisibility=hidden`**.

**Ask:** populate the macros with the usual `Q_DECL_EXPORT`/`Q_DECL_IMPORT`
switch and annotate every public symbol.

---

### ASK-M27 · Build friction: demo apps built unconditionally; no installable SDK

1. `add_subdirectory(app)` is ungated in `markoff-source/CMakeLists.txt:54`,
   `markoff-styled:66`, `markoff-live:150`. `MARKOFF_BUILD_APPS` only covers
   `apps/`, so **every consumer builds two demo apps** it never installs.
2. `markoff_live`'s PUBLIC include dir is a raw source path
   (`markoff-live/CMakeLists.txt:121-124`) — commit `17a8376f` wrapped exactly
   this for canvas/source/styled and **missed live**, so flipping
   `MARKOFF_BUILD_LIVE=ON` breaks `install(EXPORT)` validation.
3. **No `install(DIRECTORY …/include)`** — libs install but headers do not, so
   there is no consumable SDK and only in-tree subdir builds work.
4. `markoff_core` links `collabtext` and `Markoff::Parser` as **PUBLIC** and
   `MarkoffDocument.h:18-20` includes `<crdt/…>`, so every consumer inherits
   collabtext's tree — while `markoff-core/CLAUDE.md` claims *"Public-boundary
   types (no `<crdt/...>` dependency)"*. **The claim is false at the header level.**

Graffodil has the same class of problem (`src/core/CMakeLists.txt:74-81`
installs the library but no headers and no export set), and jkqtmathtext too.

---

### ASK-M28 · Test infrastructure across the whole family

- **No `LABELS` on any test in markoff-core/source/styled/live.** `ctest -L
  benchmark` cannot exclude anything. (The canvas perf labels were already filed.)
- **All four `tst_d5_two_doc_convergence.cpp` cases do one `d2ApplyBufferEdit`
  per transaction** — which is precisely why ASK-M2 survived.
- `markoff-live/tests/tst_live_render_doc_destroyed.cpp` exists **for the retired
  leaf**; there is no equivalent for the production leaves, which is why ASK-M8
  survived.
- `perf_load_bench` is not registered with ctest at all.
- Graffodil: 16 tests, `ctest -N` in our tree matches **zero**.
- collabtext's 50 tests *do* run in our ctest — but `enable_testing()` and all
  50 targets are unconditional (`collabtext/CMakeLists.txt:93`), so any project
  vendoring it builds them whether it wants them or not.
- `markoff-source/CLAUDE.md:27` documents `Markoff::Source::FindBar` — it does
  not exist.
- `UndoLog::lastEntry()` is public, returns `m_entries.back()` with no
  empty-guard, and hands out a reference into a vector `undo()`/`compact()`
  mutate.

---

### ASK-M29 · collabtext: `Global::meet` is not a meet — produces GC-unsafe watermarks

`Clock.cpp:164-178`: a replica id **absent** from `other` leaves `self`'s value
untouched, so the result keeps the higher value. The shipped caller is
`NetworkSim::compact_all()` (`NetworkSim.cpp:168-177`).

With ≥3 replicas where r0 has seen r2 but r1 never has, `meet` leaves r2's high
value in the watermark; `Buffer::compact` then GCs r2's tombstones r1 never
received → **permanent text divergence**, exactly what `compact()` exists to
prevent.

**Why no test catches it:** all five `compact_*` cases in `tst_gc.cpp` use
exactly **two** replicas, where both version vectors carry both ids and `meet`
degenerates to the correct pointwise min. The bug lives only at ≥3 replicas.

**Ask:** two-pass — collect the union of replica ids, drop every id not present
in *all* replicas (so `observed()` is false → no GC), then meet. **Ship the
missing 3-replica test.**

---

### ASK-M30 · markoff-parser: UTF-8 lead byte trusted without validation

`TreeSitterParser.cpp:205-225` infers sequence length from the lead byte alone,
with no check that the following bytes are continuation bytes and no
overlong/surrogate check. A lone `0xC3`, a truncated sequence, or a Latin-1
encoded byte desynchronises `charIdx` from `QString`'s real char count — and
`utf8ToCharOffset()` (`:288-292`) then feeds headings, links, tags, footnote
refs and inline spans.

**We parse real user vault files, and Obsidian tolerates Latin-1 notes.** The
offset skew is silent.

**Ask:** validate continuation bytes, treat invalid sequences as 1 byte, and add
a release-build assertion that the final `charIdx == text.size()`. **Test:**
Latin-1 fixture and a truncated multibyte sequence.

---

### ASK-M31 · markoff-parser: whole-document reparse, no incremental path

`TreeSitterParser.cpp:247-284` re-encodes the whole document and builds a fresh
`4N`-byte `QList<int>` map every call, and passes `nullptr` as the previous tree —
**disabling tree-sitter's incremental reuse entirely**. `buildByteToCharMap`
zero-fills then overwrites, so every byte is written twice.

We call `Document::fromMarkdown()` per file (`MetadataParser.cpp:301`), and
`libs/bases` re-parses on every filter evaluation.

**Ask:** pass `m_blockTree` as the previous tree; expose
`bool reparse(const QString&, quint32 editOffset, …)`; `resize()` instead of
zero-fill; build the map incrementally.

---

### ASK-M32 · markoff-parser: `PerfProbe` costs a mutex and a heap allocation per site, in every build

`PerfProbe.h` — `record()` takes a `QMutexLocker` then
`QString::fromLatin1(name)`. The header claims sites are *"benign to leave in
non-benchmark builds"*; they are not. Worse,
`MARKOFF_PERF_SCOPE("parser.inline_grammar_one_range")` sits **inside** the
per-inline-region loop, so a 500-heading document costs 500 mutex round-trips per
parse — perturbing the numbers it measures.

**Ask:** compile out unless `MARKOFF_PERF` is defined; key the map on `quintptr`
(string-literal address) instead of `QString`.

---

## Smaller items, listed for completeness

| Ask | Location | Note |
|---|---|---|
| Graffodil `edgeActionAt` hit-tests in **item-local** coords and copies every path per edge per press | `GraphScene.cpp:613-625` | Sibling `nodeSubItemAt` (`:634`) uses scene coords — the two disagree. Any rotated/translated edge item has an unclickable affordance. |
| Graffodil `QHash` iteration → non-deterministic hit-test winner | `GraphScene.cpp:610,634`, `CreateEdgeTool.cpp:56`, `SelectMoveTool.cpp:26` | Overlapping nodes pick an unspecified winner, varying run to run. |
| Graffodil `GraphMinimap` never resets `m_dragging` | `GraphMinimap.cpp:97` | **No `mouseReleaseEvent` exists.** After one click, every hover re-centres the view with no button held. |
| Graffodil `GraphMinimap` divides by zero-width rects | `GraphMinimap.cpp:66-68,134-136` | `painter.scale(inf, inf)` on a degenerate scene rect. |
| Graffodil `GraphScene::clearGraph()` emits mid-teardown | `GraphScene.cpp:186-197` | `m_edges.clear()` comes *after* the emit loop — a slot re-entering corrupts the registries. |
| Graffodil `LayoutResult::edgeWaypoints` keyed by `(sourceId,targetId)` | `SugiyamaLayout.cpp:790-798` | Parallel A→B edges collapse onto one waypoint list and route identically; self-loops unhandled. |
| Graffodil `countCrossings` is O(E²), up to 25× per layout | `SugiyamaLayout.cpp:131-138,654,701` | Barth–Jünger–Mutzel makes it O(E log V). |
| Graffodil `SelectMoveTool` rebuilds the guide overlay every drag frame | `SelectMoveTool.cpp:189-202` | removeItem+delete+new+re-add per mouse-move. |
| Graffodil `CreateEdgeTool` leaks its preview line | `CreateEdgeTool.cpp:21-31` | Cleans `m_highlight`, never `m_previewLine`. |
| Graffodil `AnchorHighlight`'s `QGraphicsObject` requirement was never documented | `AnchorHighlight.cpp:19`, bug report `:159-177` | The report chose "document it"; `IGraphNode.h` (80 lines, read in full) never mentions it. Consumers on a bare `QGraphicsItem` get silently inert anchor highlighting. |
| Graffodil `GraphEdgeItem::boundingRect()` ignores pen width | `GraphEdgeItem.cpp:301-312` | Highlight/selection widen the pen **without** `prepareGeometryChange()`, so the stroke clips. |
| Graffodil `QuadTree::insert()` holds a `QuadNode&` across `subdivide()` | `QuadTree.cpp:76,96` | Safe only incidentally — `qn` isn't read after, and the reserve is insufficient so realloc *will* happen. |
| Graffodil `QuadTree::computeRepulsion` uses `bounds.width()` as the region size | `QuadTree.cpp:161` | Root bounds are non-square, so wide graphs silently coarsen the Barnes-Hut approximation. |
| Graffodil `setEdgesAboveNodes()` can't tell a consumer's z=0 from the Qt default | `GraphScene.cpp:101-112` | Qt's default z **is** 0.0, so the header's claim is unachievable. |
| Graffodil `IGraphNode::anchorToward()` returns a silent (0,0) on an empty list | `IGraphNode.cpp:8-20` | Edges route to the scene origin; `anchor()` (`:28-33`) *does* warn — but at per-frame rate. |
| collabtext `set_entries` rebuilds the whole `SumTree` per edit | `IdList.cpp:394-400` | Every block insert/remove is O(n log n). Fires on every Enter that splits a paragraph. |
| collabtext publicly links `Qt6::Test` and exports its internal `src/` | `collabtext/CMakeLists.txt:76-80` | Every downstream target inherits a test-framework dep and internal `crdt/` headers — contradicting its own `CLAUDE.md` ("`src/crdt/` … internal-only"). |
| collabtext + jkqtmathtext install into **`CorbomiteTargets`** | `collabtext/CMakeLists.txt:85`, `jkqtmathtext/CMakeLists.txt` | Two third-party trees name our private export set, leaking our build layout into them. |
| Markoff `FORCE`s a rapidoaml cache var into the consumer's cache | `markoff-family/CMakeLists.txt:50` | Legit option, wrong place, and `FORCE` silently overrides an explicit `-D`. rapidoaml already does this correctly internally. |
| jkqtmathtext is vendored **twice**, ~19 MB of each copy is unread | `libs/jkqtmathtext/`, `libs/markoff-family/libs/jkqtmathtext/` | Byte-identical but for two files. Only 6 `.otf` (1.4 MB) are referenced by `xits.qrc`; `sources/` alone is 8.6 MB. Our root `CMakeLists.txt` deliberately adds ours first so the submodule's `if(NOT TARGET)` guard short-circuits — which makes the build order-dependent. |
| jkqtmathtext PUBLIC include dir is its own parent | `jkqtmathtext/CMakeLists.txt` | Puts `libs/` on every consumer's include path, so `<core/…>`, `<canvas/…>` all become includable. Our `libs/core/CMakeLists.txt:178` depends on this. |
| jkqtmathtext installs the `.so` but not its headers | same | Undermines the LGPL §4 relinkability affordance its CMake comment cites. |
| rapidoaml's changelog stops at 0.9.0 but the vendored version is 0.11.1 | `libs/rapidyaml/` | Two releases of notes missing, so the copy can't be audited against its own changelog. |
| Markoff leaves keep a **second** `Theme` member beside the base's | `markoff-styled/Editor.{h:134,cpp:421-430}`, `markoff-source/Editor.{h:142,cpp:282-302}` | Two authorities that stay in sync only because `Editor::setTheme` writes both. |

---

## Deliberately NOT asking (yet)

- **mermaid / mmdr** — deferred by user decision 2026-10-02. The Rust source is
  not in our tree and not reachable from it, so a rebuild is *impossible*, not
  merely undocumented; documented honestly in `libs/mmdr/README.md`. Resuming
  needs one of: vendored source + pinned commit, an optional external dep, or a
  fetch script — and a separate decision on whether to rewrite history.

  **One update from the 1.13.7 RE (2026-10-03), which changes the picture for the
  *upstream* ask even though our mmdr situation is unchanged.** Obsidian ships
  **stock upstream mermaid 11.13.0** — `lib/mermaid.min.js` line 1 is
  `// Source: https://unpkg.com/mermaid@11.13.0/dist/mermaid.min.js` — not a fork.
  And its header documents a **local patch**:

  > `// Manual patch: Search for "break-spaces", look behind and replace
  > return f.width===r&& with return Math.round(f.width)===Math.round(r)&&.`

  That is a float-equality bug in mermaid's `break-spaces` label wrapping, worked
  around locally rather than upstream. Two consequences: (a) there is a genuine,
  cheap **upstream bug report** available to file against mermaid, independent of
  anything Corbomite does; (b) for the planned native parser we do **not** need to
  RE mermaid at all — the grammar is upstream's own source and we have network.
  RE is only needed to learn how Obsidian *integrates* it
  (`app/own/1486-tW.js` calls `mermaid.initialize({fontFamily: "var(--font-mermaid)"})`).

## Suggested filing order

1. **ASK-M1** (collabtext asserts) — highest severity, smallest diff, three
   file:line cites sharing one fix shape.
2. **ASK-M2** (collab op keying) — has a reproduction attached. Pair with a
   `CollabMode` split.
3. **ASK-M3**, **ASK-M4** (parser) — small, self-contained, obvious tests.
4. **ASK-M29** (watermark) — needs its own design conversation and a
   3-replica test that does not exist.
5. **ASK-M5**, **ASK-M6** (Graffodil lifetime) — the `AnchorHighlight` bug
   report is the precedent, so this is a follow-up rather than a new argument.
6. **ASK-M7** (theme ignored) — biggest user-visible gap; **offer our
   `CanvasPalette` as the worked design** so the ask arrives with a solution.
7. **ASK-M8**–**ASK-M11** (lifetime, undo query, dead accessors, FindFlags) —
   these are the four that make *our* code carry workarounds; quote our
   workarounds in each issue.
8. Everything in Priority 2 as a batch.

## Maintenance

Re-verify against the pinned submodule SHAs before filing — line numbers drift
fast, and several findings here are "the code does not match its own comment",
which is exactly the class of claim that goes stale. Update
`docs/USER-TESTING.md` if any of this changes user-visible behaviour.