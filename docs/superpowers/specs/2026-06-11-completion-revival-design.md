# Completion revival — design

**Date:** 2026-06-11 · **Status:** Approved (user, 2026-06-11) · **Track:** road-to-dogfood Phase 2, lead item ("completion revival")
**Plan:** [`../plans/2026-06-11-completion-revival.md`](../plans/2026-06-11-completion-revival.md)

---

## 1. Goal and scope

Revive in-editor autocompletion, dead since the foundation port. Full
Obsidian-parity wikilink + tag completion:

| Trigger | Completes | Source |
|---|---|---|
| `[[` | note names | `Vault::getMarkdownFiles()` basenames |
| `[[` | aliases (frontmatter `aliases`/`alias`) | `MetadataCache::getFileCache(path)->frontmatter` |
| `[[note#` | headings of the resolved target | `getFileCache(target)->headings` |
| `[[note#^` | **existing** block ids of the target | `getFileCache(target)->blocks` |
| `#` | tags | `SQLiteIndex::allTags()` |

**All editable leaves.** Completion is wired once, leaf-agnostically, through
the `Markoff::MarkdownView` base. It activates in Live and Source; the Styled
leaf gets it for free if it is ever hosted editable (as Reading it is
read-only, `hasEditing() == false`, and completion never triggers — no
special-casing).

**Explicitly out of scope (user-approved deferrals):**
- `^block-id` **creation**: Obsidian writes a new `^id` into the *target* file
  when you pick a block that lacks one. That is a write-to-another-file flow
  with its own integrity concerns. This cut completes existing ids only.
  Follow-up punch-list item.
- Link-format conventions from `app.json` (`useMarkdownLinks`,
  `newLinkFormat`) — Phase 3 owns those; insertion uses the basename /
  relative-path rule in §8.
- Completion inside code blocks is not suppressed in this cut (gate on
  `EditorContext.blockKind` is a noted follow-up).
- QML-native popup, auto-pairing `]]` on `[[`.

**Phasing** (one spec, one plan, three checkpoints):
- **A1** — upstream `caretRect()` contract + re-pin; suggester interface v2;
  coordinate bridge; `CompletionController`; `[[` note names + `#` tags
  end-to-end.
- **A2** — aliases + heading completion.
- **A3** — existing-`^block` completion.

## 2. Current state (verified 2026-06-11)

**Survived the port intact (reuse as-is):**
- `EditorSuggestManager` (`libs/core/.../EditorSuggestManager.h:20-49`) —
  insertion-order, first-non-null-wins dispatch. Behavior pinned by
  `tests/core/tst_editorsuggest.cpp` (6 slots).
- `WikiLinkSuggest` / `TagSuggest` (`src/editor/`) — trigger detection +
  candidates work; only the item shape and new sub-target modes change.
- `CompletionPopup` (`src/editor/CompletionPopup.h`) — focus-safe child-widget
  popup (NOT `Qt::Popup`), external keyboard driving
  (`selectNext/selectPrevious/acceptCurrent`), internal fuzzy filter proxy
  (`setFilterText`), signals `itemSelected(text, data)` / `dismissed()`.
  Ctor contract: `CompletionPopup(QAbstractItemModel *sourceModel, QWidget
  *parent)` — parent MUST be a real widget in the editor.
- `FuzzyMatcher`, `MetadataCache` (`getFileCache` → `CachedMetadata` with
  `headings`, `blocks: QHash<QString, BlockCache>`, `frontmatter`),
  `SQLiteIndex::allTags()`, `LinkResolver`.

**Dead (the work):** `NoteEditorWidget`'s stubs
(`maybeActivateSuggester`, `positionCompletionPopup`, `absoluteCursorPos`,
`currentLineText`, `updateCompletionFilter`, `onCompletionAccepted`,
`NoteEditorWidget.cpp:436-497`) and its popup `eventFilter`
(`:387-419`). All depended on the retired `Markoff::Editor`
(`toPlainText`/`cursorLine`/viewport). These stubs are **deleted**, replaced
by `CompletionController` (§6).

