# Consolidated open asks — Markoff family, collabtext, Graffodil, vendored deps

**From:** Corbomite (Qt/KDE desktop client for Obsidian-format vaults)
**To:** the Markoff maintainers — the same people own collabtext and Graffodil
**Date:** 2026-10-18
**Status:** every ask below is open as of this date; nothing here is a request for
a decision we have already made ourselves

This is the **single document** for everything Corbomite is asking of the Markoff
project. It replaces the per-library handoffs as the live artifact: the seven
documents under `docs/handoff/2026-10-02-*` and their neighbours were sent
individually and are kept only as the record of what was sent on which date. Two
asks that were living inside those documents but never made it into the internal
index are now numbered here (ASK-M33, ASK-M34), one more was recovered from the
2026-10-01 and 2026-06-10 frontmatter handoffs (ASK-M35), and one new
build-hygiene ask is filed from this week's work (ASK-M36).

The **2026-10-17 canvas-cards handoff** was previously carried as an unnumbered
workstream. It is now numbered, as **ASK-M37** (embeddable card editor, P1),
**ASK-M38** (one session per document key, P2) and **ASK-M39** (mid-block paste
splice, P2) — one ask per decision we actually need, rather than five numbered
items that mixed two asks with three questions. Of that document's five
questions, **three are now closed by reading your own source and are not being
re-asked**: paint-without-re-import is already delivered (the pointer
`renderInto` overload exists and is `const`, and our paint path uses it); a
durable provider may already keep many scopes open at once
(`ReferenceProvider::open` tracks only a per-key `activeDocuments` set); and
`EditorWidget` already imposes no central-editor constraint at construction. Each
closure is stated with its evidence inside the ask it belongs to, so a reply can
dispute it cheaply.

**How to read it.** Severity first, then the library. Each entry states what
happens, the evidence with `file:line` and quoted code, what it costs Corbomite
specifically, the ask, and how the finding was verified. Every finding was
verified against the pinned Markoff source at our current pin (`dca64686`) by
reading it; the P0s were re-checked by hand after a delegated audit, which is why
they are stated with quoted code rather than paraphrase.

**Severity is the failure class, not the size of the diff:** P0 = provable
correctness bug with memory-safety, data-loss or divergence consequences; P1 =
user-visible gap or hot-path cost; P2 = API gap or smaller correctness issue; P3 =
build and integration hygiene. Four entries sit in the index's P2 *number* range
but are labelled P1 because their own detail document classifies them that way
(M23, M24, M30, M31), and ASK-M29 is carried here at P0 because the collabtext
handoff that details it does — the internal index had it filed under P2.

At a glance:

| Severity | Count | Where |
|---|---|---|
| P0 | 7 | collabtext ×2, markoff-parser ×2, Graffodil ×2, markoff-core ×1 |
| P1 | 19 | Markoff (cross-cutting / core / live / canvas) ×12, Graffodil ×3, markoff-parser ×3, markoff-styled ×1 |
| P2 | 12 | Markoff ×7, markoff-core ×2, markoff-parser ×1, build friction ×1, test infrastructure ×1 |
| P3 | 1 | markoff-parser ×1 |
| — | 21 rows | a table of smaller items, listed for completeness at the end |

**What we are not asking** for is listed in the internal index
(`docs/UPSTREAM-ASKS.md`, section "Deliberately NOT asking (yet)") — mostly
things where the cost of the change exceeds what it buys us, or where we decided
Corbomite should carry the divergence. We are not repeating that list here.

**Where we would rather be shown wrong:** on any entry below, a reply that
explains why the current behaviour is correct is more useful to us than a
workaround — we would rather delete a workaround than keep one.

---

## Priority 0 — provable correctness bugs, small diffs

### ASK-M1 · collabtext: `assert()` is the only guard on three anchor-lookup paths
**Severity:** P0 — in a release build this is a heap OOB *write* and a silently-lost delete.

**What happens:** `assert()` is the only guard on three anchor-lookup paths. In a release build this is an out-of-bounds heap read followed by an out-of-bounds heap write — not a crash, but silent corruption of whatever the allocator placed after the vector. On a miss in `insert_after`, `Locator::between` guards its own precondition with `assert(lo <= hi)` (`Locator.cpp:44`), also compiled out, so the entry lands at an **arbitrary position** in the sorted entries; this one silently reorders the user's document. In `apply_concrete(RemoveOp)` on a miss, the deletion is silently discarded, but its Lamport timestamp is **written into `m_version` anyway**, the op is reported as applied, and the deferred queue (`:430-448`) drops it so it is never retried. The `:478` case is **self-declaring divergence**: `version()` now asserts "I have seen delete D" for a delete that did not happen, so any future GC driven off `version()` may reclaim the insert D targeted — making the divergence permanent and unrecoverable rather than a transient.

**Evidence:** `libs/collabtext/src/crdt/IdList.cpp`

| Line | Path | Release behaviour |
|---|---|---|
| `:88` | `size_t idx = entries.size();` — "not found" sentinel | — |
| `:96` | `assert(idx < entries.size())` | compiled out |
| `:99` | `Lamport target_origin = entries[idx].origin;` | **OOB read** one-past-end |
| `:113` | `entries[idx].deletions.push_back(del_id);` | **OOB heap write** |
| `:40` | `insert_after`: `assert(found && …)`, then `Locator::between(predecessor, successor)` with a default-constructed predecessor | entry lands at an **arbitrary position** |
| `:478` | `apply_concrete(RemoveOp)`: `assert(found && …)` | — |
| `:480-483` | …then unconditionally `m_version.observe(op.timestamp); m_version.join(op.version);` and `return true` | the version vector now claims a delete that **never happened**, the deferred queue drops it so it is never retried |

`entries` is `std::vector<IdListEntry>` (`IdList.h:187`). Under `NDEBUG` the `assert` at `:96` is gone, so `idx == entries.size()` indexes one-past-the-end, and the value read at `:99` is then fed into a `Lamport`, so the corruption is not even immediately self-evident.

```cpp
:88    size_t idx = entries.size();                 // "not found" sentinel
:89-95 for (size_t i = 0; i < entries.size(); ++i) { /* find matching origin */ }
:96    assert(idx < entries.size() && "remove_at: target anchor not found in entries");
:99    Lamport target_origin = entries[idx].origin; // <-- OOB READ (idx == size())
...
:113   entries[idx].deletions.push_back(del_id);    // <-- OOB WRITE
```

```cpp
:40    assert(found && "insert_after: anchor not found in entries");
:43    Locator predecessor;   // default-constructed on a miss
:48    Locator new_loc = Locator::between(predecessor, successor);
```

```cpp
:469   bool found = false;
:470-476 for (auto &e : entries) { /* find target_origin */ if (match) { e.deletions.push_back(...); found = true; break; } }
:478   assert(found && "apply_concrete(remove): target_origin not in tree — causal gate should have prevented this");
:480   m_clock.observe(op.timestamp);
:481   m_version.observe(op.timestamp);
:482   m_version.join(op.version);
:483   set_entries(std::move(entries));
:484   return true;
```

**Cost to Corbomite:** Markoff stores block order in `IdList` and feeds it anchors from persisted document state. A stale anchor — a truncated autosave, a merge of two files, a block whose origin never made it into the local list — is an ordinary user action, not an exotic one.

**Ask:**
1. Make the miss a first-class outcome. `std::optional<IdListOperation> remove_at(const Anchor&)` returning `std::nullopt` plus an unconditional `qCWarning`, checked **before any mutation**. Same for `insert_after`.
2. In `apply_concrete`, reorder so `if (!found) return false;` precedes every mutation — the existing deferred queue then retries it.
3. Demote the `assert`s to post-guard invariant assertions.
4. **Tests:** (a) remove an anchor not present; (b) a remove op delivered *before* its insert; (c) both under a release build in CI.

**Verified:** Read directly at the line numbers quoted; §1's second half was additionally confirmed by checking that `entries` is a `std::vector` (`IdList.h:187`) and that the compiler configuration used for release builds defines `NDEBUG`.

---

### ASK-M2 · Markoff core: collab op payloads keyed without the `BlockId` → multi-block pastes lose and mis-attribute content
**Severity:** P0 — reproduced content loss on the collaboration path.

**What happens:** Document A pastes 6 paragraphs via `applyFlatEdit`, ops are routed to document B through the normal op path, and A and B both report 6 blocks — but B's block 1 holds `chunk5` and blocks 2–5 are empty, so two thirds of the pasted content is gone and `chunk5` landed in the wrong block. The BlockId is right there in `UndoCrdtTarget::buffer(block)` and is discarded before the key is built; the key has two components and neither identifies the block.

**Evidence:** Reproduced end-to-end against the built `libmarkoff_core.so`, using the same two-document harness as the existing `tst_d5_two_doc_convergence.cpp`: document A pastes 6 paragraphs via `applyFlatEdit`, ops routed through `localOpsProduced` → `applyRemoteOps`. Document B converges to:

```
A: chunk0 chunk1 chunk2 chunk3 chunk4 chunk5
B: chunk0 chunk5 '' '' '' ''
```

```
A blocks=6   B blocks=6
   0  A='chunk0'  B='chunk0'
   1  A='chunk1'  B='chunk5'   <-- wrong content in the wrong block
   2  A='chunk2'  B=''
   3  A='chunk3'  B=''
   4  A='chunk4'  B=''
   5  A='chunk5'  B=''
```

`B.serializeForSave()` → `"chunk0\n\nchunk5\n\n\n\n\n\n"`. Two-thirds of the pasted content is gone, and `chunk5` landed in block 1.

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

