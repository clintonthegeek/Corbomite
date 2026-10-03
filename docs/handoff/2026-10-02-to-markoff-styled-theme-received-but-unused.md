# Handoff → markoff-styled devs: the leaf receives a `Markoff::Theme` and does not use it

**From:** Corbomite (downstream consumer of `markoff-styled`, `markoff-source`, `markoff-live`)
**Date:** 2026-10-02
**Corbomite HEAD:** `57e79fad`
**Markoff pin:** `5d8850369c91175fe51e3738602d5893a7db21b3`
**Severity (our view):** **P1** for §1 — it is the largest user-visible gap we
have against Obsidian parity, and reading mode currently has no dark theme at
all. §2–§6 are P1/P2. Nothing here blocks your roadmap.

This is a short document, and one item dominates it. We have sent the markoff-core
and markoff-parser findings separately.

---

## 1. (P1) `FormatPass` receives a theme and reads none of it — every colour is a hardcoded literal

`src/FormatPass.h:21`
```cpp
const Markoff::Theme *theme = nullptr;
```

`src/FormatPass.cpp:425`
```cpp
const qreal fontScale = opts.fontScale;   // the ONLY field read
```

`opts.theme` is never touched anywhere in the file. Every colour is a literal, and
the comments name the `Theme` slot the code *should* be using:

```cpp
bf.setBackground(QColor(245, 245, 245));   // :113  Theme::CodeBlockBackground
cf.setForeground(QColor(100, 100, 100));   // :133  Theme::Quote.
fmt.setBackground(QColor(255, 240, 130)); // :191  Theme::Highlight
fmt.setForeground(QColor(70, 130, 180));  // :194  Theme::Tag
... ? QColor(120, 80, 200) : QColor(40, 100, 200));  // :197,:200,:211-212  WikiLink : Link
```

We read those comments as an intent that was documented and then not implemented,
which makes triaging this harder than it needs to be — a maintainer looking at
`:113` reasonably concludes the theme is handled somewhere upstream.

### Two independent breaks, either of which alone is sufficient

1. **`computeBlockHash` takes no theme** (`FormatPass.cpp:217-221`) — its inputs
   are `(kind, text, spans, attrs, fontScale)`. So even a theme-aware
   `FormatPass` would skip every block on a theme change via the per-block hash
   gate at `:464-467`.
2. **`StyleApplier::setTheme` early-returns on *pointer* equality**
   (`StyleApplier.cpp:39-43`), while `Styled::Editor::setTheme` always passes the
   same address (`&m_theme`, `Editor.cpp:49`). So it never triggers a restyle
   anyway.

### Why we are confident this is a bug and not a choice

**`StyledFindAdapter` *does* use the theme** — for its own highlights
(`Detail/StyledFindAdapter.cpp:150-154`). So in the same leaf, find highlights
re-theme and body text does not. That inconsistency is what convinced us.

### What it costs us

Corbomite has a `ThemeService` and propagates a full `Markoff::Theme` through the
contract-v2 `MarkdownView::setTheme` seam to all three leaves. In **Reading
mode** — which is a `Markoff::Styled::Editor` — dark mode and user themes have
**no effect on content**. Our canvas dark-mode work (shipped 2026-10-02) has to
build its own `CanvasPalette` because canvas items are ours; reading-mode content
is yours.

### What we would like

1. Read `opts.theme` at every colour site, with
   `opts.theme ? opts.theme->color(Slot::X) : Theme::defaultLight().color(Slot::X)`
   as the fallback so the renderer still works headless (we do render card
   previews offscreen).
2. Add `theme` to `computeBlockHash`'s inputs.
3. `StyleApplier::setTheme` should store the `Theme` **by value** and compare with
   a new `operator==`.
4. That operator is wanted anyway: `Markoff::Theme` has none, so
   `MarkdownView::setTheme` (`MarkdownView.cpp:48-53`) is unconditional and
   Corbomite — which loops all three leaves on every theme propagation — forces a
   full `StyleApplier::rerender()` (clearing `m_blockHashes` and re-running
   `FormatPass::apply` over the whole document) each time. Your own
   `markoff-styled/src/Editor.cpp:423-426` names the cause: *"No same-value
   guard: Markoff::Theme has no operator==."*

**Test:** set a theme with a distinct `CodeBlockBackground`, render a document
containing a code block, assert the block's background is that colour.

---

## 2. (P1) `recomputeContext()` has no table-frame guard — wrong `blockKind` and `headingLevel` past the first table

`src/Editor.cpp:489-527`