**The one missing primitive:** no leaf exposes a caret rectangle. Everything
else completion needs already exists on the document/base contract:
- Insertion: `MarkoffDocument::applyBlockEdit(const BlockEdit&)`
  (`MarkoffDocument.h:174`; `BlockEdit{blockId, withinBlockByteOffset,
  removedBytes, insertedUtf8}` in `BlockEdit.h`) — public, undo-integrated
  (wraps `UndoLog::Transaction`), notifies buffer proxies synchronously,
  schedules the debounced `d2DocumentChanged`. Because all three leaves share
  one `MarkoffDocument`, a document-side edit propagates to every leaf
  through the standard reactive paths (Live: `onD2Changed` → model →
  delegates; Source/Styled: `SourceTextDocumentBinding::onD2DocumentChanged`
  incremental diff).
- Signals: `MarkdownView::cursorPositionChanged(line, col)` (all leaves,
  Phase-1 contract) + `MarkoffDocument::d2DocumentChanged()` (debounced
  once per event-loop spin; the underlying CRDT state is updated
  synchronously, only the signal is deferred).
- Cursor: `MarkdownView::cursorPosition() → CursorPos{line, column}` —
  contract-v2 §3 normative flat-visual-line space, identical across leaves
  (each block contributes `1 + count('\n' in blockText)` lines; column is
  1-based UTF-16 within the line).

## 3. Architecture

```
                       ┌──────────────────────────────┐
   MainWindow          │  NoteEditorWidget            │
   (wires vault/index/ │   owns CompletionController ─┼─── activeLeaf(): Markoff::MarkdownView*
    cache/resolver into│   forwards leaf/doc swaps    │         │ caretRect()        [NEW, §4]
    suggesters,        └──────────────┬───────────────┘         │ cursorPosition()
    onVaultOpened)                    │                         │ hasEditing()
                                      ▼                         ▼
                       ┌──────────────────────────────┐   ┌───────────────┐
                       │  CompletionController (§6)   │   │ MarkoffDocument│
                       │  refresh ⟵ d2DocumentChanged │◀──│ blockText/     │
                       │          ⟵ cursorPosChanged  │   │ applyBlockEdit │
                       │  EditorSuggestManager::      │   └───────────────┘
                       │    dispatch(col, lineText)   │
                       │  CompletionPopup lifecycle   │
                       │  scoped app-level key filter │
                       └──────────────┬───────────────┘
                                      │ LineResolve (§5): CursorPos ⟷ (block, offsets, lineText)
                                      ▼
                       WikiLinkSuggest (§8) · TagSuggest (§9)
```

Data flow per keystroke: leaf edits document → `d2DocumentChanged` (and
`cursorPositionChanged`) → controller coalesces (single-shot 0 timer) →
`refresh()`: resolve line via §5 → `dispatch(colInLine, lineText, doc)` →
no trigger ⇒ dismiss; trigger ⇒ build/update popup model + filter + position
at `caretRect().bottomLeft()`. Accept (Enter/Tab) → `applyBlockEdit` replacing
the trigger range → `setCursorPosition` after the inserted text → dismiss.

## 4. Upstream contract addition — `MarkdownView::caretRect()` (markoff-family)

```cpp
// markoff-core/include/markoff/core/MarkdownView.h
/// Caret rectangle in THIS widget's coordinate system, or an invalid
/// QRect when no caret is established (no document, no focus, cursor
/// not in a text-bearing state). Consumers anchor transient UI
/// (completion popups) at bottomLeft().
virtual QRect caretRect() const { return {}; }
```

Per-leaf implementations:
- **Source** (`Markoff::Source::Editor`): `cursorRect()` of the internal
  `QPlainTextEdit`, mapped viewport → editor widget coords. One-liner.
- **Styled** (`Markoff::Styled::Editor`): `cursorRect()` of the internal
  `QTextEdit`, same mapping. One-liner.
- **Live** (`Markoff::Live::EditorWidget`): generic, **no per-delegate QML
  changes**: `quickWidget->quickWindow()->activeFocusItem()`; if null or it
  has no `cursorRectangle` property → invalid QRect. Else read
  `cursorRectangle` (QRectF, item-local), `item->mapRectToScene()`, translate
  by the `QQuickWidget`'s position inside `EditorWidget` (scene coords ==
  QQuickWidget-local coords). This works for every text-bearing delegate
  including table cells, because the focused `TextEdit` *is* the
  active-focus item whenever a `TextCaret`/cell edit is live.