**Root cause:** every per-block `CollabText::Crdt::Buffer` has its **own** `m_clock` starting at 0, so the *first* local edit in *every* block yields the identical `OpId` (`(replicaId << 48) | 1`). Two fresh blocks in one transaction collide on the key, and `QHash::take` returns only one payload; the other is dropped by the `isEmpty()` → `continue` path.

**Why no test catches it:** all four existing `tst_d5_two_doc_convergence.cpp` cases do exactly **one `d2ApplyBufferEdit` per transaction** — one Buffer op per OpId, so the collision is structurally unreachable.

**Cost to Corbomite:** Secondary cost to us: we construct `MarkoffDocument(quint16{1}, …)`, so `isCollabConfigured()` is `true` and we pay the payload encode/decode on every keystroke for a wire we never use — running this buggy path for nothing.

**Ask:**
1. Key on the target's full identity — e.g. `{targetTypeIndex, blockId, opId}`, or a per-`(targetIndex, blockId)` sub-map.
2. **Turn the silent `continue` into a `qCWarning`.** A dropped op is never acceptable.
3. Consider deriving one monotonic Lamport across the document in `applyFlatEdit` / `applyInteractiveNewline` / `replaceMatches` rather than relying on per-buffer clocks.
4. **Test:** one `applyFlatEdit` producing ≥2 blocks in a single transaction, routed through the ops path, asserting convergence.
5. **Separately:** split `CollabMode` from the replica id so a single-user consumer can keep a real replica id with op-emission *off*.

**Verified:** Reproduced with a built two-document harness against the compiled `libmarkoff_core.so` — not taken from a code skim.

---

### ASK-M3 · markoff-parser: `Document::parsedFrontmatter()` is an unsynchronised cache that publishes an empty result
**Severity:** P0 — intermittent "my properties vanished".

**What happens:** `parsedFrontmatter()` keeps `mutable` cache state with no mutex and no atomics, with **two** independent defects. First, a data race: two threads entering concurrently corrupt `cachedParsed`/`cachedParseError`. Second, a guaranteed lost-result window even *with* a mutex: `parsedOnce` is set at `:387` and the value is not written until `:392`, so any second reader in that window takes the `:384` branch and receives a default-constructed `YamlValue` — frontmatter appears absent. The mutex would not save that case; only the ordering is wrong.

**Evidence:** `libs/markoff-parser/src/Document.cpp:382-394`
```cpp
if (d->parsedOnce) return d->cachedParsed;   // :384
d->parsedOnce = true;                        // :387  flag set FIRST
const QString raw = d->frontmatter;
if (raw.isEmpty()) return {};
d->cachedParsed = YamlValue::parse(raw, &d->cachedParseError);   // :392  filled 5 lines later
return d->cachedParsed;
```

**Cost to Corbomite:** `MetadataParser.cpp:318` calls this on a worker thread and documents are shared with the UI thread. The symptom is intermittent, non-reproducible **"my properties vanished"**.

**Ask:** `std::call_once(d->parseOnce, …)` computing the parse inside the initializer, plus `std::once_flag parseOnce` in `Private`. Also **document the threading contract in `Document.h`** — it currently says nothing.

**Verified:** Read directly at the quoted lines; the function is a five-line function small enough to audit by eye.

---

### ASK-M4 · markoff-parser: a failed parse leaves the *previous* document's inline trees bound to the *new* byte map
**Severity:** P0 — silent wrong offsets rather than an error.

**What happens:** On a failed block parse, `m_utf8`/`m_byteToChar` are already the *new* document's while `m_inlineTrees`/`m_inlineRanges` still hold the *old* document's CSTs. `inlineSpansFor` then reads old trees against a new byte map, producing stale, out-of-range `SourceSpan`s — **wrong highlight ranges, wrong link/footnote offsets, rather than an error.** tree-sitter returns null on cancellation, timeout or memory pressure, all of which are reachable on a large vault file.

**Evidence:** `libs/markoff-parser/src/TreeSitterParser.cpp:254-269`
```cpp
if (m_blockTree) ts_tree_delete(m_blockTree);
m_blockTree = ts_parser_parse_string(...);
if (!m_blockTree) return false;                                  // :257  EARLY RETURN
...
for (TSTree *t : m_inlineTrees) ts_tree_delete(t);               // :268  never reached
m_inlineTrees.clear(); m_inlineRanges.clear();
```

**Ask:** move the inline-tree teardown to the **top** of `parse()`, and add `bool m_valid` set only on success with `inlineSpansFor`/`buildDocumentQueries` returning empty when `!m_valid`.

**Verified:** Mechanism read directly at the quoted lines; it depends on tree-sitter returning null, which was not forced — **reachable but not reproduced**.

### ASK-M5 · Graffodil: `BatchRenderer`'s destructor touches items the scene has already destroyed
**Severity:** P0 — use-after-free on teardown, on the virtual call path.

**What happens:** `BatchRenderer` is a **child QObject of the scene**, so destruction order is: `~BatchRenderer` body → `~QGraphicsScene` body (destroys all items) → `~QObject` body (destroys children). Its destructor calls `setBatchMode(false)`, which therefore runs against a destroyed item set, calling **virtual** `node->graphicsItem()` on freed `IGraphNode`s and `setVisible(true)` on freed `QGraphicsItem`s. Separately, `synchronize()` has no null guards, while every other call site in the library defensively uses the documented-null-safe `IGraphEdge::sourceNodeId()`.

**Evidence:** `libs/graffodil/src/batch/src/BatchRenderer.cpp:23,29-31`
```cpp
BatchRenderer::BatchRenderer(GraphScene *scene) : QObject(scene), d(new Private) {}
BatchRenderer::~BatchRenderer() { setBatchMode(false); delete d; }   // :30 re-shows every item
```
`synchronize()` lacks the guards at `:127-128`.

**Ask:** store a `QPointer<GraphScene>` and early-return in the destructor if it is null; add null guards in `synchronize()`. **Test:** scene + `BatchRenderer` in batch mode, delete the scene, assert clean under ASAN.

---

### ASK-M6 · Graffodil: raw-pointer endpoint lifetime has no detach API
**Severity:** P0 — dangling endpoint dereference, plus a ghost edge that never invalidates.

**What happens:** `GraphEdgeItem` holds `IGraphNode *m_source, *m_target`. `GraphScene::removeNode` unregisters connected edges but **never clears the edges' endpoints**. Free a node before its edges and `adjust()` dereferences freed memory on the next drag frame; survive it and `adjust()`'s early return leaves a **ghost edge frozen** at the dead node's last position, with no invalidation. The same class of bug survives in the tools: `CreateEdgeTool::m_sourceNode` (on every `mouseMoveEvent`) and `SelectMoveTool::m_dragNode` (plus a `QHash<IGraphNode*, QPointF>` keyed on raw pointers).

**Evidence:** `GraphEdgeItem.h:121-122`; `GraphScene.cpp:41-47` (`removeNode` unregisters but does not detach); `GraphEdgeItem.cpp:241-242` (`adjust()` early return); `CreateEdgeTool.cpp:199-201`; `SelectMoveTool.cpp:165-186`.

**Ask:** add `setEndpoints()` and null them from `removeNode`/`removeEdge`; make `adjust()`'s null path call `setPath({})` + `prepareGeometryChange()` so the edge disappears instead of ghosting; convert the tool members to `QPointer` resolved from `graphicsItem()` at assignment time.

**This is a direct precedent, not a new argument:** the `AnchorHighlight` dangling-pointer crash report already established the `QPointer` pattern and fixed one instance of exactly this class, leaving three others.

---

### ASK-M29 · collabtext: `Global::meet` is not a meet — produces GC-unsafe watermarks
**Severity:** P0 — permanent text divergence under ≥3 replicas
**What happens:** `Clock.cpp:164-178`: a replica id **absent** from `other` leaves `self`'s value untouched, so the result keeps the higher value. The shipped caller is `NetworkSim::compact_all()` (`NetworkSim.cpp:168-177`). With ≥3 replicas where r0 has seen r2 but r1 never has, `meet` leaves r2's high value in the watermark; `Buffer::compact` then GCs r2's tombstones r1 never received → **permanent text divergence**, exactly what `compact()` exists to prevent.
**Evidence:** `Clock.cpp:164-178`:
```cpp
uint32_t other_val = other.get(rid);
if (other_val > 0 && other_val < self_val) {   // :174
    a[i] = pack(rid, other_val);
}
```
`NetworkSim.cpp:168-177`; `Buffer::compact` (`Buffer.cpp:192`).
**Cost to Corbomite:** Permanent text divergence, which the GC path exists to make impossible.
**Ask:** two-pass — collect the union of replica ids, drop every id not present in *all* replicas (so `observed()` is false → no GC), then meet. **Ship the missing 3-replica test.**
**Verified:** Read directly at the quoted lines; `entries`/compiler config checked, and the missing case confirmed by the five `compact_*` cases in `tests/tst_gc.cpp` (`:430`, `:459`, `:480`, `:500`, `:537`) all using exactly **two** replicas (collabtext handoff §0 and §2). The index files this ask under Priority 2 while the collabtext handoff calls §2 **P0**.

## Priority 1 — user-visible gaps and hot-path cost