The format verbs correctly refuse to run past the first table frame
(`:265-292`, `selectionInTrustworthyRegion`), but `recomputeContext` has **no
such guard**. It maps `cursor.position()` — QTextDocument coordinates, which
include frame structure characters — through `doc->widgetFlatView()`, which is
raw markdown source bytes (`:500`).

Once a table frame exists, every caret position at or after it resolves to the
wrong block, so `contextChanged` fires with a wrong `blockKind`/`headingLevel`.
It also never populates `tableRow`/`tableCol`, which `EditorContext.h:21-22`
declares — both `recomputeContext` implementations leave them at `-1`.

**Why it matters to us:** `contextChanged` is what drives our whole
context-sensitive action layer. A wrong `blockKind` means Format menu entries
enable and disable against the wrong block.

**What we would like:** reuse `selectionInTrustworthyRegion`, or resolve via
`walkBlocks()`; and populate `tableRow`/`tableCol` from the frame's cell when
`inTable`.

---

## 3. (P1) `Source::Editor` lacks the list/quote marker restore that styled has

- **styled has it:** `src/StructuralTextEdit.cpp:44-81` prepends
  `listItemDisplayMarker` / `blockQuoteDisplayMarker`
- **source does not:** `markoff-source/src/InnerEditor.cpp:33-38` returns the raw
  selection

So **Copy-as-Markdown/HTML from Source mode** of a loaded list or blockquote emits
marker-less content. Your own comment in the styled file states the cost:
*"without this, Copy as HTML/RTF of a loaded quote/list re-parses bare content as
a plain paragraph and pastes into e.g. LibreOffice as Body Text, not a Block
Quote/List."*

Corbomite ships all four clipboard verbs on both leaves (Cluster N), so this
reaches users.

**What we would like:** lift `StructuralTextEdit.cpp:44-81` into a shared helper
in `markoff-core` — e.g.
`ClipboardCodec::selectedMarkdownWithMarkers(binding, cursor)` — and call it
from both leaves.

---

## 4. (P2) `Session` is not on the polymorphic base contract

`MarkdownView.h:14-109` has **no** session accessor at all.

- `Styled::Editor` has `Q_PROPERTY(Markoff::Session *session …)` plus
  `session()`/`setSession()` (`Editor.h:34-35, 86-87`)
- `Source::Editor` has **neither** — it manages a private
  `QPointer<Markoff::Session> m_session` (`Editor.h:137`) with no accessor

Corbomite holds leaves as `Markoff::MarkdownView *`
(`src/editor/NoteEditorWidget.h:105`), so we must know the concrete type to touch
sessions, and **cannot get or set one on a Source leaf at all**.

**What we would like:** `virtual Session *session() const` and
`virtual void setSession(Session *)` on `MarkdownView`.

---

## 5. (P2) Session lifetime leaks

**`Source::Editor` leaks a Session into the document.** It creates one in
`setDocument` (`Editor.cpp:150-153`) and destroys it only on a **document swap**
(`:116-126`); `~Editor()` is `= default` (`:113`). One leaked session per leaf
teardown.

The retired live leaf fixed exactly this and says why
(`markoff-live/src/EditorWidget.cpp:210-216`):
```cpp
// Session owned by document — … explicitly destroy the session we created so
// the document doesn't accumulate ghost sessions across leaf swaps.
if (d->session && document()) document()->destroySession(d->session);
```

Separately, **`Styled::Editor::setSession` never emits `sessionChanged` when the
session dies** (`Editor.cpp:412-417`): `m_session` is a `QPointer`
(`Editor.h:125`), so when the `MarkoffDocument` is destroyed the `Session` (its
QObject child) dies first and nulls the pointer; the later `if (m_session ==
s) return;` then early-returns and no signal is emitted. A consumer bound to
`sessionChanged` never learns the session is gone.

**What we would like:** destroy the owned session in `~Editor()`; connect to
`QObject::destroyed` on the session and null + emit. A
`virtual Session *createOwnedSession()` hook on `MarkdownView` would mean no leaf
has to remember — which is the pattern that has repeatedly bitten here.

---

## 6. (P2) `Styled::Editor::setDocument` does not re-wire the find adapter

`Editor.cpp:82-175` re-wires the binding, the style applier, `LinkInteraction`
and both context connections — but **never touches `m_findAdapter`**.
`StyledFindAdapter::attach` holds `m_docChangedCon` bound to one
`MarkoffDocument` (`Detail/StyledFindAdapter.cpp:78-81`), so after a document swap
find highlights stop refreshing until the consumer performs the "Detach → swap →
re-attach" dance documented in `markoff-styled/CLAUDE.md`.