**Seam discipline (markoff INVARIANTS):** this is a read-only query — no new
cursor authority, no state store (INVARIANTS #3 satisfied trivially); cite
the developmental record §delegate-focus in the markoff-side spec stub. Per
INVARIANTS #4/#5, contract tests drive the production path (real QML scene
for Live), falsifiability-proven by breaking the mapping in a throwaway stub.

Upstream deliverables: base virtual + three overrides + contract tests
(`tst_view_contract_caret_rect` covering all three leaves: valid + non-empty
+ within widget bounds after focus/seed; invalid before attach) + a short
spec in markoff `docs/specs/2026-06-11-caret-rect-contract-design.md` citing
this document. Then re-pin Corbomite (from `af91a936`).

## 5. Coordinate bridge — `LineResolve` (Corbomite, document-side)

New pure helper, `src/editor/LineResolve.{h,cpp}`, namespace
`Corbomite::LineResolve`. No widget deps — unit-testable headless against a
real `MarkoffDocument`.

```cpp
struct ResolvedLine {
    Markoff::BlockId blockId;       // block containing the visual line
    int      blockRow;              // index in iterateBlocks() order
    int      lineStartCharInBlock;  // UTF-16 offset of line start within blockText (as QString)
    QString  lineText;              // the line, without trailing '\n'
};
/// Resolve a contract-v2 flat visual line (1-based) against the document.
/// Walk iterateBlocks(); each block spans 1 + count('\n') lines.
/// Returns nullopt when line is out of range or the document is empty/null.
std::optional<ResolvedLine> resolveLine(const Markoff::MarkoffDocument *doc, int line);

/// UTF-16 char offset within blockText(QString) → UTF-8 byte offset.
/// (text.left(charPos).toUtf8().size(); clamps charPos to text length.)
uint32_t byteOffsetForChar(const QString &blockText, int charPos);
```

The controller composes these:
- `lineText` + `colInLine = cursorPosition().column - 1` feed `dispatch()`.
- Insertion: char range `[lineStartCharInBlock + info.start,
  lineStartCharInBlock + replaceEnd)` → byte range via `byteOffsetForChar` →
  `BlockEdit`.
- Post-insert caret: `setCursorPosition({line, info.start +
  insertText.length() + 1})` (deterministic across leaves; do not rely on
  any leaf's own post-edit cursor behavior).

O(blocks) per refresh; fine at vault-note scale. If profiling ever says
otherwise, a line→block index cache keyed on `d2EditSequence()` is the
designated optimization point — not built now.

## 6. `CompletionController` (new, `src/editor/CompletionController.{h,cpp}`)

Replaces the dead `NoteEditorWidget` stubs (which are deleted). One QObject
owned by `NoteEditorWidget`; holds the popup, the trigger session, and the
key filter. Public surface:

```cpp
class CompletionController : public QObject {
    Q_OBJECT
public:
    explicit CompletionController(EditorSuggestManager *manager, QObject *parent);
    void setLeaf(Markoff::MarkdownView *leaf);     // on construction + every mode switch; dismisses
    void setNoteDocument(NoteDocument *doc);        // on every document swap; dismisses
    bool isActive() const;                          // popup visible
public Q_SLOTS:
    void dismiss();
private:
    void scheduleRefresh();                         // 0-ms single-shot coalescer
    void refresh();                                 // the whole reactive brain
    void accept(const QString &display, const QString &insertText);
};
```

**Reactive model (replaces the old trigger-pos statefulness):** `refresh()`
recomputes everything from the current snapshot every time —
`cursorPosition()` + `resolveLine()` + `manager->dispatch(colInLine,
lineText, m_doc)`. No match ⇒ dismiss. Match ⇒ popup. This single path
handles trigger start, per-keystroke filter updates, cursor-moved-out
dismissal, and backspace-past-trigger dismissal with no extra state machine.
Wired to BOTH `MarkoffDocument::d2DocumentChanged` (typing) and the leaf's
`cursorPositionChanged` (pure caret moves), coalesced through one 0-ms
single-shot so a keystroke (which fires both) refreshes once.

**Gates, checked at the top of `refresh()`** — any failure ⇒ `dismiss()`:
`m_leaf && m_doc && m_leaf->hasEditing()` (kills Reading mode and detached
states); `caretRect().isValid()` (can't anchor ⇒ don't show; `qCDebug`, never
a misplaced popup).

**Popup lifecycle:** popup created lazily per trigger session, parented to
the active leaf widget, `deleteLater` on dismissal (matches the existing
`dismissCompletion` pattern). It is constructed over a controller-owned
`QStandardItemModel` (roles: `DisplayRole` = display, `UserRole` =
insertText, `UserRole+1` = detail) which is **repopulated (clear + refill) on
every refresh** from `getSuggestions(ctx)` — candidate sets are small, and
refill-always eliminates the stale-mode bug when a session morphs, e.g.
`[[Note` → `[[Note#`;
`setFilterText(set.filter)` applies the popup's fuzzy proxy (§7 defines
`filter`). Positioned at `leaf->mapTo(popupParent,
caretRect().bottomLeft() + QPoint(0, 2))`, clamped to the leaf's rect;
flipped above the caret when there's no room below.