### ASK-M7 · Markoff styled leaf: `Markoff::Theme` is plumbed all the way in and then **ignored**
**Severity:** P1 — the largest user-visible gap we have: a dark or user theme has no effect on content in Reading mode
**What happens:** `FormatPass.h:21` declares `const Markoff::Theme *theme = nullptr;` and `FormatPass.cpp` reads **only** `opts.fontScale`. Every colour is a literal, with comments naming the Theme slot they *should* use (`FormatPass.cpp:113,133,191,194,197,200,211-212`): `QColor(245,245,245) // Theme::CodeBlockBackground`, `QColor(100,100,100) // Theme::Quote.`, etc. **Two independent breaks:** `computeBlockHash` (`:217-221`) takes no theme, so even a theme-aware `FormatPass` would skip every block on a change; and `StyleApplier::setTheme` (`:39-43`) early-returns on *pointer* equality while `Styled::Editor::setTheme` always passes the same address (`Editor.cpp:49`). **Proof it is a bug, not a choice:** `StyledFindAdapter` *does* use the theme for its own highlights (`StyledFindAdapter.cpp:150-154`) — so find highlights re-theme and the body text does not.
**Evidence:** `src/FormatPass.h:21`
```cpp
const Markoff::Theme *theme = nullptr;
```
`src/FormatPass.cpp:425`
```cpp
const qreal fontScale = opts.fontScale;   // the ONLY field read
```
```cpp
bf.setBackground(QColor(245, 245, 245));   // :113  Theme::CodeBlockBackground
cf.setForeground(QColor(100, 100, 100));   // :133  Theme::Quote.
fmt.setBackground(QColor(255, 240, 130)); // :191  Theme::Highlight
fmt.setForeground(QColor(70, 130, 180));  // :194  Theme::Tag
... ? QColor(120, 80, 200) : QColor(40, 100, 200));  // :197,:200,:211-212  WikiLink : Link
```
Two independent breaks, either of which alone is sufficient: (1) **`computeBlockHash` takes no theme** (`FormatPass.cpp:217-221`) — its inputs are `(kind, text, spans, attrs, fontScale)`, so even a theme-aware `FormatPass` would skip every block on a theme change via the per-block hash gate at `:464-467`; (2) **`StyleApplier::setTheme` early-returns on *pointer* equality** (`StyleApplier.cpp:39-43`), while `Styled::Editor::setTheme` always passes the same address (`&m_theme`, `Editor.cpp:49`), so it never triggers a restyle anyway. `StyledFindAdapter` *does* use the theme — for its own highlights (`Detail/StyledFindAdapter.cpp:150-154`), so in the same leaf find highlights re-theme and body text does not.
**Cost to Corbomite:** we propagate a full `Markoff::Theme` to all three leaves. In Reading mode a dark theme and user themes have **no effect on content**, and the `// Theme::X` comments actively hide it. This is the largest user-visible gap we have. Our canvas dark-mode work (shipped 2026-10-02) has to build its own `CanvasPalette` because canvas items are ours; reading-mode content is yours.
**Ask:** 1. Read `opts.theme` at every colour site, with `opts.theme ? opts.theme->color(Slot::X) : Theme::defaultLight().color(Slot::X)` as the fallback so the renderer still works headless (we do render card previews offscreen). 2. Add `theme` to `computeBlockHash`'s inputs. 3. `StyleApplier::setTheme` should store the `Theme` **by value** and compare with a new `operator==`. 4. Add `bool operator==(const Theme&)` to `Theme` — its absence currently forces Corbomite to restyle every leaf on every theme propagation, forcing a full document re-render each time (`MarkdownView::setTheme`, `MarkdownView.cpp:48-53`, is unconditional and forces a full `StyleApplier::rerender()`, clearing `m_blockHashes` and re-running `FormatPass::apply` over the whole document each time). **Test:** set a theme with a distinct `CodeBlockBackground`, render a document containing a code block, assert the block's background is that colour.
**Verified:** confirmed as a bug rather than a choice because `StyledFindAdapter` does use the theme while the body text does not; request-in-progress text, `markoff-styled/src/Editor.cpp:423-426`, names the cause: *"No same-value guard: Markoff::Theme has no operator==."*

### ASK-M8 · Markoff: systemic raw-pointer lifetime gap — fixed only in the **retired** leaf
**Severity:** P1 — use-after-free crash class on completely ordinary Qt document teardown
**What happens:** Raw `MarkoffDocument *` with no `QPointer` and no `destroyed()` connection in:

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
**Evidence:** the table and the `markoff-live/src/EditorWidget.cpp:268-279` quotation above. It bites the styled leaf via `StyleApplier.cpp:60-64` and `StyledTableRenderer.h:23`; `MarkdownView` holds a raw `MarkoffDocument *` (`MarkdownView.h:105`, `MarkdownView.cpp:39-46`).
**Cost to Corbomite:** this is exactly the crash class we hit on 2026-06-10 (first-run SIGSEGV: a double `vaultRequested` tore the vault down under a live editor). We fixed it in our code *and* in markoff-live; the production leaves still have it. Any consumer that deletes a `MarkoffDocument` while a view is attached — completely ordinary Qt — is one Ctrl+Z or one mouse-move from a UAF.
**Ask:** 1. Lift the `EditorWidget` pattern into `MarkdownView::setDocument` itself so all leaves are fixed in one place. 2. Change `m_document` to `QPointer<MarkoffDocument>`. 3. Same for the five members above.
**Verified:** against the pinned source (index preamble: every finding was verified against the pinned source and is stated with `file:line` and quoted code rather than paraphrase); the fix pattern is quoted verbatim from the retired leaf, and `markoff-live` itself documents why.

### ASK-M9 · Markoff: no undo-availability query, so we must lie
**Severity:** P1 — wrong UI state: Undo/Redo stay enabled with an empty stack and never grey out
**What happens:** `MarkdownView` exposes `undo()`/`redo()` with **no availability query**; `MarkoffDocument::undoDepth()` (`MarkoffDocument.h:105`) reads the **legacy** stack and is always 0 on a D2 document; the D2 `UndoLog::entryCount()` is not surfaced.
**Evidence:** our workaround, in our code (`src/editor/MarkdownView.cpp:133-141`):
```cpp
bool MarkdownView::canUndo() const {
    // Markoff::MarkdownView's contract exposes undo()/redo() actions but
    // no undo-stack-depth query — hasEditing() (editable at all) is the
    // best available proxy until an upstream API addition.
    return canEdit();
}
```
**Cost to Corbomite:** so our Undo/Redo actions are enabled whenever the view is editable, **even with an empty stack, and never grey out.**
**Ask:** `bool canUndoD2() const; bool canRedoD2() const; qsizetype d2UndoDepth() const;` on `MarkoffDocument`, plus `virtual bool canUndo() const` / `canRedo() const` on `MarkdownView`.
**Verified:** against the pinned source (index preamble: every finding was verified against the pinned source and is stated with `file:line` and quoted code rather than paraphrase).

### ASK-M10 · Markoff: three public accessors are permanently dead on a D2 document
**Severity:** P1 — dead public API hands consumers confident wrong answers
**What happens:** `MarkoffDocumentPrivate.h:66-69` — `latestParse`, `latestBlockAnchors`, `latestBlockRanges` are **never assigned anywhere**. Consequently `parsedDocument()` always returns `nullptr`, and `blockAnchorAt()`, `blockByteRange()`, `blockAt()` always return `std::nullopt` after `loadFromMarkdown()`. The header documents them as ordinary working APIs (`MarkoffDocument.h:136,141-149`). This class of problem *did* get `[[deprecated]]` on `toMarkdownUtf8()` / `toMarkdown()` (`:81-84`), so the treatment is inconsistent.
**Evidence:** **downstream evidence of harm:** the styled leaf's `CLAUDE.md` carries a section titled *"**D2-broken core APIs to avoid**… When in doubt about a core API, search `latestBlockRanges` first"*. Also `blockByteRange` (`:842-845`) bounds its loop on `latestBlockAnchors.size()` but indexes `latestBlockRanges[i]` — a latent OOB.
**Cost to Corbomite:** a consumer reading only the public header gets confident wrong answers.
**Ask:** 1. Populate the three members, or mark all four accessors `[[deprecated("…D2 documents are unindexed")]]` and add `bool isBlockIndexValid() const`. 2. Fix the latent OOB in `blockByteRange` (`:842-845`), which bounds its loop on `latestBlockAnchors.size()` but indexes `latestBlockRanges[i]`.
**Verified:** against the pinned source (index preamble: every finding was verified against the pinned source and is stated with `file:line` and quoted code rather than paraphrase).

### ASK-M11 · Markoff: `SearchEngine::FindFlags` advertises Regex / WholeWords / Backwards; only CaseSensitive exists
**Severity:** P1 — silently wrong search results in every user build
**What happens:** `SearchEngine.h:27-33` declares all four; `SearchEngine.cpp:25-26` guards the other three with `Q_ASSERT`, which **vanishes under `NDEBUG`**. A "whole word" search silently returns substring matches and a "regex" search silently does a literal search, in every user build. `Backwards` isn't even asserted. `FindController.h:54-56` actively misdocuments it: *"Whole-words / regex are accepted by the underlying SearchEngine but UI work is deferred."* **They are not accepted.**
**Evidence:** `SearchEngine.h:27-33` declares all four; `SearchEngine.cpp:25-26` guards the other three with `Q_ASSERT`; `FindController.h:54-56` misdocuments them as accepted.
**Ask:** implement them, or remove them from the enum, add `bool supports(FindFlag)`, and fix the comment.
**Verified:** against the pinned source (index preamble: every finding was verified against the pinned source and is stated with `file:line` and quoted code rather than paraphrase).

