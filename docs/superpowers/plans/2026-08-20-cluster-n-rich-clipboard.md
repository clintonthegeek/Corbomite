# Cluster N — Rich clipboard (copy-as + smart paste)

**Opened:** 2026-08-20. **Type:** Full plan. **Track:** strategic cluster.
**Branch:** `feature/rich-clipboard` (Corbomite worktree `.worktrees/rich-clipboard`; Markoff worktree nested at that tree's `libs/markoff-family`, based on pin `a3d8055e` — not standalone Markoff `master`).
**Status:** **CLOSED 2026-08-20 (Phases 0–5).** Live eyeball passed on the worktree binary. Markoff tip `bafa3095` (includes blockquote HTML/RTF export fix). Branch `feature/rich-clipboard` not yet merged to `master` (master mid-M4). Image paste/drop remains out of scope.
**Lettering:** post-reset N. Not legacy Cluster N (plugin-ready surfaces, closed 2026-04-17, archived).
**Punch-list item absorbed:** P5 Paste-from-HTML → Markdown (bucket ③). Image paste/drop is out of scope.
**Plan body** below is the 2026-08-20 session plan, filed unmodified.

---

# Rich clipboard (copy-as + smart paste)

**Branch:** `feature/rich-clipboard` (same name in Corbomite and Markoff)
**Why a worktree:** `master` is dirty with in-progress Cluster M alignment work (`CanvasAlignmentStrategy`, scene/view/command edits). This work must not share that tree.
**Punch-list item absorbed:** P5 “Paste-from-HTML → Markdown” (bucket ③ re-target; `docs/punch-list.md` + PARITY-MATRIX “Paste/drop images, paste HTML→MD”). Image paste/drop is **out of scope**.

---

## Situation

Per PROJECT-STATE, current focus is Cluster M (canvas authoring; M3 closed, M4 next) on dirty `master`. This clipboard work is a **parallel track**, isolated in worktrees.

**Markoff standalone `~/dev/Markoff` master (`0bae686e`) is also mid-arc** (G1 canvas a11y, 20 commits past the Corbomite submodule pin). Do **not** branch from that tip. Branch Markoff from the pin Corbomite currently consumes: `a3d8055e`.

---

## What exists today

| Surface | Copy | Paste |
|---|---|---|
| **Canvas `View`** (LivePreview) | `setText(selectedText())` — raw D2 buffer slices joined with `\n\n`. Drag-out already dual-MIME `text/plain` + `text/markdown` of the **same** bytes. List items lose markers (buffers store content-only). | `insertText(clipboard()->text())` — line-split + Enter via `StructuralKeyHandler`. Ignores HTML/RTF. |
| **Live `LiveClipboardController`** | `text/plain` = `reconstructFlatMarkdown` + `application/x-markoff-blocks` JSON. Closest to “right”. | Structured blocks, else `hasText()` only. Ignores HTML/RTF. |
| **Source `Editor`** | Native `QPlainTextEdit` — markdown as `text/plain` only. | Native — no HTML→MD. |
| **Styled `Editor`** | Native `QTextEdit` — Qt HTML/RTF of the **themed** document, **not** source markdown. Reading-mode copy is already “rich” but unfaithful as markdown. | Native rich insert into `QTextDocument`, then bound back to D2 — lossy and not markdown. |
| **Corbomite host** | No Edit-menu copy/paste at all (`corbomiteui.rc.in` Edit = undo/redo/find). Format verbs already dispatch through `MarkdownView`. | — |
| **Whiteboard `libs/canvas`** | Separate node-JSON clipboard (M2.4). **Must not be touched.** Distinct focus owner. | — |

Core already has the paste *sink*: `MarkoffDocument::applyStructuredPaste` / `reconstructFlatMarkdown` / `Cmd::pasteMarkdown`. What is missing is a **codec** (MD⇄HTML/RTF/plain) and a **single copy/paste chokepoint** every leaf calls.

Audit intent (`editor.md` rec 15): `Markoff::htmlToMarkdown(QString)` as a shared free function, called when `source->hasHtml()`. Obsidian: `htmlToMarkdown` is Turndown; `Ctrl+Shift+V` is paste-as-plain.

---

## Design decisions

### 1. Default Copy is multi-flavor; “Copy as X” is exclusive

Default `Ctrl+C` writes **all** of these onto one `QMimeData` (receivers pick):

| MIME | Payload |
|---|---|
| `text/plain` | **Raw markdown** of the selection (round-trips into other MD editors / terminals) |
| `text/markdown` | Same bytes as `text/plain` |
| `text/html` | Semantic HTML (`<strong>`, `<em>`, `<h1>`…, `<a href>`, lists) — **not** themed Qt HTML |
| `text/rtf` | Semantic RTF generated from the same structure |
| `application/x-markoff-blocks` | Existing Live JSON (canvas/source/styled grow this too) |

**Copy as Markdown / Plain / HTML / RTF** each put **only** that flavor. This is the whole reason exclusive commands exist: Word/LibreOffice will prefer `text/html` over markdown if both are present.

**Copy as Plain** = visible text with markup stripped (`**bold**` → `bold`, `# Title` → `Title`, list markers kept as `- `/`1. `). This is **not** what `text/plain` is on default Copy.

### 2. Smart paste priority (lossy, acknowledged)

```
1. application/x-markoff-blocks  (internal round-trip)
2. text/markdown                 (already markdown)
3. text/html     → htmlToMarkdown
4. text/rtf      → rtfToMarkdown   (only if no HTML)
5. text/plain                    (last resort)
```

Then insert via `applyFlatEdit` / `Cmd::pasteMarkdown` (parser rematerializes blocks) — **not** canvas’s char-by-char `insertText`.

**Paste as Plain** (`Ctrl+Shift+V`, Obsidian-faithful): take `text/plain` if present, else strip the HTML/RTF conversion to visible text, insert as literal (no second markdown parse of the stripped text).

**Paste as Markdown**: force HTML/RTF→MD even when `text/plain` is also present (browsers put a flattened plain fallback that would otherwise win if we ever inverted the order). Default smart paste already prefers HTML over plain, so this command is the explicit override.

### 3. Codecs live in `markoff-core`, widget-free

New files:

- `libs/markoff-core/include/markoff/core/ClipboardCodec.h`
- `libs/markoff-core/src/ClipboardCodec.cpp`
- `libs/markoff-core/tests/tst_clipboard_codec.cpp`

Public API (pure functions, no `QClipboard`):

```cpp
namespace Markoff::ClipboardCodec {
    QString markdownToPlain(const QByteArray &markdown);   // strip markup
    QString markdownToHtml(const QByteArray &markdown);    // semantic HTML
    QByteArray markdownToRtf(const QByteArray &markdown);  // semantic RTF

    QByteArray htmlToMarkdown(const QString &html);        // lossy
    QByteArray rtfToMarkdown(const QByteArray &rtf);       // lossy; subset
    QString htmlToPlain(const QString &html);
    QString rtfToPlain(const QByteArray &rtf);

    // Selection → mime. `exclusive` = one flavor; otherwise all flavors.
    enum class Flavor { All, Markdown, Plain, Html, Rtf };
    QMimeData *mimeFromMarkdown(const QByteArray &markdown,
                                const QJsonDocument &blocksPayload, // may be empty
                                Flavor flavor);

    // Incoming mime → markdown (smart) or plain.
    enum class PasteMode { Smart, Plain };
    QByteArray markdownFromMime(const QMimeData *mime, PasteMode mode);
}
```

**MD→HTML/plain:** walk `inlineSpansFor()` + block kinds. Skip `isDelimiter` spans for plain. Emit tags/`**` reconstruction from span flags for HTML. Reuse `reconstructFlatMarkdown` for the raw-markdown flavor. Do **not** route through `Styled::DocumentRenderer` (themed Qt HTML is the wrong export).

**HTML→MD:** `QTextDocument::setHtml` + walk `QTextBlock`/`QTextFragment` char formats. v1 mapping:

| HTML | Markdown |
|---|---|
| `h1`–`h6` | ATX `#` |
| `strong`/`b` | `**` |
| `em`/`i` | `_` |
| `s`/`del`/`strike` | `~~` |
| `code` (inline) | `` ` `` |
| `pre`/`code` | fenced block |
| `a[href]` | `[text](url)` |
| `ul`/`ol`/`li` | `- `/`1. ` |
| `blockquote` | `>` |
| `hr` | `---` |
| `img[src]` | `![]()` |
| simple `<table>` | GFM pipes |
| everything else | visible text |

No Turndown JS, no new vendor. Qt Gui is already a `markoff-core` dep.

**RTF export:** build a temporary `QTextDocument` from the HTML (or from formats) and `QTextDocumentWriter(&ba, "RTF")`.

**RTF import:** Office/browser clipboards almost always include HTML too — HTML wins. RTF-only fallback: small control-word subset (`\b \i \ul \strike \par \pard \plain \bullet` + `HYPERLINK` fields). Full RTF is not a goal. Linux Qt has no public RTF reader.

### 4. Leaf chokepoint: `MarkdownView` verbs + one helper in core

Add to `MarkdownView` (defaults no-op, matching format verbs):

```cpp
virtual void copy();
virtual void copyAsPlain();
virtual void copyAsMarkdown();
virtual void copyAsHtml();
virtual void copyAsRtf();
virtual void cut();
virtual void paste();          // smart
virtual void pasteAsPlain();   // Ctrl+Shift+V
```

Each leaf override:

1. Snapshot selection → markdown bytes + optional blocks JSON.
2. `ClipboardCodec::mimeFromMarkdown(...)` → `QClipboard`.
3. Paste: `markdownFromMime(...)` → `applyFlatEdit` / `Cmd::pasteMarkdown` at the caret.

**Canvas:** replace `View::copy/cut/paste` bodies; `createMimeDataFromSelection()` shares the same helper (drag-out becomes multi-flavor too). Reconstruct list markers (today they drop).

**Live:** `LiveClipboardController::copy/paste` call the codec; keep `kBlocksMime`.

**Source:** intercept `QPlainTextEdit` copy/paste (`createMimeDataFromSelection` / `insertFromMimeData` on a tiny inner subclass, or event filter). Do not let native copy emit only plain.

**Styled:** same intercept so Reading-mode copy emits markdown+HTML, not themed Qt HTML. Paste-as-markdown writes D2, not native rich text.

### 5. Corbomite host: Edit menu + command palette, no double-fire

`MainWindow` already routes format verbs with `addEditorActionBase`. Add the same for clipboard verbs. **Do not** add `KStandardAction::copy/paste` with the standard shortcuts — leaves already handle `Ctrl+C/X/V` in `keyPressEvent`. Host actions:

| Action name | Label | Shortcut |
|---|---|---|
| `edit_copy_as_markdown` | Copy as Markdown | (none, palette/menu) |
| `edit_copy_as_plain` | Copy as Plain Text | (none) |
| `edit_copy_as_html` | Copy as HTML | (none) |
| `edit_copy_as_rtf` | Copy as RTF | (none) |
| `edit_paste_plain` | Paste as Plain Text | `Ctrl+Shift+V` |

Default Copy/Paste stay leaf-local (upgraded). Optional: also register `edit_copy`/`edit_paste` **without** standard shortcuts for the command palette, calling the same verbs.

`src/app/corbomiteui.rc.in` Edit menu, after undo/redo:

```
Cut / Copy / Paste
Copy as → Markdown / Plain Text / HTML / RTF
Paste as Plain Text
Find…
```

Canvas `View::buildContextMenu` grows a “Copy as” submenu. Live/Source/Styled context menus if they have one; otherwise host menu is enough.

Enablement: copy-as requires a selection (or whole-doc if Obsidian-empty-copy is later desired — **v1: selection required**, matching canvas today). Paste-as-plain enabled when clipboard has any of text/html/rtf/plain.

---

## Worktree setup (do first, before any code)

Markoff already uses `~/dev/Markoff/.worktrees/<name>`. Mirror that, and **place the Markoff worktree at the Corbomite worktree’s submodule path** so CMake keeps using `libs/markoff-family` with no pin dance during development.

```bash
# 1. Corbomite worktree from current clean master (a470ca8f), NOT the dirty tree
git -C /home/clinton/dev/Corbomite worktree add -b feature/rich-clipboard \
    /home/clinton/dev/Corbomite/.worktrees/rich-clipboard

# 2. The worktree has an empty gitlink dir for the submodule — replace it
#    with a Markoff worktree of the SAME branch name, based on the pin.
rmdir /home/clinton/dev/Corbomite/.worktrees/rich-clipboard/libs/markoff-family
git -C /home/clinton/dev/Markoff worktree add -b feature/rich-clipboard \
    /home/clinton/dev/Corbomite/.worktrees/rich-clipboard/libs/markoff-family \
    a3d8055e
```

Configure a **separate** build dir inside the Corbomite worktree (`cmake --preset` will need a worktree-local `CMakeUserPresets` or an explicit `-B build-dev`). Do not share `~/dev/Corbomite/build-dev` with the dirty master tree.

When Markoff work is ready to land: commit on Markoff `feature/rich-clipboard`, push, then in the Corbomite worktree `git add libs/markoff-family` to re-pin.

**Never** `git add -A` (testvaults stay dirty on master; this worktree starts clean).

---

## Implementation phases (TDD)

### Phase 0 — Worktrees + build

- Create both worktrees as above.
- Configure/build the Corbomite worktree against the nested Markoff checkout.
- Confirm existing clipboard tests still pass: `tst_canvas_selection`, `tst_canvas_drag_drop`, `tst_canvas_context_menu`, `tst_live_render_clipboard_*`, `tst_markoff_doc_apply_structured_paste`.

### Phase 1 — `ClipboardCodec` (Markoff core, no widgets)

Named tests in `tst_clipboard_codec`:

- `markdownToPlain_stripsBoldItalicStrikeCodeHeadings`
- `markdownToPlain_keepsListMarkersAndLinkText` (`[label](url)` → `label`)
- `markdownToHtml_emitsSemanticTagsNotThemedCss`
- `markdownToRtf_roundTripsBoldItalicViaQTextDocument`
- `htmlToMarkdown_boldItalicLinkListHeading`
- `htmlToMarkdown_tableToGfmPipes`
- `htmlToMarkdown_stripsScriptAndStyle`
- `rtfToMarkdown_boldItalicPar` (hand-built minimal RTF)
- `markdownFromMime_prefersMarkoffBlocksThenMarkdownThenHtmlThenRtfThenPlain`
- `markdownFromMime_plainModeIgnoresHtml`
- `mimeFromMarkdown_allFlavorsHasFiveFormats`
- `mimeFromMarkdown_exclusiveHtmlHasNoPlain` (the Word-leak falsifier)

Do **not** touch leaves until this suite is green.

### Phase 2 — Canvas leaf (the requested surface)

- `View::copy/cut` → `ClipboardCodec::mimeFromMarkdown` (All).
- `View::paste` → smart `markdownFromMime` then `Cmd::pasteMarkdown` / `applyFlatEdit` at caret (replace selection first). Keep `insertText` for drag-in of **plain** drops only, or route drops through the same codec.
- `copyAs*` / `pasteAsPlain` methods on `View` + `EditorWidget` overrides of the new `MarkdownView` verbs.
- `createMimeDataFromSelection` uses the All-flavor mime (drag-out gains HTML/RTF).
- Context menu: Copy as submenu + Paste as Plain Text; Paste enabled if HTML/RTF present even when `text()` is empty.
- Reconstruct list markers (fix latent bug). Update `tst_canvas_selection` expected bytes for list cases; add:

  - `copy_writesHtmlAndMarkdownMime`
  - `copyAsPlain_stripsMarkers`
  - `paste_htmlBecomesMarkdown`
  - `pasteAsPlain_stripsIncomingHtml`
  - `copy_listItemIncludesMarker`

### Phase 3 — Live + Source + Styled (so “all views get the fix”)

- [x] Live: replace `LiveClipboardController` mime assembly/paste fallback with the codec (`e81fedea`). Existing `tst_live_render_clipboard_*` stay.
- [x] Source: `InnerEditor` overrides mime create/insert + MarkdownView verbs (`365e9b91`, `tst_source_rich_clipboard`).
- [x] Styled: same on `StructuralTextEdit` so Reading copy is markdown-faithful (`365e9b91`, `tst_styled_rich_clipboard`).
- `ViewContractChecks` grows copy/paste no-crash calls (optional — not done).

### Phase 4 — Corbomite host wiring

- [x] `MainWindow` `addEditorActionBase` for the eight verbs (`821f714c`).
- [x] `corbomiteui.rc.in` Edit menu (bump `version` attr so KXMLGUI reloads).
- [x] `updateEditorActionStates`: copy-as gated on selection + `hasCursor()`; paste-as-plain gated on `!isReadOnly() && hasEditing()` plus clipboard flavors.
- [x] Punch-list HTML-paste item ticked; PARITY-MATRIX row stays 🟡 (images still missing).

### Phase 5 — Live eyeball (gate) — **PASSED 2026-08-20**

Against the worktree binary, not master:

1. [x] Canvas LivePreview multi-flavor Ctrl+C → Kate (markdown) / LibreOffice (formatted).
2. [x] Copy as Plain → stripped text.
3. [x] Copy as HTML → exclusive (no markdown plain).
4. [x] Foreign HTML paste → markdown links/lists/bold; LO Block Quote → Quote block.
5. [x] Ctrl+Shift+V → flattened plain.
6. [x] Whiteboard node clipboard untouched (M2.4).
7. [x] Reading-mode copy → markdown-faithful (not themed Qt HTML).

**Eyeball follow-up fixed same session:** Quote → LO as HTML/RTF landed as Body Text because export ignored `blockQuoteDepth` (Markoff `bafa3095`). User re-confirmed both HTML and RTF paste as Block Quote after the rebuild.

---

## Out of scope

- Paste/drop of **images** into the vault (same PARITY-MATRIX row; separate punch item).
- Empty-selection `Mod+C` copies whole note (Obsidian Live Preview quirk).
- Plugin `htmlToMarkdown` API / `sanitizeHTMLToDom` allowlist.
- Themed HTML export / “Copy as styled HTML”.
- Wikilink rewriting on paste.
- Touching dirty Cluster M files on `master`.

---

## Risk / traps

- **Double-handling Ctrl+C:** host `KStandardAction::copy` + leaf `keyPressEvent` = two clipboard writes. Host exclusive commands only; leave standard chords in the leaf.
- **`text/plain` identity:** default plain **is markdown**. Stripped text is a different command. Mixing these makes round-trip paste destroy markup.
- **Canvas `selectedText()` vs Live `reconstructFlatMarkdown`:** one function after this work. List-item buffers have no marker.
- **Styled native copy:** today it already puts HTML on the clipboard — the *wrong* HTML. Phase 3 is a behavior change in Reading mode and must be eyeballed.
- **Offscreen clipboard:** existing tests already use `QGuiApplication::clipboard()` under `QT_QPA_PLATFORM=offscreen`; follow that, don’t invent a fake clipboard.
- **Whiteboard collision:** `libs/canvas` node clipboard is a different `QMimeData` type (`.canvas` JSON). Don’t reuse `application/x-markoff-blocks` there.

---

## Suggested commit split

1. `feat(core): ClipboardCodec markdown/html/rtf/plain` (Markoff)
2. `feat(canvas): multi-flavor copy + smart paste` (Markoff)
3. `feat(live,source,styled): route clipboard through ClipboardCodec` (Markoff)
4. `feat(editor): Copy as / Paste as Plain on MarkdownView + Edit menu` (Corbomite, after re-pin)