**Keys:** a **scoped application-level event filter installed only while the
popup is visible** (installed on show, removed on dismiss). Intercepts
Up/Down (navigate), Enter/Return/Tab (accept), Escape (dismiss); everything
else passes through to the editor. Rationale: in the Live leaf, key events
land on the QML `TextEdit` inside the `QQuickWidget` — a widget-level
`eventFilter` on `EditorWidget` never sees them, which is why the old
`NoteEditorWidget::eventFilter` approach cannot survive. Qt runs
later-installed application filters first, so this wins over
`ScopeManager`'s dispatcher exactly while a popup is up — document this
ordering dependency in a comment at the install site. The old
`eventFilter` completion branch in `NoteEditorWidget` is deleted (the
`FocusOut` dismiss moves into the controller via the same filter watching
`m_leaf`).

**Accept path:** `itemSelected(text, data)` → `accept()`:
re-resolve the line (the snapshot may be one coalesce stale), re-dispatch to
get a fresh `TriggerInfo`; if no trigger survives, abort silently. Else build
`BlockEdit` per §5 with removal range `[start, replaceEnd)`, call
`m_doc->markoff()->applyBlockEdit(edit)`, then `setCursorPosition` after the
inserted text, then dismiss. Undo arrives free via `applyBlockEdit`'s
transaction (one Ctrl+Z removes the completion).

**Lifetime safety (the 2026-06-10 UAF lesson applies):** the controller
holds `NoteDocument*`/`MarkoffDocument*` raw; it must connect to the
document's `destroyed()` and to `NoteEditorWidget`'s swap notifications and
null + dismiss. Never touch `m_doc` inside `accept()` without the null gate.

## 7. Suggester interface v2 (`libs/core/.../EditorSuggest.h`)