### ASK-M12 · Markoff: `onSaveComplete` waits forever for acks that a single-user consumer never sends — so GC never runs and memory grows forever
**Severity:** P1 — unbounded memory growth plus a per-keystroke cost that grows monotonically for the life of the document
**What happens:** `MarkoffDocument.cpp:2598-2609` only compacts when `ackedWatermark >= W`; otherwise, for a `collabConfigured` document with no peers, it emits `wantsAcksAtWatermark(W)` and **waits forever**. **We land in exactly that branch**: `NoteDocument.cpp:41-42` constructs `MarkoffDocument(quint16{1}, …)` → `collabConfigured == true`; we write files ourselves via `serializeForSave()` and never call `notifyAcksAtWatermark` or `triggerGc`. Consequence: **orphan per-block CRDT Buffers, BufferProxies, `blockLoadTimeBytes` and `blockEditSequences` accumulate for every block ever created, forever.** `collectGarbage()`/`compact()` do not help — they touch only the dead legacy flat buffer (`:969-977`). **This compounds a per-keystroke cost:** `d2EditSequence()` (`MarkoffDocument.cpp:1171-1177`) sums `blockEditSequences` over **every block ever created**, and `scheduleD2Changed()` calls it on every edit. Because `blockEditSequences` is only pruned by `disposeOrphans()` — which never runs — **our typing cost grows monotonically for the life of the document.** We call it a second time in `NoteDocument::hasUnsavedEdits()` (`:130`).
**Evidence:** `MarkoffDocument.cpp:2598-2609` (compact only when `ackedWatermark >= W`); `NoteDocument.cpp:41-42`; `MarkoffDocument.cpp:1171-1177`; `NoteDocument::hasUnsavedEdits()` (`:130`); `:969-977` (`collectGarbage()`/`compact()` touch only the dead legacy flat buffer).
**Cost to Corbomite:** memory grows forever for every block ever created, and typing cost grows monotonically for the life of the document.
**Ask:** (a) keep a running total incremented alongside the per-block counter; (b) in `onSaveComplete`, compact immediately when there are no peers; (c) make `collectGarbage()`/`compact()` delegate to `WatermarkCoordinator`; (d) document that `save()` can drain Ctrl+Z — `undoLog.compact(all=true)` is `WatermarkCoordinator.cpp:53-54` and is tested as intended, but nothing in the public header says so.
**Verified:** against the pinned source (index preamble: every finding was verified against the pinned source and is stated with `file:line` and quoted code rather than paraphrase); `undoLog.compact(all=true)` (`WatermarkCoordinator.cpp:53-54`) is tested as intended.

### ASK-M13 · Markoff: ~6 full-document materialisations **per keystroke**
**Severity:** P1 — hot-path cost: ~6 full-document materialisations per keystroke
**What happens:** `SourceTextDocumentBinding.cpp:456-462` (`onQtContentsChange`, i.e. per character typed) calls `widgetFlatView()`, then `QString::fromUtf8`, then `toPlainText()`, then `findBlockAtSepByte` **twice**. `widgetFlatView()` is not cached — it is `iterateBlocks()` plus a fresh `QByteArray::fromStdString()` per block (`:1159-1164`), so one CRDT-buffer materialisation and heap allocation per block, repeatedly. Structural keys do it again (`:390`), and `sepViewPosOf` (`:307`) converts every block just to get its **length** while being called from a function that walks the document three more times.
**Evidence:** `SourceTextDocumentBinding.cpp:456-462`; `:1159-1164`; `:390`; `:307`.
**Ask:** (a) a cached flat view on `MarkoffDocument` invalidated on edit-sequence change; (b) a block-index cache (`QVector<quint32> blockByteOffsets_`) so sep↔block resolution is a binary search; (c) coalesce `onQtContentsChange` into one queued handler.
**Verified:** against the pinned source (index preamble: every finding was verified against the pinned source and is stated with `file:line` and quoted code rather than paraphrase).

### ASK-M14 · Graffodil: no theming surface at all
**Severity:** P1 — no theming surface; consumers are forced to re-implement Graffodil's colours
**What happens:** Five hardcoded colours: `GraphEdgeItem.cpp:353` (selection `QColor(0,120,215)`), `CreateEdgeTool.cpp:96`, `SelectMoveTool.cpp:145-146,199`, `AnchorHighlight.cpp:77`, `GraphMinimap.cpp:56-57,89-90`, `ConnectorItem.cpp:12-13`. Worse, `BatchRenderer.cpp:112` assigns `QColor(100,150,200)` **unconditionally**, so a consumer that themes individual nodes gets uniform blue in batch mode with no API to prevent it.
**Evidence:**

| Location | Role |
|---|---|
| `GraphEdgeItem.cpp:353` | selection `QColor(0, 120, 215)` |
| `CreateEdgeTool.cpp:96` | preview line |
| `SelectMoveTool.cpp:145-146, 199` | rubber band, guides |
| `AnchorHighlight.cpp:77` | accent |
| `GraphMinimap.cpp:56-57, 89-90` | dark chrome |
| `ConnectorItem.cpp:12-13` | grey |
| **`BatchRenderer.cpp:112`** | `nd.color = QColor(100, 150, 200)` — assigned **unconditionally** |

**Direct evidence this bit us:** our `CanvasPalette.h:12` says canvas had *"zero theme awareness: ~15 hardcoded literals"* — and our `ReconnectEdgeTool.cpp:78` **re-hardcodes the identical `QColor(0,120,215)`** because Graffodil's own connect-preview line is not themeable.
**Cost to Corbomite:** Corbomite's canvas dark-mode work (shipped 2026-10-02) had to build a 14-role `CanvasPalette` (`libs/canvas/include/canvas/CanvasPalette.h`); we are reimplementing your colour in our tool because we have no seam.
**Ask:** add a `Graffodil::Palette` struct plus `GraphScene::setPalette()` and `GraphEdgeItem::setSelectionColor()`, threaded to every site above; for `BatchRenderer`, an optional `std::function<QColor(const IGraphNode*)>` colour provider so batch mode can honour per-node theming. **We can ship our `CanvasPalette` as the worked example.**
**Verified:** against the pinned source (index preamble: every finding was verified against the pinned source and is stated with `file:line` and quoted code rather than paraphrase); our own `CanvasPalette` shipped 2026-10-02 and `ReconnectEdgeTool.cpp:78` re-hardcodes the identical literal.

### ASK-M15 · Graffodil: no interaction path is sub-linear, despite shipping `Spatial` for exactly that
**Severity:** P1 — hot-path cost: an O(items × nodes) scan plus per-node `anchors()` allocation on every mouse-move
**What happens:** Every mouse-move does an O(items × nodes) scan plus an `anchors()` allocation per node: `CreateEdgeTool.cpp:51-63` and `:175-183`, `SelectMoveTool.cpp:22-46`, `CompositeTool.cpp:58-75` (`isNearAnchor`, on every move), `GraphScene.cpp:634-640`. `nodes()` returns `QHash::values()` — a fresh `QList` per call. **`Graffodil::Spatial` (QuadTree + SpatialGrid) has zero consumers inside `Graffodil::Core`.** The index was built and never wired to the paths that need it. (Our own audit nearly deleted `SpatialGrid` as dead code before finding it was a member of `BatchNodeItem` — see the punch list.)
**Evidence:** `CreateEdgeTool.cpp:54-61`:
```cpp
for (auto *item : items)
    for (auto *node : m_scene->nodes())        // QHash::values() — fresh QList, heap alloc
        if (node->graphicsItem() == item || node->graphicsItem()->isAncestorOf(item))
```
and the fallback at `:175-183` iterates all nodes × all anchors, calling `node->anchors()` — which **constructs a fresh `QList<Anchor>` per node per mouse-move**. `CompositeTool::isNearAnchor` has an AABB pre-reject (`:64-67`) but still calls `sceneBoundingRect()` and `anchors()` per candidate, plus a `QLineF(...).length()` (a `std::hypot`) per anchor. `CompositeTool::mouseMoveEvent:185` broadcasts to **all** sub-tools, compounding it. Note the lesson recorded from our own audit: a symbol whose only references are in a header you did not read is not dead — `SpatialGrid` is live, as a member of `BatchNodeItem`.
**Ask:** build a QuadTree over node bounding rects in `GraphScene`, invalidated on structural change, and route all five call sites through one `GraphScene::nodesAt(scenePos, radius)`. Cache `anchors()` per node.
**Verified:** against the pinned source (index preamble: every finding was verified against the pinned source and is stated with `file:line` and quoted code rather than paraphrase); our own audit nearly deleted `SpatialGrid` as dead code before finding it was a member of `BatchNodeItem`.

### ASK-M16 · Markoff: `SearchEngine::findByBlock` is O(len × matches) in the case-insensitive path
**Severity:** P1 — hot-path cost: O(len × matches) re-serialisation on every keystroke in the find box
**What happens:** `SearchEngine.cpp:53-64` re-serialises the whole block prefix to UTF-8 **twice per match** (`blockStr.left(u16idx).toUtf8().size()`). Searching `"e"` in one 100 KB paragraph with 2 000 matches is ~4 000 allocations over 200 MB of copying. `FindController` runs this on **every keystroke** in the find box and re-runs on every `d2DocumentChanged` with no debounce.
**Evidence:** `SearchEngine.cpp:53-64`; `blockStr.left(u16idx).toUtf8().size()` re-serialises the whole block prefix to UTF-8 twice per match.
**Ask:** track a running UTF-16→UTF-8 byte offset as you advance, and debounce `recomputeMatches` behind a ~150 ms timer.
**Verified:** against the pinned source (index preamble: every finding was verified against the pinned source and is stated with `file:line` and quoted code rather than paraphrase).