**What we would like:** `Editor::setDocument` should disconnect and reconnect the
adapter's document connection itself.

Separately, `renderHighlights` (`StyledFindAdapter.cpp:136-180`) is wired to
`matchesChanged`, `currentMatchChanged`, `themeChanged` **and** `d2DocumentChanged`
(`:68-85`), and each fire walks every block into a `QHash<BlockId, WalkEntry>`
(`:159-162`) where each entry holds the block's full text, plus a
`QString::fromUtf8(buf.left(off))` allocation per match (`:33-37`). So every
`findNext()` and every `d2DocumentChanged` is O(document + matches). Caching
`WalkEntry`s keyed on `blockEditSequence` — invalidated on `d2DocumentChanged`
only, not on `currentMatchChanged` — would fix it.

---

## 7. Smaller items

| Item | Location | Note |
|---|---|---|
| `Styled::StructuralTextEdit::selectedMarkdown` indexes `iterateBlocks()` by `QTextBlock::blockNumber()` | `StructuralTextEdit.cpp:56-81` | A `QTextTable` frame adds blocks, so with a table present the wrong ListItem/BlockQuote markers get prepended. The comment at `:52-55` assumes 1:1 lockstep, which frames break. |
| Both leaves keep a **second** `Theme` member beside the base's | `markoff-styled/Editor.{h:134, cpp:421-430}`, `markoff-source/Editor.{h:142, cpp:282-302}` | Two authorities that stay in sync **only** because `Editor::setTheme` writes both. One leaf forgetting the second write silently desyncs `theme()` from `MarkdownView::theme()`. Dropping the leaf copies and passing `&this->theme()` downstream would remove the class. |
| The structured-paste path is dead in production | `InnerEditor.cpp:46,102`, `StructuralTextEdit.cpp:93,117`, `MarkoffDocument.cpp:1302` | All three leaves call `mimeFromMarkdown(md, /*blocks*/{}, flavor)`, so the `application/x-markoff-blocks` flavour is never emitted outside the retired live leaf. `applyStructuredPaste` has zero non-test callers. Probably fine as a reserved API — flagging so it is a decision rather than an oversight. |
| `LinkInteraction::resolveLinkAt` mixes two coordinate spaces, and does O(document) work at mouse-event rate | `LinkInteraction.cpp:53-56, 66-67`, `MarkoffDocument.cpp:767-790` | The byte offset is computed from `toPlainText()` — the **separator** view — then passed to `textAnchorAt()`, which accumulates **no-separator** lengths (`:773-789`). The offset is one byte too large for every block boundary crossed, so hovering/clicking a link in block *k* resolves *k* bytes past the target. Separately, `handleMove` (`:108-125`) runs on **every** `MouseMove`: a full document copy, a prefix copy, an `iterateBlocks()` walk, `blockText()` and `inlineSpansFor()` — O(document) at 60–120 Hz, no throttle, no early-out. Suggest mapping the `QTextBlock` number to a `BlockId` via the existing `walkBlocks()`, and throttling. |
| `MarkdownView` holds a raw `MarkoffDocument *` | `MarkdownView.h:105`, `MarkdownView.cpp:39-46` | Covered in full in the markoff-core handoff, but it bites the styled leaf via `StyleApplier.cpp:60-64` and `StyledTableRenderer.h:23`. The fix pattern already exists in your retired `markoff-live/src/EditorWidget.cpp:268-279`. |
| `Markoff::Source::FindBar` is documented but does not exist | `markoff-source/CLAUDE.md:27` | `grep -rn FindBar libs/markoff-source/` matches only that line. Probably a stale doc line. |

---

## 8. Suggested triage order

1. **§1** — biggest user-visible gap, and the fix is mostly mechanical once the
   hash and change-gate are handled.
2. **§2** — a correctness bug in a signal our entire action layer depends on.
3. **§3** — one shared helper, two call sites, clear user-facing effect.
4. **§4** + **§5** — base-contract work; §5's `Source::Editor` leak is a few lines.
5. §6, §7 as capacity allows.

We are aware §1 might already be known and deliberately deferred to a
"port the whole renderer onto Live" milestone — the file's own header and the
`CLAUDE.md` reference "E1 rewire", which suggests it is on someone's radar. If it
is a deliberate sequencing choice we would rather plan around it than push; our
ask would then be **only** the cheap half: making the `// Theme::X` comments
honest, so the next reader (us included) does not re-triage this from scratch
every few months.