Breaking change, pre-1.0, plugin-exposed via `EditorSuggestRegistrar`
("ui.editor" permission) — sweep confirms no in-tree plugin implements
`EditorSuggest` (example plugins don't; re-verify in the plan). Manager
dispatch semantics are UNCHANGED (the 6 `tst_editorsuggest` slots keep
passing modulo the item-shape rename).

```cpp
struct EditorSuggestTriggerInfo {
    int start = -1;        // UTF-16 char offset WITHIN lineText (line-relative)
    int end = -1;          // cursor position within lineText
    int replaceEnd = -1;   // optional: end of replacement range (≥ end);
                           // -1 ⇒ same as end. Lets wiki-link consume a
                           // pre-existing "]]" after the cursor.
    QString query;         // lineText.mid(start, end - start)
};

struct EditorSuggestItem {
    QString display;       // shown in the popup
    QString insertText;    // literal replacement for [start, replaceEnd)
    QString detail;        // optional dim right-hand context (path, target note)
};

struct EditorSuggestionSet {
    QList<EditorSuggestItem> items;  // candidate UNIVERSE for the current mode
    QString filter;                  // what the popup's fuzzy proxy filters by
};

// onTrigger(cursorPos, lineText, file) — unchanged signature/semantics.
virtual EditorSuggestionSet getSuggestions(const EditorSuggestTriggerInfo &ctx) = 0;
// selectSuggestion() is RETIRED — insertText is fully resolved per item.
```

Why `filter` lives in the set: in `[[Note#se`, the candidate universe is
*headings of Note* and the filter is `se` — the popup must not fuzzy-match
`Note#se` against heading names. The suggester owns that split; the popup
stays dumb. Suggesters return the full universe; ranking/filtering is the
popup proxy's job (`FuzzyMatcher` inside `CompletionFilterProxy`) — no
double-filtering.

Also in this change: fix the `TriggerInfo` header comment (it claims
"absolute character offsets in the document"; every implementation and
`tst_editorsuggest::testTriggerInfoCarriesQuery` treat them as
line-relative — line-relative is now normative), and clamp `cursorPos` to
`lineText.length()` at the top of `EditorSuggestManager::dispatch` (punch-list
P3, `tests/core/tst_editorsuggest.cpp:117` context).

## 8. `WikiLinkSuggest` extensions

`onTrigger`: unchanged backward scan for `[[` (bail on `]]` already closed
before cursor or line start). NEW: if the two chars at `lineText.mid(end)`
are `"]]"`, set `replaceEnd = end + 2` (consume them; prevents `]]]]`).

`getSuggestions(ctx)` dispatches on the query's shape:

| Query shape | Mode | Universe | filter | insertText |
|---|---|---|---|---|
| `q` (no `#`) | note names + aliases | every md basename; plus, per note with frontmatter `aliases`/`alias` (string or array — accept both, Obsidian does), one item per alias with `detail` = target note | `q` | `name]]` — or `relative/path]]` (sans `.md`) when the basename is ambiguous across the vault (resolved via `LinkResolver`); alias items insert `target\|alias]]` |
| `T#q` | headings | `getFileCache(resolve(T))->headings` texts | `q` | `T#Heading]]` |
| `T#^q` | blocks | `getFileCache(resolve(T))->blocks` keys (existing ids only — §1) | `q` | `T#^id]]` |

`resolve(T)`: `LinkResolver` lookup of `T` against the vault (same semantics
as link-click navigation); unresolved target ⇒ empty universe (popup shows
nothing ⇒ controller keeps it visible-but-empty? No — `refresh()` dismisses
when `visibleRowCount() == 0`).

New dependencies, mirroring the existing `setVault` pattern:
`setMetadataCache(MetadataCache*)`, `setLinkResolver(LinkResolver*)` — wired
in `MainWindow::onVaultOpened` next to the existing `m_wikiSuggest->setVault`
(`MainWindow.cpp:2233`), nulled in `onVaultClosed`. Null cache/resolver ⇒
modes degrade gracefully (names-only).

Aliases ship in **A2**; the table's row 1 is names-only in A1.

## 9. `TagSuggest`

Mechanically updated to the v2 shape: universe = `index->allTags()` (leading
`#` stripped), `filter = ctx.query`, `insertText = tag` (the typed `#` stays).
Nested tags (`a/b`) work as plain strings. No new dependencies.

## 10. Wiring

- `NoteEditorWidget`: constructs the controller (manager comes from the
  existing `setEditorSuggestManager`, which currently stores and does
  nothing); calls `controller->setLeaf(activeLeaf())` in the constructor and
  in `setViewMode` step 4 (after attach); `controller->setNoteDocument(doc)`
  in `setNoteDocument`. Deletes: the five stub methods, the
  completion branch of `eventFilter`, fields `m_completionPopup` /
  `m_activeSuggester` / `m_completionTriggerPos`, and `onTextChanged`'s
  completion remnant.
- `MainWindow::onVaultOpened/onVaultClosed`: add the two new
  `WikiLinkSuggest` setters beside the existing vault/index wiring. No other
  MainWindow changes (leaf-agnosticism grep gate unaffected).

## 11. Error handling and edge cases

- **Invalid `caretRect()`** ⇒ no popup (gate, §6). Never a guessed position.
- **Document destroyed / swapped / vault closed mid-popup** ⇒ dismiss via
  `destroyed()` + swap hooks (§6); suggesters' null-dep gates (§8).
- **Mode switch mid-popup** ⇒ `setLeaf` dismisses before re-anchoring.
- **Accept after stale snapshot** ⇒ re-dispatch in `accept()`; silent abort
  when the trigger no longer holds (§6).
- **Cursor past line end** (rapid-edit race) ⇒ `dispatch()` clamp (§7).
- **Empty universe / no fuzzy matches** ⇒ dismiss when
  `visibleRowCount() == 0` after filter.
- **Read-only / Reading** ⇒ `hasEditing()` gate.
- **Multi-line blocks** (load-time `\n` in a block) ⇒ `resolveLine` is
  defined over visual lines exactly like contract-v2 `cursorPosition`, so
  offsets stay consistent.
- **UTF-16 vs UTF-8**: all suggester/popup math is UTF-16 (`QString`);
  conversion to bytes happens exactly once, in `byteOffsetForChar` at the
  `BlockEdit` boundary (§5).

## 12. Testing

Markoff (upstream, INVARIANTS #4/#5 — falsifiable, production callsites):
- `tst_view_contract_caret_rect` — per leaf: invalid before attach; valid,
  non-empty, within bounds after attach+focus (Live drives the real QML
  scene like `tst_view_contract_live_attach_window`); tracks the caret down
  a multi-block document.

Corbomite:
- `tst_line_resolve` — resolveLine over single/multi-`\n` blocks, first/last
  lines, out-of-range, empty doc; byteOffsetForChar with multibyte (é, 日,
  emoji surrogate pairs).
- `tst_editorsuggest` — existing 6 slots updated to v2 shapes (dispatch
  semantics unchanged); + clamp slot (un-clamped cursorPos no longer asserts).
- `tst_wikilink_suggest` — trigger detection (start/mid-line, `]]`-closed
  bail, replaceEnd consume); names mode; A2: alias items (string + array
  frontmatter), heading mode incl. unresolved target; A3: block mode.
  Fake vault dir + real `MetadataCache` where practical.
- `tst_tag_suggest` — v2 shape, `#` trigger after whitespace/line-start only.
- `tst_completion_controller` — the core suite, headless offscreen, real
  `MarkoffDocument` + a minimal `MarkdownView` test leaf (overrides
  `caretRect` to a fixed rect, `hasEditing` true): type-`[[`-show,
  filter-narrowing, cursor-move-out dismiss, backspace-past-trigger dismiss,
  accept-replaces-range-and-moves-caret (assert document bytes AND
  `cursorPosition()`), accept-is-one-undo-step, read-only never triggers,
  doc-destroyed-while-open dismisses without crash (UAF regression posture),
  invalid-caretRect suppresses.
- `tst_note_editor_widget_completion` — integration: real `NoteEditorWidget`,
  Live leaf, document-driven edits + cursor placement; popup visibility
  through real `caretRect`. (Keyboard-level QML typing stays out — covered
  upstream by the harness; here we drive the document like
  `tst_note_editor_widget_ephemeral` does.)

Falsifiability: per markoff discipline, prove the controller suite bites by
temporarily breaking trigger detection (must fail) before trusting green.
Suites: markoff 269/272 baseline (3 known-red, queue #10) and Corbomite
260/260 must hold; every phase ends green + committed + pushed.

## 13. Phase exit criteria

- **A1:** typing `[[` in Live or Source pops fuzzy note-name completion at
  the caret; Enter inserts `name]]` and the caret lands after it; one undo
  removes it; `#` does the same for tags; Reading mode never triggers;
  Corbomite suite green incl. all new A1 tests; markoff re-pinned with
  caret-rect contract tests green; PARITY-MATRIX completion rows updated.
- **A2:** aliases appear (with target-note detail) and insert
  `target|alias]]`; `[[note#` lists headings of the resolved target.
- **A3:** `[[note#^` lists existing block ids; punch-list follow-up filed
  for `^id` creation; decisions-archive closeout; PROJECT-STATE updated.

## 14. Follow-ups filed at closeout (not in this arc)

`^id` creation on block pick · code-block trigger suppression via
`EditorContext` · `app.json` link-format conventions (Phase 3) · popup
`detail` column rendering polish if A2's plain rendering reads poorly ·
line→block index cache if profiling demands it.