### ASK-M17 · Markoff: `LinkInteraction` mixes two coordinate spaces and does O(document) work at mouse-event rate
**Severity:** P1 — wrong link resolution (one byte too large per crossed block boundary) plus O(document) work at mouse-event rate
**What happens:** `LinkInteraction.cpp:53-56` computes a byte offset from `toPlainText()` — which is the **separator** view — then passes it to `textAnchorAt()`, which accumulates **no-separator** lengths (`MarkoffDocument.cpp:773-789`). The offset is therefore **one byte too large for every block boundary already crossed**, so hovering or clicking a link in block *k* resolves *k* bytes past the target. Independently, `handleMove` (`:108-125`) runs on **every** `MouseMove`: a full document copy, another prefix copy, an `iterateBlocks()` walk, `blockText()`, and `inlineSpansFor()` — O(document) at 60–120 Hz, with no throttle.
**Evidence:** `LinkInteraction.cpp:53-56, 66-67`; `MarkoffDocument.cpp:767-790` / `:773-789`; `handleMove` (`:108-125`).
**Ask:** map the `QTextBlock` number to a `BlockId` via the existing `walkBlocks()`, or use `findBlockAtSepByte`; throttle `handleMove` and bail early when the caret's block is unchanged.
**Verified:** against the pinned source (index preamble: every finding was verified against the pinned source and is stated with `file:line` and quoted code rather than paraphrase).

### ASK-M18 · Markoff: O(N²) structural operations
**Severity:** P1 — algorithmic complexity: pasting N paragraphs is O(N²) and Replace-All is O(matches × N)
**What happens:** `MarkoffDocument.cpp:1446-1453` — `d2InsertBlock` calls `iterateBlocks()` on **every** block insert just to compute a row; `:1836-1841` does the same for removal. `applyFlatEdit`'s multi-block path calls both, so **pasting N paragraphs is O(N²)**. `:1782-1830` — `replaceMatches` calls `applyFlatEdit` once per match, so **Replace-All is O(matches × N)**.
**Evidence:** `MarkoffDocument.cpp:1446-1453`; `:1836-1841`; `:1782-1830`.
**Ask:** let `IdList` report the index directly (or cache a `BlockId → index` map), and give `replaceMatches` a single-transaction multi-range primitive.
**Verified:** against the pinned source (index preamble: every finding was verified against the pinned source and is stated with `file:line` and quoted code rather than paraphrase).

### ASK-M19 · Graffodil: `SugiyamaLayout` layer assignment is a single pass, so any cycle silently corrupts layering
**Severity:** P1 — silent layering corruption on any cycle, producing a too-small layer and unroutable long edges
**What happens:** `SugiyamaLayout.cpp:437-439` — Kahn's algorithm returns a **partial** order when a cycle exists; the code only handles the all-or-nothing case (`if (topoOrder.isEmpty())`). A single self-loop or 2-cycle leaves the order non-empty but incomplete, missing nodes are appended **arbitrarily** (`:478-481`), and the longest-path pass (`:486-494`) then runs once with `if (layer.contains(pred))` silently skipping predecessors that appear later — yielding a **too-small layer**, edges with `span <= 0`, and long edges routed with no waypoints. **Canvas files and dependency graphs legitimately contain cycles**, and ours do.
**Evidence:** `src/sugiyama/src/SugiyamaLayout.cpp:437-439, 478-481, 486-494`
```cpp
QList<QString> topoOrder = topologicalSort(allTasks, successors, predecessors);
if (topoOrder.isEmpty()) topoOrder = allTasks;      // :438  handles all-or-nothing only
```
Edges then get `span <= 0`, dummy vertices are never inserted for them, and long edges are routed with no waypoints at all.
**Cost to Corbomite:** canvas files and dependency graphs legitimately contain cycles, and ours do.
**Ask:** relax layer assignment to a fixed point (`do { … } while (changed);`). **Test:** one self-loop plus a 3-node chain, asserting the chain's layering is unaffected by the loop.
**Verified:** against the pinned source (index preamble: every finding was verified against the pinned source and is stated with `file:line` and quoted code rather than paraphrase).

### ASK-M23 · Markoff: styled `recomputeContext()` has no table-frame guard
**Severity:** P1 — wrong `blockKind`/`headingLevel` past the first table drives the wrong UI actions
**What happens:** `Editor.cpp:489-527` maps `cursor.position()` (QTextDocument coordinates, which include frame structure characters) through `doc->widgetFlatView()` (raw markdown source bytes). Past the first table frame every position resolves to the wrong block, so `contextChanged` fires with a wrong `blockKind`/`headingLevel`. The format verbs *do* guard via `selectionInTrustworthyRegion` (`:265-292`) — the context path just forgot. `tableRow`/`tableCol` are also never populated by any leaf.
**Evidence:** `Editor.cpp:489-527`, `:265-292`; `EditorContext.h:21-22` declares `tableRow`/`tableCol`, both left at `-1`.
**Cost to Corbomite:** `contextChanged` is what drives our whole context-sensitive action layer. A wrong `blockKind` means Format menu entries enable and disable against the wrong block.
**Ask:** reuse `selectionInTrustworthyRegion`, and populate `tableRow`/`tableCol`.
**Verified:** Verified against the pinned source per the index preamble; the sites are quoted from the markoff-styled handoff §2.

### ASK-M24 · Markoff: source leaf lacks the list/quote marker restore that styled has
**Severity:** P1 — Copy-as-Markdown/HTML from Source silently drops list/quote markers
**What happens:** `markoff-styled/src/StructuralTextEdit.cpp:44-81` prepends `listItemDisplayMarker`/`blockQuoteDisplayMarker`; **`Source::InnerEditor.cpp:33-38` does not.** So Copy-as-Markdown/HTML from **Source** mode emits marker-less content — which, as styled's own comment says, pastes into LibreOffice as Body Text rather than a List/Block Quote.
**Evidence:** `markoff-styled/src/StructuralTextEdit.cpp:44-81`; `markoff-source/src/InnerEditor.cpp:33-38`; styled comment:
> *"without this, Copy as HTML/RTF of a loaded quote/list re-parses bare content as a plain paragraph and pastes into e.g. LibreOffice as Body Text, not a Block Quote/List."*
**Cost to Corbomite:** Corbomite ships all four clipboard verbs on both leaves (Cluster N), so this reaches users.
**Ask:** lift it into a shared helper in `markoff-core` and call it from both.
**Verified:** Verified against the pinned source per the index preamble; the sites are quoted from the markoff-styled handoff §3.

### ASK-M30 · markoff-parser: UTF-8 lead byte trusted without validation
**Severity:** P1 — silent offset skew on Latin-1 or truncated UTF-8
**What happens:** `TreeSitterParser.cpp:205-225` infers sequence length from the lead byte alone, with no check that the following bytes are continuation bytes and no overlong/surrogate check. A lone `0xC3`, a truncated sequence, or a Latin-1 encoded byte desynchronises `charIdx` from `QString`'s real char count — and `utf8ToCharOffset()` (`:288-292`) then feeds headings, links, tags, footnote refs and inline spans.
**Evidence:** `TreeSitterParser.cpp:205-225`; `utf8ToCharOffset()` `:288-292`.
**Cost to Corbomite:** We parse real user vault files, and Obsidian tolerates Latin-1 notes. The offset skew is silent.
**Ask:** validate continuation bytes, treat invalid sequences as 1 byte, and add a release-build assertion that the final `charIdx == text.size()`. **Test:** Latin-1 fixture and a truncated multibyte sequence.
**Verified:** Read directly at the quoted lines (markoff-parser handoff §3); no reproduction is claimed.

### ASK-M31 · markoff-parser: whole-document reparse, no incremental path
**Severity:** P1 — O(document) work per parse with incremental reuse disabled
**What happens:** `TreeSitterParser.cpp:247-284` re-encodes the whole document and builds a fresh `4N`-byte `QList<int>` map every call, and passes `nullptr` as the previous tree — **disabling tree-sitter's incremental reuse entirely**. `buildByteToCharMap` zero-fills then overwrites, so every byte is written twice.
**Evidence:** `TreeSitterParser.cpp:247-284`; `m_utf8 = text.toUtf8()` `:248`; `m_byteToChar = buildByteToCharMap(m_utf8)` `:249`; `ts_parser_parse_string(..., nullptr, ...)` `:256`; `QList<int> map(utf8.size() + 1, 0)` `:207`.
**Cost to Corbomite:** We call `Document::fromMarkdown()` per file (`MetadataParser.cpp:301`), and `libs/bases` re-parses on every filter evaluation.
**Ask:** pass `m_blockTree` as the previous tree; expose `bool reparse(const QString&, quint32 editOffset, …)`; `resize()` instead of zero-fill; build the map incrementally.
**Verified:** Read directly at the quoted lines (markoff-parser handoff §4); no reproduction is claimed.

### ASK-M35 · markoff-parser: frontmatter closing-fence `\n---` match lacks EOL check
**Severity:** P1 — false-close on `----`, `--- `, or `---foo` silently corrupts the frontmatter/body split on round-trip
**What happens:** `Document::extract()` searches for the closing fence using `source.indexOf(QStringLiteral("\n---"), 3)`. The search string `"\n---"` is a **prefix match only** — it matches any line that *starts* with `---`, including `----`, `---<!-- end -->`, `--- ` (trailing space), etc. The CommonMark spec and Obsidian both require the closing frontmatter fence to be **exactly** `---` on its own line (followed by `\n`, `\r\n`, or EOF). A document whose intended closing fence is `----`, `--- `, or `---foo` has that line wrongly accepted; `afterFm = endPos + 4` then eats three of the four dashes / the dashes-plus-marker and mangles the start of the body. `markdownContent()` (`:191`) has the same unfenced search, and `withFrontmatter()` (`:403`) calls it, so a Corbomite link-rewrite round-trip writes the mangled split back to disk permanently.
**Evidence:** `Document.cpp:42`:
```cpp
int endPos = source.indexOf(QStringLiteral("\n---"), 3);
```
`Document.cpp:191` (same in `markdownContent()`); `withFrontmatter()` `:403`.
**Cost to Corbomite:** FileManager link rewriting calls `withFrontmatter(parsedFrontmatter())`; Case A's intended `----` line is permanently reduced to a lone `-` and a spurious correct close is written, with the YAML itself intact so nothing surfaces the corruption until the body is inspected.
**Ask:** check that the character after `\n---` is `\r`, `\n`, or EOF, via a shared `findClosingFence()` helper at both call sites (`:42`, `:191`); the `:57` EOF branch is already correct. **Acceptance:** add the four failing cases from the handoff to `tst_frontmatter.cpp` — `----`, `--- `, `---foo`, and the round-trip-preservation Case D.
**Verified:** Traces were derived directly from `extract()` and checked against the code, not invented; the handoff supplies a falsifiable `tst_frontmatter` oracle (Cases A–D). The 2026-10-01 handoff (§4) records that the pin moved ~2 months to `5d8850369` and the line was unchanged; confirmed still unchanged at the current pin `dca64686` (`Document.cpp:42`).

### ASK-M37 · Markoff: a canvas text card cannot be edited without either a `QTextEdit` proxy or forking the editor
**Severity:** P1 — blocks the card-editor swap, the last canvas authoring gap we cannot close host-side

**What we are asking for.** We want to put an **embeddable, card-sized editor**
over a canvas card's rectangle in a `QGraphicsProxyWidget`. `Canvas::EditorWidget`
is the right leaf, and it is worth saying why before the ask: its constructor
(`EditorWidget.cpp:89-102`) only parents a `View` and a `CanvasActionController`
into a zero-margin `QVBoxLayout`. **There is no central-editor assumption to
argue about** — no toolbar, no minimum size, no `setSizePolicy` override
anywhere in the leaf. What is missing is a *documented* way to embed it at card
size, plus one behaviour we cannot express with what is public.

**Evidence.**
- `EditorWidget.cpp:89-102` — constructor: `new View(this)`, an action
  controller, and `QVBoxLayout` with `setContentsMargins(0,0,0,0)` and
  `setSpacing(0)`. Grepped `EditorWidget.cpp` and `View.cpp` for
  `setMinimumSize` / `minimumSizeHint` / `setSizePolicy`: no hits.
- `markoff/core/MarkdownView.h:14-40` — the base is a `QWidget` with a fully
  virtual contract (`setDocument`, `setCursorPosition`, `setReadOnly`,
  `caretRect`, …) and no size policy of its own. Embeddable in principle;
  nothing says so.
- `markoff/styled/DocumentRenderer.h:33-42` — **this half of the 2026-10-17 ask
  is already delivered.** Both `renderInto` overloads are public and `const`, one
  taking `const MarkoffDocument *`, one taking bytes; the header itself documents
  the per-frame pattern ("own a `QTextDocument`, renderInto it once, and
  paint/measure it directly"). Our paint path already uses the pointer overload
  and never re-imports a `ready` document, so there is no per-frame identity
  minting. **No ask here** — we struck it from the older document's live status
  rather than re-asking it.

**What we still need.**
1. **A documented embedded/card mode.** Either a constructor flag on
   `EditorWidget` (host-controlled size, read-only until the card is the one
   being edited) or a header statement that the leaf is embeddable as-is and
   the host owns geometry. We will not fork a second editor.
2. **Scroll ownership when embedded.** `View` is a `QAbstractScrollArea`; a card
   must not scroll itself independently of the canvas zoom.
   `setScrollPositionVisualLine`/`scrollPositionVisualLine` are public and
   virtual (`MarkdownView.h:24-25`), so we can drive it — but there is no flag to
   suppress the scrollbars or hand scrolling to the host.

**Cost to Corbomite:** today's card editing is a `QTextEdit` proxy — plain text
in, plain text out, stored back into the node on focus loss. So a card gets no
Markdown editing at all: no live formatting, no link interaction, no block
structure. That is the whole remaining gap in canvas authoring; the rest of
Cluster M is host-side work we have already landed.

**Ask:** items 1 and 2. We would rather be told this leaf is deliberately
single-purpose than discover it by crashing.

**Verified:** `EditorWidget.cpp:89-102` and the absence of size-policy calls read
directly at the pin `dca64686`; `MarkdownView.h:14-40` and
`DocumentRenderer.h:33-42` quoted. The "no ask on repaint" half was verified by
re-reading the renderer header against item 3 of the 2026-10-17 handoff, which is
now satisfied.

## Priority 2 — API gaps and smaller correctness

### ASK-M20 · Markoff: `Session` is not on the polymorphic base contract
**Severity:** P2 — API gap that forces consumers to know the concrete leaf type
**What happens:** `MarkdownView.h:14-109` has **no** session accessor. `Styled::Editor` has `session()`/`setSession()` + a Q_PROPERTY; `Source::Editor` has **neither** — it manages a private `QPointer<Session>` with no accessor. We hold leaves as `Markoff::MarkdownView *` (`NoteEditorWidget.h:105`) and so cannot get or set a session on a Source leaf at all.
**Evidence:** `MarkdownView.h:14-109`; `Styled::Editor` `session()`/`setSession()` + Q_PROPERTY; `Source::Editor` private `QPointer<Session>` with no accessor; `NoteEditorWidget.h:105`.
**Cost to Corbomite:** We hold leaves as `Markoff::MarkdownView *`, so touching sessions requires the concrete type and a Source leaf cannot be reached at all.
**Ask:** add `virtual Session *session() const` / `virtual void setSession(Session *)` to `MarkdownView`.
**Verified:** Verified against the pinned source per the index preamble; the sites above are quoted directly from the markoff-styled handoff §4.

### ASK-M21 · Markoff: session lifetime leaks
**Severity:** P2 — one leaked session per leaf teardown; a death signal that never fires
**What happens:** `Source::Editor` creates a `Session` in `setDocument` (`Editor.cpp:150-153`), destroys it only on a **document swap** (`:116-126`), and `~Editor()` is `= default`. One leaked session per leaf teardown. **The retired live leaf fixed exactly this** (`EditorWidget.cpp:210-216`) and says why. Separately, `Styled::Editor::setSession` (`:412-417`) never emits `sessionChanged` when the session dies, because the `QPointer` is already null and it early-returns — so a consumer bound to that signal never learns.
**Evidence:** `Source::Editor` `setDocument` `Editor.cpp:150-153`, swap `:116-126`, `~Editor() = default` (`:113`); retired fix `markoff-live/src/EditorWidget.cpp:210-216`:
```cpp
// Session owned by document — … explicitly destroy the session we created so
// the document doesn't accumulate ghost sessions across leaf swaps.
if (d->session && document()) document()->destroySession(d->session);
```
`Styled::Editor::setSession` `Editor.cpp:412-417`; `m_session` is a `QPointer` (`Editor.h:125`).
**Cost to Corbomite:** A consumer bound to `sessionChanged` never learns the session is gone.
**Ask:** destroy the owned session in `~Editor()`; connect to `QObject::destroyed` and null+emit. Consider a `virtual Session *createOwnedSession()` hook on `MarkdownView` so no leaf has to remember.
**Verified:** Verified against the pinned source per the index preamble; the quoted sites are from the markoff-styled handoff §5.

### ASK-M22 · Markoff: process-global `static` BlockId counters
**Severity:** P2 — cross-document `BlockId` aliasing
**What happens:** `MarkoffDocument.cpp:1066,1420,1338` are function-local `static std::atomic`s. `allocateD2BlockId()` (`:1920`) is correctly per-document; these three are not, so **two open notes mint identical `BlockId`s** for structurally-inserted blocks.
**Evidence:** `MarkoffDocument.cpp:1066,1420,1338`; `allocateD2BlockId()` `:1920`; `MarkoffDocumentPrivate.h` is the per-document home.
**Cost to Corbomite:** Nothing in the library compares IDs across documents today, but any host-side index keyed by `BlockId` — fold/collapse state, a remote-cursor registry, a persisted ephemeral-state map — will alias.
**Ask:** move all three onto `Private`.
**Verified:** Verified against the pinned source per the index preamble; the sites are quoted from the markoff-core handoff §8.

### ASK-M25 · Markoff: `StyledFindAdapter` is not re-wired on document swap, and re-walks the document per signal
**Severity:** P2 — find highlights silently stop refreshing after a swap; per-signal full-document walks
**What happens:** `Editor.cpp:82-175` re-wires the binding, style applier, `LinkInteraction` and both context connections — but never touches `m_findAdapter`, whose `m_docChangedCon` is bound to one `MarkoffDocument` (`:78-81`). After a swap, find highlights stop refreshing unless the consumer performs a documented "detach → swap → re-attach" dance. Separately, `renderHighlights` (`StyledFindAdapter.cpp:136-180`) is wired to `matchesChanged`, `currentMatchChanged`, `themeChanged` **and** `d2DocumentChanged`, and each fire walks every block.
**Evidence:** `Editor.cpp:82-175`, `:78-81`; `Detail/StyledFindAdapter.cpp:136-180`, `:68-85`, `:159-162`; per-match `QString::fromUtf8(buf.left(off))` at `:33-37`.
**Cost to Corbomite:** Every `findNext()` and every `d2DocumentChanged` is O(document + matches).
**Ask:** re-wire the adapter in `setDocument`; cache walk entries keyed on `blockEditSequence`.
**Verified:** Verified against the pinned source per the index preamble; the sites are quoted from the markoff-styled handoff §6.

### ASK-M26 · Markoff: export macros are no-ops while the libs are `SHARED`
**Severity:** P2 — libraries cannot be consumed on MSVC and break with `-fvisibility=hidden`
**What happens:** `MarkoffCoreExport.h:4` and `MarkoffStyledExport.h:4` are `#define … (empty)` with comments saying *"built STATIC"* — **both are `SHARED`** (`markoff-core/CMakeLists.txt:17`, `markoff-styled/CMakeLists.txt:11`). `markoff-source` has **no export header at all**. `Markoff::UndoLog` — which consumers must instantiate to use the D2 API — has **no** macro, nor do `FormatOps`, `Cmd::*`, or the `Detail::` helpers.
**Evidence:** `MarkoffCoreExport.h:4`; `MarkoffStyledExport.h:4`; `markoff-core/CMakeLists.txt:17`; `markoff-styled/CMakeLists.txt:11`; no export header in `markoff-source`.
**Cost to Corbomite:** Latent on Linux/GCC (no `-fvisibility=hidden`, so everything exports by luck), but the libraries **cannot be consumed on MSVC** and **will fail to link the moment anyone adds `-fvisibility=hidden`**.
**Ask:** populate the macros with the usual `Q_DECL_EXPORT`/`Q_DECL_IMPORT` switch and annotate every public symbol.
**Verified:** Verified against the pinned source per the index preamble; the sites are quoted from the markoff-core handoff §8.

### ASK-M27 · Build friction: demo apps built unconditionally; no installable SDK
**Severity:** P2 — every consumer builds uninstalled demos and has no consumable SDK
**What happens:**
1. `add_subdirectory(app)` is ungated in `markoff-source/CMakeLists.txt:54`, `markoff-styled:66`, `markoff-live:150`. `MARKOFF_BUILD_APPS` only covers `apps/`, so **every consumer builds two demo apps** it never installs.
2. `markoff_live`'s PUBLIC include dir is a raw source path (`markoff-live/CMakeLists.txt:121-124`) — commit `17a8376f` wrapped exactly this for canvas/source/styled and **missed live**, so flipping `MARKOFF_BUILD_LIVE=ON` breaks `install(EXPORT)` validation.
3. **No `install(DIRECTORY …/include)`** — libs install but headers do not, so there is no consumable SDK and only in-tree subdir builds work.
4. `markoff_core` links `collabtext` and `Markoff::Parser` as **PUBLIC** and `MarkoffDocument.h:18-20` includes `<crdt/…>`, so every consumer inherits collabtext's tree — while `markoff-core/CLAUDE.md` claims *"Public-boundary types (no `<crdt/...>` dependency)"*. **The claim is false at the header level.**
**Evidence:** `markoff-source/CMakeLists.txt:54`; `markoff-styled/CMakeLists.txt:66`; `markoff-live/CMakeLists.txt:150`, `:121-124`; `markoff-core/CMakeLists.txt:144-152`; `MarkoffDocument.h:18-20`; commit `17a8376f`. Graffodil has the same class of problem (`src/core/CMakeLists.txt:74-81` installs the library but no headers and no export set), and jkqtmathtext too.
**Cost to Corbomite:** Every consumer builds two demo apps it never installs; there is no consumable SDK and every consumer inherits collabtext's include tree.
**Verified:** Verified against the pinned source per the index preamble; the sites are quoted from the markoff-core handoff §9.

### ASK-M28 · Test infrastructure across the whole family
**Severity:** P2 — missing labels and missing regression tests let known bugs survive
**What happens:**
- **No `LABELS` on any test in markoff-core/source/styled/live.** `ctest -L benchmark` cannot exclude anything. (The canvas perf labels were already filed.)
- **All four `tst_d5_two_doc_convergence.cpp` cases do one `d2ApplyBufferEdit` per transaction** — which is precisely why ASK-M2 survived.
- `markoff-live/tests/tst_live_render_doc_destroyed.cpp` exists **for the retired leaf**; there is no equivalent for the production leaves, which is why ASK-M8 survived.
- `perf_load_bench` is not registered with ctest at all.
- Graffodil: 16 tests, `ctest -N` in our tree matches **zero**.
- collabtext's 50 tests *do* run in our ctest — but `enable_testing()` and all 50 targets are unconditional (`collabtext/CMakeLists.txt:93`), so any project vendoring it builds them whether it wants them or not.
- `markoff-source/CLAUDE.md:27` documents `Markoff::Source::FindBar` — it does not exist.
- `UndoLog::lastEntry()` is public, returns `m_entries.back()` with no empty-guard, and hands out a reference into a vector `undo()`/`compact()` mutate.
**Evidence:** `tst_d5_two_doc_convergence.cpp` (four cases, one `d2ApplyBufferEdit` each); `markoff-live/tests/tst_live_render_doc_destroyed.cpp`; `collabtext/CMakeLists.txt:93`; `markoff-source/CLAUDE.md:27`; `UndoLog.h:70`.
**Cost to Corbomite:** The gaps are why ASK-M2 and ASK-M8 survived, and "the suite is green" is an unreliable signal.
**Verified:** Verified against the pinned source per the index preamble; the sites are quoted from the markoff-core handoff §10 and the Graffodil handoff §8.

### ASK-M32 · markoff-parser: `PerfProbe` costs a mutex and a heap allocation per site, in every build
**Severity:** P2 — per-parse mutex/allocation cost that perturbs the measurements
**What happens:** `PerfProbe.h` — `record()` takes a `QMutexLocker` then `QString::fromLatin1(name)`. The header claims sites are *"benign to leave in non-benchmark builds"*; they are not. Worse, `MARKOFF_PERF_SCOPE("parser.inline_grammar_one_range")` sits **inside** the per-inline-region loop, so a 500-heading document costs 500 mutex round-trips per parse — perturbing the numbers it measures.
**Evidence:** `include/markoff/parser/PerfProbe.h`; `MARKOFF_PERF_SCOPE(...)` at `TreeSitterParser.cpp:279`.
**Cost to Corbomite:** 500 mutex round-trips per parse on a 500-heading document, perturbing the numbers the probe exists to measure.
**Ask:** compile out unless `MARKOFF_PERF` is defined; key the map on `quintptr` (string-literal address) instead of `QString`.
**Verified:** Read directly at the quoted lines (markoff-parser handoff §5); no reproduction is claimed.

### ASK-M38 · Markoff: one document key gets one session — so a card's paint session and its edit session collide
**Severity:** P2 — a hard one-session-per-key limit that decides our card architecture

**What happens:** `EphemeralProvider::open` refuses a second session for a
document key that already has one, and the key is **document-wide, not
epoch-wide**. `documentLeaseKey` returns `scope.documentKey` alone, with the
reason in a comment: *"The service lease is document-wide, not epoch-wide. A
transition to a new epoch cannot open a second writer while an old epoch remains
active."* So a second `open()` for the same card returns `writer_conflict` even at
a different epoch.

**Evidence:** `libs/markoff-collabtext-vnext/src/DocumentStorageProvider.cpp:1661-1676`:
```cpp
if (mode != StorageMode::ephemeral)
    return Error(ErrorCode::unsupported_operation,
                 "ephemeral provider cannot open durable storage");
if (!validScope(scope))
    return Error(ErrorCode::invalid_identity,
                 "storage scope requires a bounded UTF-8 document key and nonzero epoch");
std::lock_guard lock(m_state->mutex);
const QString lease = documentLeaseKey(scope);
if (m_state->activeDocuments.contains(lease))
    return Error(ErrorCode::writer_conflict,
                 "ephemeral document already has an active session");
```
`documentLeaseKey` `:98-103`; `validScope` `:90-96` requires a non-empty,
UTF-8-round-tripping key of at most 256 bytes and a non-zero epoch.

**One of our two 2026-10-17 questions is answered by this code, and we are
closing it rather than re-asking.** We asked whether a durable provider may keep
many scopes open at once. **Yes.** `ReferenceProvider::open` (`:1689`) tracks
only the same per-key `activeDocuments` set, so one durable provider can hold
every open note *and* every open card, keyed by distinct `documentKey`. That
question is closed; our default remains one ephemeral provider per document,
which matches the limit exactly.

**What is still open** is the consequence for cards specifically. A card's bytes
live in the `.canvas` JSON, so a card key is
`canvas:<vault-relative .canvas path>#<node id>` — not a filesystem path. That
satisfies `validScope`, but it means the **paint** session and the **edit**
session for one card want the *same* key at the same time: paint opens a session
document (`openForRender`), and once the card owns a real document the editor will
hold it. Under the lease rule above only one of those can be open. We need to
know which you intend:

1. **One session per card, shared by paint and edit** — paint holds a non-owning
   view of the document the editor already owns. This is our preference, and it
   needs no API change, only confirmation that a paint pass may hold a
   `MarkoffDocument *` it does not own. Note the pointer `renderInto` overload is
   already `const` and takes `const MarkoffDocument *`
   (`DocumentRenderer.h:33-34`), which is the shape we need; we just want that
   confirmed for the shared case rather than inferred.
2. **Distinct keys for paint and edit** — accepting that they then hold
   independent state and a painted card can lag the edited one.

**Cost to Corbomite:** we cannot write the card-editor swap without knowing which
of these holds, because it decides whether a card has one document or two. We
have deliberately **not** guessed and **not** worked around it: our cards are
still `QTextEdit` proxies (ASK-M37), so nothing is currently broken. We would
rather fix the architecture than ship a second, divergent card document.

**Ask:** confirm (1) or (2).

**Verified:** all four quoted sites read directly at `dca64686`. Our side is in
the tree rather than inferred — `libs/core/include/corbomite/core/EditingSession.h:17-39`
states that `keyHint` is "a stable lookup string, not a document UUID and not a
vault path that anyone should persist", and specifies the card key form.

### ASK-M39 · Markoff: a mid-block `pasteMarkdownAt` splits the block and trims the payload
**Severity:** P2 — a paste into the middle of a block produces three blocks where one belongs, and leading whitespace is silently dropped

**What happens:** `pasteMarkdownAt` delegates to
`pasteMarkdownReplacingRange(target, byteOffset, target, byteOffset, …)` — an
empty replacement range, i.e. an insertion. Two consequences, both traced from
the control flow:

1. **The payload is trimmed.** `cleaned = cleaned.trimmed()` at `:3906`, before
   parsing. Pasting `" more"` at a caret yields `"more"` — the leading space that
   made it a continuation is gone.
2. **The head is severed and the tail lands on the wrong block.** With no
   selection, `reuseTarget` is false for any mid-block offset (`:3971-3972`
   requires `byteOffset == 0`), so `:4127-4131` truncates the target at the
   caret and `:4152-4176` inserts the parsed payload as **new** blocks after it.
   The suffix is then re-appended to the **last pasted block**
   (`:4197-4203`), not to the head. So `"Hello"` plus `"X\n\nY"` at byte 3
   becomes `"Hel"` / `"X"` / `"Ylo"` — three blocks — where a flat splice
   (prefix + payload + suffix, then parse) would give `"HelX\n\nYlo"`.

**Evidence:** `MarkoffDocumentVNext.cpp:3883-3888` (`pasteMarkdownAt` →
`pasteMarkdownReplacingRange` with an empty range); `cleaned.trimmed()` `:3906`;
`reuseTarget` `:3971-3972`; the head cut `:4127-4131`; new-block insertion
`:4152-4176`; tail re-append to `lastBlock` `:4197-4203`.

**Cost to Corbomite:** template insertion is affected — a multi-block template
pasted mid-block loses its leading indentation and is split around the caret —
and any future card editor hits the same path, which is why it also gates
ASK-M37's swap. We currently emulate the flat splice by replacing the whole
block range with `prefix + payload + suffix` and re-parsing, and by using
`applyBlockEdit` when the payload has no blank line. That emulation is a
workaround we would rather delete.

**Ask:** a flat splice primitive — take `prefix + payload + suffix`, parse once,
commit as one action — or, if the current behaviour is deliberate, say so and we
will keep emulating. Either answer is useful; silence means we keep the
workaround forever.

**Verified:** all sites read directly at the pin, and the three-block result is
traced from that code rather than from the older document's prose. Our own
emulation lives in `MainWindow::insertTemplate`, so neither side of this ask is
inferred from the other.

## Priority 3 — build and integration hygiene

### ASK-M36 · markoff-parser: `tests/` is added with no gate, so a host cannot switch the leaf's tests off
**Severity:** P3 — build and integration hygiene; no runtime effect, but a consumer cannot reach a clean release configure.

**What happens:** `libs/markoff-parser/CMakeLists.txt:95` calls `add_subdirectory(tests)` unconditionally. The other leaves expose a per-leaf gate so a host can turn their tests off (`MARKOFF_CORE_BUILD_TESTS`, `MARKOFF_SOURCE_BUILD_TESTS`, `MARKOFF_STYLED_BUILD_TESTS`, `MARKOFF_CANVAS_BUILD_TESTS`); markoff-parser has none, so any consumer vendoring the family gets 13 test targets and 13 ctest entries it cannot disable. `markoff-canvas` has the gate but defaults it **ON** when undefined, so a host has to know to set it explicitly to OFF. (Corbomite does now; that is how this was found.)

**Evidence:** `libs/markoff-parser/CMakeLists.txt:95` — `add_subdirectory(tests)`, with no enclosing `if()`.
`libs/markoff-canvas/CMakeLists.txt:90-93`:
```cmake
if(NOT DEFINED MARKOFF_CANVAS_BUILD_TESTS)
    set(MARKOFF_CANVAS_BUILD_TESTS ON)
endif()
if(MARKOFF_CANVAS_BUILD_TESTS)
    add_subdirectory(tests)
```

**Cost to Corbomite:** measured 2026-10-18. A fresh `cmake --preset release` configure of Corbomite reports **15** ctest entries, of which **13** are this leaf's (`tst_parser_list_items`, `tst_parser_inline_span_bake`, `tst_markoff_parser_document`, `tst_markoff_parser_document_queries`, `tst_markoff_parser_document_top_level_blocks`, `tst_markoff_parser_frontmatter`, `tst_markoff_parser_linktext`, `tst_markoff_parser_splitter`, `tst_markoff_parser_splitter_table_passthrough`, `tst_markoff_parser_table`, `tst_link_target_decomposition`, `tst_inline_spans_link_target`, `tst_embed_image_node`); the other two are ECM's `appstreamtest`. Corbomite's own test gate is OFF in that preset, so the check that is supposed to answer "does a release configure build any host tests?" is diluted by a family member that cannot be switched off at all.

**Ask:** wrap `add_subdirectory(tests)` in `MARKOFF_PARSER_BUILD_TESTS`, defaulting OFF — or, better and as Corbomite's own TODO already asks, make `MARKOFF_BUILD_TESTS` the single canonical gate that propagates to every leaf. Also default `MARKOFF_CANVAS_BUILD_TESTS` to OFF when undefined rather than ON, so a host that says `-DMARKOFF_BUILD_TESTS=OFF` gets no leaf tests without having to enumerate them.

**Verified:** `ctest -N` counts before and after Corbomite added the missing `MARKOFF_CANVAS_BUILD_TESTS` line to its FORCE-OFF block — release configure 17 → 15, dev configure 280 → 278, with the two canvas targets gone from both. The 13 parser entries were unchanged by that edit, which is what isolates them to this leaf.

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

### ASK-M33 · markoff-core: no local frontmatter edit that participates in D2 undo
**Severity:** P2 — a user-initiated "Add file property" action silently discards the note's undo history
**What happens:** In Corbomite, `MarkdownView::insertFrontmatterProperty()` is the "Add file property" command: it performs a *user-initiated* edit — inserting a blank row into the frontmatter block — via `doc->setMarkdown(...)` → `markoff()->resetContent(bytes, Markoff::Origin::TestFixture)`, and because `TestFixture` is a stack-clearing origin, adding a property row silently discards the note's entire undo history with no way to Ctrl+Z back out. `Origin::UserRevertToSaved` looks like the right origin and its `Origin.h` comment even says *"pushes one mega edit so Ctrl+Z reverses it"*, but it does not work on the D2 path: the mega-edit lands on the **legacy** CRDT buffer, while `wipeD2State()` + `buildD2FromBytes(newContent)` run outside any undo transaction. Since the D2 reshape, D2 is the only stack the editing leaves record into. The structural blocker is that **frontmatter is not in the D2 block space**: it lives in a separate `FrontmatterMap`, serialised by `serializeFrontmatter()` as `"---\n" + raw + "\n---\n\n"` before the block loop, while `applyFlatEdit`'s coordinate space is "no-separator concatenation of `blockText(id)` for each block in `iterateBlocks()`" — so there is no offset at which `applyFlatEdit` can reach frontmatter at all.
**Evidence:** `resetContent()`:
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
The only public frontmatter methods today are `frontmatterValue()` (read) and `applyRemoteFrontmatterMapOp()` (collaboration replay, not a local edit). `setFrontmatterMap` is still absent at the current pin `dca64686`.
**Cost to Corbomite:** A user action is silently non-undoable, and the workaround we tried (`Origin::UserRevertToSaved`) was a placebo whose regression test caught it and was reverted.
**Ask:** a **local** frontmatter edit that participates in D2 undo, e.g.
```cpp
/// Replace the frontmatter map and record it in the D2 UndoLog so a single
/// undoD2() reverts it. Mirrors the existing
/// MarkoffDocument::toggleListItemChecked(BlockAnchor) precedent, which is how
/// the identically-shaped checkbox-click gap was handled.
bool setFrontmatterMap(const FrontmatterMap &fm);
```
If a general `applyMapOp` covering frontmatter + linkRef + footnoteDef is more natural, that is fine too. **Acceptance:** set a frontmatter key, assert one `undoD2()` restores the previous frontmatter byte-for-byte.
**Verified:** Reproduced in Corbomite: the `Origin::UserRevertToSaved` change was written, its regression test caught that `undoD2()` does not restore the previous content, and it was reverted; the D2 path and `FrontmatterMap` serialization were read directly at the quoted sites (2026-10-01 handoff §1).

### ASK-M34 · markoff-core: `Origin.h` documents undo semantics the D2 path does not implement
**Severity:** P2 — a stale doc comment leads consumers to ship a broken undo path
**What happens:** `Origin.h:14` documents `UserRevertToSaved` as *"pushes one mega edit so Ctrl+Z reverses it"*. That is true only of the **legacy** undo stack. Against the D2 undo log — the one the editor leaves actually use — it is inert. Related, from the same investigation: `Origin::ExternalReloadClean` and `ExternalReloadResolved` are documented *"stack cleared"* and `Vault.cpp:273` relies on that, but they too only clear the legacy stack — `wipeD2State()` explicitly preserves `d->undoLog` (`MarkoffDocument.cpp:2178-2179`). That does not cause data loss: `buildD2FromBytes()` re-parses into fresh `BlockId`s and resets `d2EditSequence()`, so the preserved entries are stale and `undoD2()` is a silent no-op. It is a low-severity UX wart (Ctrl+Z does nothing after an external change) and another place where documented intent and implementation have diverged.
**Evidence:** `Origin.h:14`; `MarkoffDocument.cpp:2178-2179`; `Vault.cpp:273`. The comment is unchanged at the current pin `dca64686`.
**Cost to Corbomite:** A downstream consumer can read that comment, do the obvious thing, and ship a broken undo path; we did exactly that.
**Ask:** correct the `UserRevertToSaved` comment to say it is legacy-stack only, or make the implementation match it; look at the `ExternalReloadClean`/`ExternalReloadResolved` "stack cleared" claim alongside it.
**Verified:** Read directly at the quoted lines; the `ExternalReload*` claim was checked and pinned in a test rather than reported as a bug (2026-10-01 handoff §2 and §5).

