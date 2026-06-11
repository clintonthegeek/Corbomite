# Completion Revival Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Revive `[[`-wikilink and `#`-tag autocompletion (names, aliases, headings, existing `^block` ids) in all editable leaves, leaf-agnostically through the `Markoff::MarkdownView` base.

**Architecture:** One new upstream primitive (`MarkdownView::caretRect()`, contract-v2 extension in the markoff-family submodule, all three leaves) + a Corbomite-side stack: suggester interface v2 (structured items), a pure `LineResolve` coordinate bridge, and a reactive `CompletionController` that replaces the dead `NoteEditorWidget` stubs. Insertion goes through `MarkoffDocument::applyBlockEdit` (undo-integrated, propagates to every leaf). Spec: [`../specs/2026-06-11-completion-revival-design.md`](../specs/2026-06-11-completion-revival-design.md) — read it first; section refs below (§N) point there.

**Tech Stack:** Qt 6.8 Widgets + QtQuick (Live leaf), KF6, CMake presets. Corbomite tests: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest …`. Markoff tests: `scripts/run-tests.sh`.

**Phases:** A1 = Tasks 1–13 (caret-rect contract + `[[` names + `#` tags end-to-end). A2 = Tasks 14–15 (aliases, headings). A3 = Tasks 16–17 (existing `^blocks`, closeout). Each phase ends green + committed + pushed (both repos where touched).

**Ground rules (repo conventions — non-negotiable):**
- **Never `git add -A`** in Corbomite — `testvaults/` is deliberately dirty; stage by explicit path.
- Corbomite build: `cmake --build --preset dev -j 10` from `/home/clinton/dev/Corbomite`. Full suite: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10` — baseline **260/260**.
- Markoff build: `cmake --build build-dev -j 10` from `/home/clinton/dev/Corbomite/libs/markoff-family`. Suite: `scripts/run-tests.sh -E 'tst_realistic|tst_benchmark'` — baseline **269/272** (the 3 reds are documented queue #10 failures: `tst_live_render_e2_nav_shift_extend`, `tst_live_render_focus_chokepoint_invariant`, `tst_live_render_cursor_typing_invariant`; any OTHER failure is a new regression).
- The markoff submodule is on detached HEAD at the pin. Before Task 1: `cd libs/markoff-family && git fetch origin && git checkout origin/master` (expect `af91a936` or later). Push markoff commits with `git push origin HEAD:master`.
- Commit messages end with `Co-Authored-By:` trailer per session convention.

---

## File structure

**markoff-family (upstream, Tasks 1–4):**
| File | Change |
|---|---|
| `libs/markoff-core/include/markoff/core/MarkdownView.h` | + `virtual QRect caretRect() const` (default invalid) |
| `libs/markoff-core/tests/tst_markdown_view_base.cpp` | + base-default slot |
| `libs/markoff-source/include/markoff/source/Editor.h` + `src/Editor.cpp` | + override |
| `libs/markoff-styled/include/markoff/styled/Editor.h` + `src/Editor.cpp` | + override |
| `libs/markoff-source/tests/tst_view_contract_source.cpp` | + caret-rect slots |
| `libs/markoff-styled/tests/tst_view_contract_styled.cpp` | + caret-rect slots |
| `libs/markoff-live/include/markoff/live/EditorWidget.h` + `src/EditorWidget.cpp` | + override (activeFocusItem strategy, §4) |
| `libs/markoff-live/tests/tst_view_contract_live_caret_rect.cpp` (new) + `tests/CMakeLists.txt` | live contract test |
| `docs/specs/2026-06-11-caret-rect-contract-design.md` (new) | mini-spec citing the Corbomite spec |

**Corbomite (Tasks 5–17):**
| File | Change |
|---|---|
| `libs/core/include/corbomite/core/EditorSuggest.h` | interface v2 (§7) |
| `libs/core/src/EditorSuggestManager.cpp` | cursorPos clamp |
| `tests/core/tst_editorsuggest.cpp` | v2 mock + clamp slot |
| `src/editor/WikiLinkSuggest.{h,cpp}` | v2; A2 aliases/headings; A3 blocks |
| `src/editor/TagSuggest.{h,cpp}` | v2 |
| `src/editor/LineResolve.{h,cpp}` (new) | coordinate bridge (§5) |
| `src/editor/CompletionController.{h,cpp}` (new) | controller (§6) |
| `src/editor/NoteEditorWidget.{h,cpp}` | delete stubs; own + feed controller |
| `src/app/MainWindow.cpp` | wire resolver/cache into WikiLinkSuggest |
| `src/editor/CMakeLists.txt` (or wherever editor sources are listed — verify with `grep -n "WikiLinkSuggest" src/**/CMakeLists.txt`) | add new .cpp files |
| `tests/editor/tst_line_resolve.cpp`, `tst_wikilink_suggest.cpp`, `tst_tag_suggest.cpp`, `tst_completion_controller.cpp`, `tst_note_editor_widget_completion.cpp` (all new) + `tests/editor/CMakeLists.txt` | tests |
| `docs/PARITY-MATRIX.md`, `docs/punch-list.md`, `docs/PROJECT-STATE.md`, `docs/decisions-archive.md` | per phase exits |

---

# Phase A1

### Task 1: markoff — base `caretRect()` default

**Files:**
- Modify: `libs/markoff-family/libs/markoff-core/include/markoff/core/MarkdownView.h`
- Test: `libs/markoff-family/libs/markoff-core/tests/tst_markdown_view_base.cpp`

All paths in Tasks 1–4 are relative to `/home/clinton/dev/Corbomite/libs/markoff-family`. First sync the submodule:

```bash
cd /home/clinton/dev/Corbomite/libs/markoff-family
git fetch origin && git checkout origin/master   # detached at master tip (af91a936+)
```

- [ ] **Step 1: Write the failing test.** In `tst_markdown_view_base.cpp`, find the existing test class's `private Q_SLOTS:` section and add (match the file's existing style for constructing a bare `MarkdownView` — it already instantiates one in other slots):

```cpp
    // Contract-v2 extension (caret-rect, 2026-06-11): the base default is
    // an INVALID rect — "no caret established". Leaves override.
    void caretRect_baseDefault_isInvalid()
    {
        Markoff::MarkdownView v;
        QVERIFY(!v.caretRect().isValid());
    }
```

- [ ] **Step 2: Verify it fails to compile** (no such member):

```bash
cmake --build build-dev --target tst_markdown_view_base -j 10 2>&1 | tail -5
```
Expected: error `no member named 'caretRect' in 'Markoff::MarkdownView'`.

- [ ] **Step 3: Implement.** In `MarkdownView.h`, after the `hasEditing()` line (`virtual bool hasEditing() const { return false; }`), add:

```cpp
    /// Caret rectangle in THIS widget's coordinate system, or an invalid
    /// QRect when no caret is established (no document, no focus, cursor
    /// not in a text-bearing state). Consumers anchor transient UI
    /// (completion popups) at bottomLeft(). Contract-v2 extension
    /// (2026-06-11 caret-rect; driven by Corbomite completion revival).
    virtual QRect caretRect() const { return {}; }
```

`<QWidget>` is already included (QRect comes with it); no other change.

- [ ] **Step 4: Build + run:**

```bash
cmake --build build-dev --target tst_markdown_view_base -j 10 && \
QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_markdown_view_base 2>&1 | tail -4
```
Expected: all slots PASS including `caretRect_baseDefault_isInvalid`.

- [ ] **Step 5: Commit** (in the submodule):

```bash
git add libs/markoff-core/include/markoff/core/MarkdownView.h libs/markoff-core/tests/tst_markdown_view_base.cpp
git commit -m "feat(core): MarkdownView::caretRect() contract — base default invalid

Contract-v2 extension: leaves report the caret rectangle in their own
widget coordinates for transient-UI anchoring (completion popups).
Driven by Corbomite's completion revival (spec 2026-06-11 there).

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task 2: markoff — Source + Styled overrides

**Files:**
- Modify: `libs/markoff-source/include/markoff/source/Editor.h`, `libs/markoff-source/src/Editor.cpp`
- Modify: `libs/markoff-styled/include/markoff/styled/Editor.h`, `libs/markoff-styled/src/Editor.cpp`
- Test: `libs/markoff-source/tests/tst_view_contract_source.cpp`, `libs/markoff-styled/tests/tst_view_contract_styled.cpp`

- [ ] **Step 1: Write the failing tests.** In `tst_view_contract_source.cpp`, add a slot to the existing contract class (reuse its existing document/editor construction helpers — read the file's other slots first and copy the local pattern for creating an `Editor` + attached doc; the code below shows the assertion logic to embed):

```cpp
    void caretRect_validAfterAttach_tracksCursor()
    {
        Markoff::MarkoffDocument doc(1);
        doc.loadFromMarkdown(QByteArrayLiteral("Alpha one.\n\nBeta two.\n\nGamma three.\n"));
        Markoff::Source::Editor ed;
        ed.resize(600, 400);
        ed.show();
        QVERIFY(QTest::qWaitForWindowExposed(&ed));

        QVERIFY(!ed.caretRect().isValid());   // before attach: no caret
        ed.setDocument(&doc);
        ed.setCursorPosition({1, 1});
        const QRect r1 = ed.caretRect();
        QVERIFY(r1.isValid());
        QVERIFY(ed.rect().contains(r1.topLeft()));
        ed.setCursorPosition({3, 1});          // a later visual line
        const QRect r3 = ed.caretRect();
        QVERIFY(r3.isValid());
        QVERIFY2(r3.top() > r1.top(), "caretRect must move down with the cursor");
    }
```

Add the same slot (substituting `Markoff::Styled::Editor`) to `tst_view_contract_styled.cpp`.

- [ ] **Step 2: Verify both fail.** Build the two test targets; expected: `r1.isValid()` FAILS (base default returns invalid) — compile succeeds because the base virtual exists since Task 1.

```bash
cmake --build build-dev --target tst_view_contract_source tst_view_contract_styled -j 10 && \
QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_view_contract_source 2>&1 | grep -E "caretRect|FAIL" | head -3
```

- [ ] **Step 3: Implement Source.** `Editor.h` — add to the "MarkdownView contract" override block:

```cpp
    QRect caretRect() const override;
```

`Editor.cpp` (member is `m_editor`, a `QPlainTextEdit*` — see `plainTextEdit()` accessor):

```cpp
QRect Editor::caretRect() const
{
    if (!document()) return {};
    const QRect r = m_editor->cursorRect();               // viewport coords
    return QRect(m_editor->viewport()->mapTo(const_cast<Editor *>(this), r.topLeft()),
                 r.size());
}
```

- [ ] **Step 4: Implement Styled.** Same declaration in `markoff-styled/.../Editor.h`. In `Editor.cpp`, the inner widget is reachable via the existing public accessor `textEdit()` (a `QTextEdit*`; the private member may be in a d-pointer — use whichever the file's other methods use):

```cpp
QRect Editor::caretRect() const
{
    if (!document()) return {};
    QTextEdit *te = textEdit();
    if (!te) return {};
    const QRect r = te->cursorRect();
    return QRect(te->viewport()->mapTo(const_cast<Editor *>(this), r.topLeft()),
                 r.size());
}
```

- [ ] **Step 5: Build + run both:**

```bash
cmake --build build-dev --target tst_view_contract_source tst_view_contract_styled -j 10 && \
QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_view_contract_source 2>&1 | tail -3 && \
QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_view_contract_styled 2>&1 | tail -3
```
Expected: PASS, 0 failed in each.

- [ ] **Step 6: Commit:**

```bash
git add libs/markoff-source/include/markoff/source/Editor.h libs/markoff-source/src/Editor.cpp \
        libs/markoff-styled/include/markoff/styled/Editor.h libs/markoff-styled/src/Editor.cpp \
        libs/markoff-source/tests/tst_view_contract_source.cpp libs/markoff-styled/tests/tst_view_contract_styled.cpp
git commit -m "feat(source,styled): caretRect() — inner text widget cursorRect mapped to editor coords

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task 3: markoff — Live override

**Files:**
- Modify: `libs/markoff-live/include/markoff/live/EditorWidget.h`, `libs/markoff-live/src/EditorWidget.cpp`
- Create: `libs/markoff-live/tests/tst_view_contract_live_caret_rect.cpp`
- Modify: `libs/markoff-live/tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test** (model: `tst_view_contract_live_attach_window.cpp` — same includes, same `makeParagraphs` helper, same link pattern):

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// MarkdownView contract — caretRect() on the live leaf (contract-v2
// extension, 2026-06-11; consumer: Corbomite completion revival).
//
// INVARIANTS note: caretRect is a read-only query over the focused QML
// delegate's TextEdit (window activeFocusItem) — no new cursor authority,
// no stored state (INVARIANTS #3 trivially satisfied). INVARIANTS #5:
// this test drives the REAL path — QQuickWidget scene + initial-focus
// seed + activeFocusItem — not a mock.

#include <markoff/core/MarkoffDocument.h>
#include <markoff/live/EditorWidget.h>
#include <markoff/live/LiveCursorState.h>
#include <markoff/live/LiveListModelBinding.h>

#include <QStringList>
#include <QTest>

using namespace Markoff::Live;

namespace {
QString makeParagraphs(int count)
{
    QStringList blocks;
    blocks.reserve(count);
    for (int i = 0; i < count; ++i)
        blocks.append(QStringLiteral("Paragraph %1 line A.").arg(i));
    return blocks.join(QStringLiteral("\n\n"));
}
} // namespace

class TestViewContractLiveCaretRect : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void caretRect_invalidBeforeAttach()
    {
        EditorWidget w;
        w.resize(800, 300);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(!w.caretRect().isValid());
    }

    void caretRect_validAfterSeed_withinBounds_tracksCursor()
    {
        Markoff::MarkoffDocument doc(1);
        doc.loadFromMarkdown(makeParagraphs(8).toUtf8());

        EditorWidget w;
        w.resize(800, 400);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setDocument(&doc);

        // Wait for the initial-focus seed to establish a TextCaret.
        auto *cs = w.binding()->cursorState();
        QTRY_VERIFY(cs->currentTextCaret().has_value());

        QTRY_VERIFY2(w.caretRect().isValid(),
                     "caretRect stays invalid after a TextCaret is established");
        const QRect r0 = w.caretRect();
        QVERIFY2(w.rect().contains(r0.topLeft()),
                 qPrintable(QStringLiteral("caret %1,%2 outside widget %3x%4")
                                .arg(r0.x()).arg(r0.y())
                                .arg(w.width()).arg(w.height())));

        // Move to a later block; the rect must follow downward.
        w.setCursorPosition({5, 1});
        QTRY_VERIFY(w.caretRect().isValid() && w.caretRect().top() > r0.top());
    }
};

QTEST_MAIN(TestViewContractLiveCaretRect)
#include "tst_view_contract_live_caret_rect.moc"
```

Register in `libs/markoff-live/tests/CMakeLists.txt`, after the `tst_view_contract_live_doc_destroyed` block:

```cmake
# Contract-v2 caret-rect extension (2026-06-11): caretRect() over the
# focused QML delegate's TextEdit. Same link pattern as tst_view_contract_live.
qt_add_executable(tst_view_contract_live_caret_rect
    tst_view_contract_live_caret_rect.cpp)
target_link_libraries(tst_view_contract_live_caret_rect PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::QuickWidgets
    Qt6::Widgets Qt6::Test
    markoff_live markoff_core
    markoff_liveplugin markoff_liveplugin_init)
add_test(NAME tst_view_contract_live_caret_rect
         COMMAND tst_view_contract_live_caret_rect)
set_tests_properties(tst_view_contract_live_caret_rect PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: Verify it fails:**

```bash
cmake -S . -B build-dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null && \
cmake --build build-dev --target tst_view_contract_live_caret_rect -j 10 && \
QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_view_contract_live_caret_rect 2>&1 | tail -5
```
Expected: `caretRect_validAfterSeed…` FAILs (base default invalid); `caretRect_invalidBeforeAttach` passes.

- [ ] **Step 3: Implement.** `EditorWidget.h` — add near the other contract overrides:

```cpp
    QRect caretRect() const override;
```

`EditorWidget.cpp` — add `#include <QQuickWindow>` to the include block, then:

```cpp
QRect EditorWidget::caretRect() const
{
    // Read-only query over the focused QML text item (INVARIANTS #3: no
    // second cursor store). Whenever a TextCaret / cell edit is live, the
    // window's activeFocusItem IS the focused TextEdit, for every
    // text-bearing delegate kind — no per-delegate QML changes needed.
    if (!document() || !d->quickWidget) return {};
    QQuickWindow *win = d->quickWidget->quickWindow();
    if (!win) return {};
    QQuickItem *focus = win->activeFocusItem();
    if (!focus) return {};
    const QVariant cr = focus->property("cursorRectangle");
    if (!cr.isValid() || !cr.canConvert<QRectF>()) return {};   // not a text item
    const QRectF sceneRect = focus->mapRectToScene(cr.toRectF());
    // Scene coords == QQuickWidget-local coords; translate into this widget.
    const QPoint origin = d->quickWidget->mapTo(const_cast<EditorWidget *>(this), QPoint(0, 0));
    return sceneRect.translated(origin.x(), origin.y()).toRect();
}
```

- [ ] **Step 4: Run:**

```bash
cmake --build build-dev --target tst_view_contract_live_caret_rect -j 10 && \
QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_view_contract_live_caret_rect 2>&1 | tail -4
```
Expected: 2 slots PASS. (If `validAfterSeed` is flaky on `activeFocusItem` timing, the QTRY wrappers absorb it; a persistent failure means the mapping is wrong — debug, don't loosen the test.)

- [ ] **Step 5: Falsifiability probe (INVARIANTS #4).** Temporarily break the mapping — in `caretRect()` replace `return sceneRect.translated(…).toRect();` with `return {};` — rebuild, confirm the test FAILS, then restore and confirm PASS. Do not commit the probe.

- [ ] **Step 6: Commit:**

```bash
git add libs/markoff-live/include/markoff/live/EditorWidget.h libs/markoff-live/src/EditorWidget.cpp \
        libs/markoff-live/tests/tst_view_contract_live_caret_rect.cpp libs/markoff-live/tests/CMakeLists.txt
git commit -m "feat(live): caretRect() via window activeFocusItem cursorRectangle

Read-only query, no new cursor authority (INVARIANTS #3); works for all
text-bearing delegates without per-delegate QML changes. Falsifiability-
probed per INVARIANTS #4.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task 4: markoff — mini-spec, full suite, push

**Files:**
- Create: `docs/specs/2026-06-11-caret-rect-contract-design.md` (in the submodule)

- [ ] **Step 1: Write the mini-spec:**

```markdown
# caretRect() — MarkdownView contract extension

**Date:** 2026-06-11 · **Status:** Shipped with this commit · **Driver:**
Corbomite completion revival
(`Corbomite:docs/superpowers/specs/2026-06-11-completion-revival-design.md` §4).

`virtual QRect MarkdownView::caretRect() const` — the caret rectangle in the
view widget's own coordinate system; invalid `QRect{}` when no caret is
established (no document / no focus / no text-bearing cursor state). Base
default: invalid. Consumers anchor transient UI (completion popups) at
`bottomLeft()`.

Implementations: source/styled map the inner text widget's `cursorRect()`
viewport→editor; live reads the window `activeFocusItem`'s
`cursorRectangle` property and maps scene→widget (read-only query — no new
cursor authority, INVARIANTS #3; the focused TextEdit IS the active-focus
item whenever a TextCaret/cell edit is live, so all text-bearing delegate
kinds are covered without per-delegate QML work).

Tests: `tst_markdown_view_base::caretRect_baseDefault_isInvalid`,
caret-rect slots in `tst_view_contract_source` / `tst_view_contract_styled`,
`tst_view_contract_live_caret_rect` (falsifiability-probed).
```

- [ ] **Step 2: Full suite:**

```bash
scripts/run-tests.sh -E 'tst_realistic|tst_benchmark' 2>&1 | tail -6
```
Expected: only the 3 documented queue #10 reds; total count grew by 1 (the new live test).

- [ ] **Step 3: Commit + push:**

```bash
git add docs/specs/2026-06-11-caret-rect-contract-design.md
git commit -m "docs: caret-rect contract mini-spec

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
git push origin HEAD:master
git branch -f master HEAD
```

### Task 5: Corbomite — re-pin submodule

**Files:**
- Modify: gitlink `libs/markoff-family`

- [ ] **Step 1: Re-pin + rebuild + baseline:**

```bash
cd /home/clinton/dev/Corbomite
git add libs/markoff-family
cmake --build --preset dev -j 10 2>&1 | tail -2
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark -j 10 2>&1 | tail -3 && cd ..
```
Expected: build exit 0; **260/260**.

- [ ] **Step 2: Commit:**

```bash
git commit -m "build: re-pin markoff-family (caretRect contract-v2 extension)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task 6: Suggester interface v2 + dispatch clamp

**Files:**
- Modify: `libs/core/include/corbomite/core/EditorSuggest.h`
- Modify: `libs/core/src/EditorSuggestManager.cpp` (find `dispatch` — it iterates `m_suggests` calling `onTrigger`)
- Test: `tests/core/tst_editorsuggest.cpp`

- [ ] **Step 1: Rewrite the interface.** Replace the `EditorSuggestTriggerInfo` struct and the three pure virtuals in `EditorSuggest.h` (keep the file's includes, the `Component` base, and the class-level comments about insertion-order dispatch — update only what's shown):

```cpp
// Trigger range + filter query produced by EditorSuggest::onTrigger.
// Mirrors Obsidian's EditorSuggestTriggerInfo (domains/editor.md §3).
//
//   start, end : UTF-16 char offsets WITHIN lineText (line-relative)
//                bracketing the text the suggester will replace on accept.
//                end is the cursor position.
//   replaceEnd : optional replacement-range end (>= end); -1 means "same
//                as end". Lets wiki-link consume a pre-existing "]]"
//                after the cursor instead of producing "]]]]".
//   query      : lineText.mid(start, end - start) — what the popup's
//                fuzzy proxy will be fed (via EditorSuggestionSet::filter).
struct EditorSuggestTriggerInfo {
    int start = -1;
    int end = -1;
    int replaceEnd = -1;
    QString query;
};

// One completion candidate. insertText is the FULL literal replacement
// for [start, replaceEnd) — closing punctuation included; there is no
// post-selection transform step (selectSuggestion is retired).
struct EditorSuggestItem {
    QString display;       // shown in the popup (also what fuzzy filters)
    QString insertText;    // literal replacement text
    QString detail;        // optional context (path, target note); may be empty
};

// The candidate UNIVERSE for the current trigger mode plus the string the
// popup's fuzzy proxy should filter by. The split matters for sub-target
// modes: in `[[Note#se` the universe is *headings of Note* and the filter
// is `se` — the popup must never fuzzy-match `Note#se` against headings.
struct EditorSuggestionSet {
    QList<EditorSuggestItem> items;
    QString filter;
};
```

and in the class body replace `getSuggestions` + `selectSuggestion` with:

```cpp
    // Produce the candidate universe for the given trigger context. The
    // popup's CompletionFilterProxy does the fuzzy filtering/ranking
    // against set.filter — implementations return ALL mode-appropriate
    // candidates and do NOT pre-filter.
    virtual EditorSuggestionSet getSuggestions(const EditorSuggestTriggerInfo &ctx) = 0;
```

(`onTrigger` is unchanged.)

- [ ] **Step 2: Add the clamp** at the top of `EditorSuggestManager::dispatch` in `EditorSuggestManager.cpp` (before the suggester loop):

```cpp
    // Defensive clamp (punch-list P3): rapid-edit races can hand us a
    // cursorPos one past the line end; suggesters slice lineText with it.
    cursorPos = qBound(0, cursorPos, int(lineText.length()));
```

(If the parameter is `const int`, drop the `const` in both declaration and definition.)

- [ ] **Step 3: Update the test mock + add the clamp slot.** In `tests/core/tst_editorsuggest.cpp`, update `SigilSuggest` to the v2 shape — replace its `getSuggestions`/`selectSuggestion` with:

```cpp
    EditorSuggestionSet getSuggestions(const EditorSuggestTriggerInfo &ctx) override
    {
        EditorSuggestionSet set;
        set.filter = ctx.query;
        for (const QString &c : m_candidates)
            set.items.append({c, c, {}});
        return set;
    }
```

Keep every existing dispatch-semantics slot's assertions identical (they don't touch the item shape). If the mock previously clamped `cursorPos` internally (the `a6a664d5` fix), REMOVE that clamp — dispatch now owns it — and add:

```cpp
    void testDispatchClampsCursorPastLineEnd()
    {
        EditorSuggestManager manager;
        SigilSuggest s(QLatin1Char('@'), {QStringLiteral("hit")});
        manager.registerSuggest(&s);
        // line length 4; cursorPos 9 must clamp to 4, not assert/slice OOB.
        auto result = manager.dispatch(9, QStringLiteral("@hi!"), nullptr);
        QVERIFY(result.has_value());
        QCOMPARE(result->info.end, 4);
        QCOMPARE(result->info.query, QStringLiteral("hi!"));
    }
```

- [ ] **Step 4: Fix remaining compile errors.** `WikiLinkSuggest`/`TagSuggest` still implement the old signatures — Task 7 rewrites them properly, but the tree must compile now. Apply the Task 7 Step 3/Step 4 code for both suggesters as part of this task if you prefer one compile unit of work, OR mechanically convert them now exactly as shown in Task 7 (the plan's Task 7 then only adds their tests + `replaceEnd`/resolver behavior). Recommended: do the mechanical conversion now, leave behavioral additions to Task 7. Grep for any other implementer/caller:

```bash
grep -rn "getSuggestions\|selectSuggestion" src/ libs/ tests/ examples/ --include=*.cpp --include=*.h | grep -v build
```
Expected remaining call sites: the two suggesters, the dead `NoteEditorWidget` stubs (leave; deleted in Task 12), tests.

- [ ] **Step 5: Build + run:**

```bash
cmake --build --preset dev -j 10 2>&1 | tail -2
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_editorsuggest --output-on-failure 2>&1 | tail -3 && cd ..
```
Expected: PASS (7 slots).

- [ ] **Step 6: Commit:**

```bash
git add libs/core/include/corbomite/core/EditorSuggest.h libs/core/src/EditorSuggestManager.cpp \
        tests/core/tst_editorsuggest.cpp src/editor/WikiLinkSuggest.h src/editor/WikiLinkSuggest.cpp \
        src/editor/TagSuggest.h src/editor/TagSuggest.cpp
git commit -m "feat(core): EditorSuggest interface v2 — structured items, line-relative offsets pinned

EditorSuggestionSet{items, filter} replaces QStringList + selectSuggestion;
insertText fully resolved per item; replaceEnd added to TriggerInfo;
dispatch clamps cursorPos (punch-list P3). Plugin ABI break, pre-1.0
(EditorSuggestRegistrar unaffected; no in-tree plugin implements the
interface). Dispatch semantics unchanged — tst_editorsuggest slots intact.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task 7: WikiLinkSuggest + TagSuggest v2 behavior + tests

**Files:**
- Modify: `src/editor/WikiLinkSuggest.h`, `src/editor/WikiLinkSuggest.cpp`
- Modify: `src/editor/TagSuggest.cpp`
- Create: `tests/editor/tst_wikilink_suggest.cpp`, `tests/editor/tst_tag_suggest.cpp`
- Modify: `tests/editor/CMakeLists.txt`

- [ ] **Step 1: Write the failing tests.** `tests/editor/tst_wikilink_suggest.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// A1 coverage: trigger detection + names mode + replaceEnd + ambiguity.
// (A2 adds alias/heading slots; A3 adds blocks — same file.)
#include "WikiLinkSuggest.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"

#include <QObject>
#include <QTemporaryDir>
#include <QTest>

using namespace Corbomite;

class WikiLinkSuggestTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    std::unique_ptr<FileSystemAdapter> m_adapter;
    std::unique_ptr<Vault> m_vault;
    LinkResolver m_resolver;

    void writeNote(const QString &rel, const QByteArray &body)
    {
        const QString abs = m_dir.path() + QLatin1Char('/') + rel;
        QDir().mkpath(QFileInfo(abs).absolutePath());
        QFile f(abs);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(body);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        writeNote(QStringLiteral("Alpha.md"), "# Alpha\n");
        writeNote(QStringLiteral("Beta.md"), "# Beta\n");
        writeNote(QStringLiteral("sub/Beta.md"), "# Beta in sub\n");
        m_adapter = std::make_unique<FileSystemAdapter>();
        m_vault = std::make_unique<Vault>(m_adapter.get(), nullptr);
        m_vault->load(m_dir.path());
        QStringList paths;
        for (auto *tf : m_vault->getMarkdownFiles()) paths << tf->path;
        m_resolver.setVaultPaths(paths);
    }

    void trigger_afterDoubleBracket()
    {
        WikiLinkSuggest s(m_vault.get());
        auto info = s.onTrigger(6, QStringLiteral("x [[Al"), nullptr);
        QVERIFY(info.has_value());
        QCOMPARE(info->start, 4);
        QCOMPARE(info->end, 6);
        QCOMPARE(info->query, QStringLiteral("Al"));
        QCOMPARE(info->replaceEnd, -1);                   // nothing to consume
        QVERIFY(!s.onTrigger(1, QStringLiteral("no link"), nullptr).has_value());
    }

    void trigger_bailsOnClosedLink()
    {
        WikiLinkSuggest s(m_vault.get());
        QVERIFY(!s.onTrigger(9, QStringLiteral("[[done]] x"), nullptr).has_value());
    }

    void trigger_consumesTrailingBrackets()
    {
        WikiLinkSuggest s(m_vault.get());
        auto info = s.onTrigger(4, QStringLiteral("[[Al]]"), nullptr);
        QVERIFY(info.has_value());
        QCOMPARE(info->replaceEnd, 6);                    // consume the "]]"
    }

    void names_universeAndInsertText()
    {
        WikiLinkSuggest s(m_vault.get());
        s.setLinkResolver(&m_resolver);
        auto info = s.onTrigger(2, QStringLiteral("[["), nullptr);
        QVERIFY(info.has_value());
        const auto set = s.getSuggestions(*info);
        QCOMPARE(set.filter, QString());
        // At least the 3 A1 notes: Alpha, Beta (x2 — ambiguous basename).
        // ">=" not "==": Task 14 (A2) grows this fixture with more notes
        // and alias items in the same initTestCase.
        QVERIFY(set.items.size() >= 3);
        QString alphaInsert, betaInserts;
        for (const auto &it : set.items) {
            if (it.display == QStringLiteral("Alpha")) alphaInsert = it.insertText;
            if (it.display == QStringLiteral("Beta"))  betaInserts += it.insertText + QLatin1Char(';');
        }
        QCOMPARE(alphaInsert, QStringLiteral("Alpha]]"));               // unique → basename
        QVERIFY2(betaInserts.contains(QStringLiteral("Beta]]"))
                     && betaInserts.contains(QStringLiteral("sub/Beta]]")),
                 qPrintable(QStringLiteral("ambiguous basenames must insert paths: %1").arg(betaInserts)));
    }
};

QTEST_MAIN(WikiLinkSuggestTest)
#include "tst_wikilink_suggest.moc"
```

`tests/editor/tst_tag_suggest.cpp` — TagSuggest needs an `SQLiteIndex`; constructing a full index for a unit test is heavy, and `allTags()` is the only call. Check `SQLiteIndex` for in-memory open (`open(":memory:")` per SQLite convention — verify: `grep -n "open" libs/storage/include/corbomite/storage/SQLiteIndex.h`). If in-memory open + a tag-insertion path is not trivially available, test TagSuggest's `onTrigger` only (pure function) and leave `getSuggestions` covered by the controller integration test with a stub index — do NOT build test scaffolding heavier than the code under test:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "TagSuggest.h"
#include <QObject>
#include <QTest>

using namespace Corbomite;

class TagSuggestTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void trigger_afterHashAtLineStart()
    {
        TagSuggest s(nullptr);
        auto info = s.onTrigger(3, QStringLiteral("#pr"), nullptr);
        QVERIFY(info.has_value());
        QCOMPARE(info->start, 1);
        QCOMPARE(info->query, QStringLiteral("pr"));
    }
    void trigger_afterHashMidLineNeedsSpace()
    {
        TagSuggest s(nullptr);
        QVERIFY(s.onTrigger(5, QStringLiteral("x #pr"), nullptr).has_value());
        QVERIFY(!s.onTrigger(5, QStringLiteral("x#prq"), nullptr).has_value());
    }
    void nullIndex_returnsEmptyUniverse()
    {
        TagSuggest s(nullptr);
        EditorSuggestTriggerInfo ctx; ctx.start = 1; ctx.end = 3;
        ctx.query = QStringLiteral("pr");
        const auto set = s.getSuggestions(ctx);
        QVERIFY(set.items.isEmpty());
        QCOMPARE(set.filter, QStringLiteral("pr"));
    }
};

QTEST_MAIN(TagSuggestTest)
#include "tst_tag_suggest.moc"
```

Register both in `tests/editor/CMakeLists.txt` (mirror the `tst_note_editor_widget_ephemeral` block; `tst_wikilink_suggest` additionally links `Corbomite::Vault` and `Corbomite::Storage`):

```cmake
add_executable(tst_wikilink_suggest tst_wikilink_suggest.cpp)
target_include_directories(tst_wikilink_suggest PRIVATE ${CMAKE_SOURCE_DIR}/src/editor)
target_link_libraries(tst_wikilink_suggest PRIVATE
    Qt6::Test Qt6::Widgets CorbomiteApp Corbomite::Core Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_wikilink_suggest COMMAND tst_wikilink_suggest)
set_tests_properties(tst_wikilink_suggest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_tag_suggest tst_tag_suggest.cpp)
target_include_directories(tst_tag_suggest PRIVATE ${CMAKE_SOURCE_DIR}/src/editor)
target_link_libraries(tst_tag_suggest PRIVATE
    Qt6::Test Qt6::Widgets CorbomiteApp Corbomite::Core Corbomite::Storage)
add_test(NAME tst_tag_suggest COMMAND tst_tag_suggest)
set_tests_properties(tst_tag_suggest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

(If `WikiLinkSuggest.cpp`/`TagSuggest.cpp` are compiled into `CorbomiteApp`, linking `CorbomiteApp` suffices — verify with `grep -rn "WikiLinkSuggest.cpp" src/ --include=CMakeLists.txt`; adjust to link whatever target owns them.)

- [ ] **Step 2: Run to verify failure:** configure + build the two test targets; expected: compile errors (`setLinkResolver` missing, `replaceEnd` behavior absent).

```bash
cmake --preset dev >/dev/null && cmake --build --preset dev --target tst_wikilink_suggest tst_tag_suggest -j 10 2>&1 | tail -5
```

- [ ] **Step 3: Implement WikiLinkSuggest v2 (names mode).** `WikiLinkSuggest.h` becomes:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/EditorSuggest.h"

namespace Corbomite {

class Vault;
class LinkResolver;
class MetadataCache;

// Built-in suggester for `[[wiki-link]]` completion: note names (+ aliases,
// `#heading` and `#^block` sub-targets in later phases). Spec:
// docs/superpowers/specs/2026-06-11-completion-revival-design.md §8.
class WikiLinkSuggest : public EditorSuggest {
public:
    explicit WikiLinkSuggest(Vault *vault);

    std::optional<EditorSuggestTriggerInfo> onTrigger(int cursorPos,
                                                        const QString &lineText,
                                                        NoteDocument *file) override;
    EditorSuggestionSet getSuggestions(const EditorSuggestTriggerInfo &ctx) override;

    void setVault(Vault *vault) { m_vault = vault; }
    void setLinkResolver(LinkResolver *resolver) { m_resolver = resolver; }
    void setMetadataCache(MetadataCache *cache) { m_cache = cache; }

private:
    Vault *m_vault;
    LinkResolver *m_resolver = nullptr;
    MetadataCache *m_cache = nullptr;       // used from A2 (aliases/headings)
    QString m_sourcePath;                   // relativePath of last onTrigger's file;
                                            // dispatch always calls onTrigger
                                            // immediately before getSuggestions.
};

} // namespace Corbomite
```

`WikiLinkSuggest.cpp` — keep the existing `onTrigger` walk, add the source-path stash + `replaceEnd`, and replace `getSuggestions`/delete `selectSuggestion`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "WikiLinkSuggest.h"

#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"

namespace Corbomite {

WikiLinkSuggest::WikiLinkSuggest(Vault *vault)
    : m_vault(vault)
{
}

std::optional<EditorSuggestTriggerInfo>
WikiLinkSuggest::onTrigger(int cursorPos, const QString &lineText, NoteDocument *file)
{
    m_sourcePath = file ? file->relativePath() : QString();
    if (cursorPos < 0 || cursorPos > lineText.length()) return std::nullopt;
    int i = cursorPos - 1;
    while (i >= 1) {
        const QChar c = lineText.at(i);
        if (c == QLatin1Char(']')) return std::nullopt;
        if (c == QLatin1Char('[') && lineText.at(i - 1) == QLatin1Char('[')) {
            EditorSuggestTriggerInfo info;
            info.start = i + 1;
            info.end = cursorPos;
            info.query = lineText.mid(info.start, info.end - info.start);
            // Consume a pre-existing "]]" right after the cursor so accepting
            // a candidate doesn't produce "]]]]" (spec §8).
            if (lineText.mid(cursorPos, 2) == QLatin1String("]]"))
                info.replaceEnd = cursorPos + 2;
            return info;
        }
        --i;
    }
    return std::nullopt;
}

EditorSuggestionSet WikiLinkSuggest::getSuggestions(const EditorSuggestTriggerInfo &ctx)
{
    EditorSuggestionSet set;
    set.filter = ctx.query;
    if (!m_vault) return set;

    const auto files = m_vault->getMarkdownFiles();
    set.items.reserve(files.size());
    for (auto *tf : files) {
        if (!tf) continue;
        EditorSuggestItem item;
        item.display = tf->basename;
        // Shortest target that resolves uniquely: basename when unique
        // vault-wide, else the relative path (sans .md). LinkResolver's
        // name map is keyed by lowercased filename WITH extension
        // (TAbstractFile::name), not the extension-less basename.
        QString target = tf->basename;
        if (m_resolver && m_resolver->candidateCount(tf->name.toLower()) > 1) {
            target = tf->path;
            if (target.endsWith(QStringLiteral(".md"))) target.chop(3);
        }
        item.insertText = target + QStringLiteral("]]");
        item.detail = tf->path;
        set.items.append(item);
    }
    return set;
}

} // namespace Corbomite
```

- [ ] **Step 4: Implement TagSuggest v2.** In `TagSuggest.cpp` replace `getSuggestions` + delete `selectSuggestion` (header: swap the two old virtuals for the one new `getSuggestions` signature):

```cpp
EditorSuggestionSet TagSuggest::getSuggestions(const EditorSuggestTriggerInfo &ctx)
{
    EditorSuggestionSet set;
    set.filter = ctx.query;
    if (!m_index) return set;
    QStringList tags = m_index->allTags();
    set.items.reserve(tags.size());
    for (QString &t : tags) {
        if (t.startsWith(QLatin1Char('#'))) t.remove(0, 1);
        set.items.append({t, t, {}});
    }
    return set;
}
```

(Drop the now-unused `FuzzyMatcher` includes from both suggester .cpp files — the popup owns filtering.)

- [ ] **Step 5: Build + run:**

```bash
cmake --build --preset dev --target tst_wikilink_suggest tst_tag_suggest -j 10 && \
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R "tst_wikilink_suggest|tst_tag_suggest|tst_editorsuggest" --output-on-failure 2>&1 | tail -4 && cd ..
```
Expected: 3/3 PASS.

- [ ] **Step 6: Commit:**

```bash
git add src/editor/WikiLinkSuggest.h src/editor/WikiLinkSuggest.cpp src/editor/TagSuggest.h src/editor/TagSuggest.cpp \
        tests/editor/tst_wikilink_suggest.cpp tests/editor/tst_tag_suggest.cpp tests/editor/CMakeLists.txt
git commit -m "feat(editor): WikiLink/Tag suggesters on interface v2 — universe+filter split, replaceEnd, path disambiguation

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task 8: LineResolve coordinate bridge

**Files:**
- Create: `src/editor/LineResolve.h`, `src/editor/LineResolve.cpp`
- Create: `tests/editor/tst_line_resolve.cpp`
- Modify: the CMake target that owns `src/editor/` sources (same one found in Task 7), `tests/editor/CMakeLists.txt`

- [ ] **Step 1: Write the failing test** `tests/editor/tst_line_resolve.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// LineResolve — contract-v2 flat visual line ⟷ (block, offsets, lineText).
// Spec §5. The line space matches MarkdownView::cursorPosition(): each
// block contributes 1 + count('\n') lines.
#include "LineResolve.h"
#include <markoff/core/MarkoffDocument.h>
#include <QObject>
#include <QTest>

using namespace Corbomite;

class LineResolveTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void singleLineBlocks()
    {
        Markoff::MarkoffDocument doc(1);
        doc.loadFromMarkdown(QByteArrayLiteral("Alpha\n\nBeta\n\nGamma\n"));
        auto r1 = LineResolve::resolveLine(&doc, 1);
        QVERIFY(r1.has_value());
        QCOMPARE(r1->blockRow, 0);
        QCOMPARE(r1->lineStartCharInBlock, 0);
        QCOMPARE(r1->lineText, QStringLiteral("Alpha"));
        auto r3 = LineResolve::resolveLine(&doc, 3);
        QVERIFY(r3.has_value());
        QCOMPARE(r3->blockRow, 2);
        QCOMPARE(r3->lineText, QStringLiteral("Gamma"));
    }

    void multiLineBlock()
    {
        // A code block keeps internal newlines in its buffer.
        Markoff::MarkoffDocument doc(1);
        doc.loadFromMarkdown(QByteArrayLiteral("Intro\n\n```\nline a\nline b\n```\n"));
        // Find which visual line "line b" is by walking what cursorPosition
        // space defines: block 0 = "Intro" (1 line), block 1 = the code
        // block buffer. Resolve successive lines and assert one of them is
        // exactly "line b" with a nonzero lineStartCharInBlock.
        bool found = false;
        for (int line = 1; line <= 8; ++line) {
            auto r = LineResolve::resolveLine(&doc, line);
            if (!r) break;
            if (r->lineText == QStringLiteral("line b")) {
                QVERIFY(r->lineStartCharInBlock > 0);
                found = true;
            }
        }
        QVERIFY2(found, "multi-line block's inner line not resolvable");
    }

    void outOfRangeAndNull()
    {
        Markoff::MarkoffDocument doc(1);
        doc.loadFromMarkdown(QByteArrayLiteral("One\n"));
        QVERIFY(!LineResolve::resolveLine(&doc, 0).has_value());
        QVERIFY(!LineResolve::resolveLine(&doc, 99).has_value());
        QVERIFY(!LineResolve::resolveLine(nullptr, 1).has_value());
    }

    void byteOffsetMultibyte()
    {
        // "héllo 日本" — é is 2 UTF-8 bytes, 日/本 are 3 each.
        const QString s = QString::fromUtf8("h\xC3\xA9llo \xE6\x97\xA5\xE6\x9C\xAC");
        QCOMPARE(LineResolve::byteOffsetForChar(s, 0), 0u);
        QCOMPARE(LineResolve::byteOffsetForChar(s, 1), 1u);   // before é
        QCOMPARE(LineResolve::byteOffsetForChar(s, 2), 3u);   // after é
        QCOMPARE(LineResolve::byteOffsetForChar(s, 7), 8u);   // after the space
        QCOMPARE(LineResolve::byteOffsetForChar(s, 8), 11u);  // after 日
        QCOMPARE(LineResolve::byteOffsetForChar(s, 99), uint32_t(s.toUtf8().size()));  // clamps
    }
};

QTEST_MAIN(LineResolveTest)
#include "tst_line_resolve.moc"
```

CMake registration (same pattern; links `CorbomiteApp` + `markoff_core`):

```cmake
add_executable(tst_line_resolve tst_line_resolve.cpp)
target_include_directories(tst_line_resolve PRIVATE ${CMAKE_SOURCE_DIR}/src/editor)
target_link_libraries(tst_line_resolve PRIVATE Qt6::Test Qt6::Widgets CorbomiteApp markoff_core)
add_test(NAME tst_line_resolve COMMAND tst_line_resolve)
set_tests_properties(tst_line_resolve PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: Verify compile failure** (`LineResolve.h` doesn't exist):

```bash
cmake --preset dev >/dev/null && cmake --build --preset dev --target tst_line_resolve -j 10 2>&1 | tail -3
```

- [ ] **Step 3: Implement.** `src/editor/LineResolve.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>
#include <QString>
#include <markoff/core/MarkoffDocument.h>

namespace Corbomite::LineResolve {

/// One contract-v2 flat visual line resolved against the document.
/// The line space is MarkdownView::cursorPosition()'s (normative,
/// markoff contract-v2 spec §3): each block contributes
/// 1 + count('\n' in blockText) lines, 1-based.
struct ResolvedLine {
    Markoff::BlockId blockId;
    int blockRow = -1;             ///< index in iterateBlocks() order
    int lineStartCharInBlock = 0;  ///< UTF-16 offset of line start within blockText-as-QString
    QString lineText;              ///< the line, without trailing '\n'
};

/// nullopt when doc is null, line < 1, or past the last line.
std::optional<ResolvedLine> resolveLine(const Markoff::MarkoffDocument *doc, int line);

/// UTF-16 char offset within a block's text (as QString) → UTF-8 byte
/// offset into the block buffer. Clamps charPos to [0, length].
uint32_t byteOffsetForChar(const QString &blockText, int charPos);

} // namespace Corbomite::LineResolve
```

`src/editor/LineResolve.cpp` (the walk mirrors `EditorWidget`'s `toCursorPos`/`fromCursorPos` in markoff-live — cite it):

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "LineResolve.h"

namespace Corbomite::LineResolve {

// Mirrors the normative flat-line walk in markoff-live's
// EditorWidget.cpp (toCursorPos/fromCursorPos) — keep in lockstep with
// contract-v2 spec §3 if that ever changes.
std::optional<ResolvedLine> resolveLine(const Markoff::MarkoffDocument *doc, int line)
{
    if (!doc || line < 1) return std::nullopt;
    const auto ids = doc->iterateBlocks();
    int cur = 1;
    for (int row = 0; row < int(ids.size()); ++row) {
        const QString text = QString::fromUtf8(doc->blockText(ids[size_t(row)]));
        const int span = 1 + int(text.count(QLatin1Char('\n')));
        if (line < cur + span) {
            int pos = 0;
            for (int i = 0; i < line - cur; ++i)
                pos = int(text.indexOf(QLatin1Char('\n'), pos)) + 1;
            const qsizetype nl = text.indexOf(QLatin1Char('\n'), pos);
            ResolvedLine out;
            out.blockId = ids[size_t(row)];
            out.blockRow = row;
            out.lineStartCharInBlock = pos;
            out.lineText = (nl < 0) ? text.mid(pos) : text.mid(pos, int(nl) - pos);
            return out;
        }
        cur += span;
    }
    return std::nullopt;
}

uint32_t byteOffsetForChar(const QString &blockText, int charPos)
{
    const int clamped = qBound(0, charPos, int(blockText.length()));
    return uint32_t(QStringView(blockText).left(clamped).toUtf8().size());
}

} // namespace Corbomite::LineResolve
```

Add `LineResolve.cpp` to the editor sources list in the owning CMake target.

- [ ] **Step 4: Build + run:**

```bash
cmake --build --preset dev --target tst_line_resolve -j 10 && \
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_line_resolve --output-on-failure 2>&1 | tail -3 && cd ..
```
Expected: PASS. (If `multiLineBlock` finds no "line b", inspect what `loadFromMarkdown` does to fenced blocks with `doc.blockText` prints — the test intentionally discovers rather than assumes the code-block buffer shape; adjust the *fixture markdown*, never the resolver, until a multi-`\n` block is exercised.)

- [ ] **Step 5: Commit:**

```bash
git add src/editor/LineResolve.h src/editor/LineResolve.cpp tests/editor/tst_line_resolve.cpp tests/editor/CMakeLists.txt src/editor/CMakeLists.txt
git commit -m "feat(editor): LineResolve — contract-v2 visual line ⟷ block/offset bridge

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

(Substitute the actual CMake file path that lists editor sources if it differs.)

### Task 9: CompletionController — core refresh/dismiss

**Files:**
- Create: `src/editor/CompletionController.h`, `src/editor/CompletionController.cpp`
- Create: `tests/editor/tst_completion_controller.cpp`
- Modify: editor-sources CMake target, `tests/editor/CMakeLists.txt`

- [ ] **Step 1: Write the controller header** (full file; later tasks fill the .cpp incrementally):

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointer>
#include <QObject>
#include <QRect>

#include <markoff/core/MarkdownView.h>

class QStandardItemModel;

namespace Corbomite {

class CompletionPopup;
class EditorSuggestManager;
class NoteDocument;

/// Leaf-agnostic completion driver (spec
/// docs/superpowers/specs/2026-06-11-completion-revival-design.md §6).
/// Owns the popup + trigger session; reads the active leaf ONLY through
/// the Markoff::MarkdownView base (cursorPosition / caretRect /
/// hasEditing) and mutates ONLY the shared MarkoffDocument
/// (applyBlockEdit), so it works identically for every leaf.
///
/// Reactive model: refresh() recomputes the entire trigger state from the
/// current snapshot on every (coalesced) document/cursor change — no
/// stored trigger position, no per-key state machine.
class CompletionController : public QObject {
    Q_OBJECT
public:
    explicit CompletionController(QObject *parent = nullptr);
    ~CompletionController() override;

    void setManager(EditorSuggestManager *manager);
    void setLeaf(Markoff::MarkdownView *leaf);      // every mode switch; dismisses
    void setNoteDocument(NoteDocument *doc);        // every document swap; dismisses

    bool isActive() const;                          // popup visible
    CompletionPopup *popup() const { return m_popup; }   // tests

public Q_SLOTS:
    void dismiss();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;  // scoped app filter

private:
    void scheduleRefresh();
    void refresh();
    void ensurePopup();
    void positionPopup(const QRect &caretRect);
    void accept(const QString &display, const QString &insertText);

    EditorSuggestManager *m_manager = nullptr;
    Markoff::MarkdownView *m_leaf = nullptr;
    NoteDocument *m_doc = nullptr;                  // nulled via destroyed()
    CompletionPopup *m_popup = nullptr;
    QStandardItemModel *m_model = nullptr;
    bool m_refreshPending = false;
    QMetaObject::Connection m_docChangedCon;
    QMetaObject::Connection m_docDestroyedCon;
    QMetaObject::Connection m_leafCursorCon;
};

} // namespace Corbomite
```

- [ ] **Step 2: Write the failing tests.** `tests/editor/tst_completion_controller.cpp` (full file; the accept/undo slots referencing Task 10 behavior are included now and will fail until Task 10 — implement Task 9's slots first if running selectively, but the file is written once):

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// CompletionController — spec §6. Headless: a FakeLeaf MarkdownView
// supplies caretRect/cursorPosition; a real NoteDocument supplies the
// text + edit path; a stub '@' suggester isolates controller logic from
// the real suggesters (covered by their own tests).
#include "CompletionController.h"
#include "CompletionPopup.h"
#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/MarkdownView.h>
#include <markoff/core/MarkoffDocument.h>

#include <QObject>
#include <QTest>

using namespace Corbomite;

namespace {

class FakeLeaf : public Markoff::MarkdownView {
public:
    QRect caretRect() const override { return m_caret; }
    bool hasCursor()  const override { return true; }
    bool hasEditing() const override { return !isReadOnly(); }
    Markoff::CursorPos cursorPosition() const override { return m_pos; }
    void setCursorPosition(Markoff::CursorPos p) override
    {
        m_pos = p;
        Q_EMIT cursorPositionChanged(p.line, p.column);
    }
    QRect m_caret{20, 20, 2, 14};
    Markoff::CursorPos m_pos{1, 1};
};

class AtSuggest : public EditorSuggest {
public:
    std::optional<EditorSuggestTriggerInfo> onTrigger(int cursorPos,
                                                        const QString &lineText,
                                                        NoteDocument *) override
    {
        int i = cursorPos - 1;
        while (i >= 0 && !lineText.at(i).isSpace()) {
            if (lineText.at(i) == QLatin1Char('@')) {
                EditorSuggestTriggerInfo info;
                info.start = i + 1;
                info.end = cursorPos;
                info.query = lineText.mid(info.start, info.end - info.start);
                return info;
            }
            --i;
        }
        return std::nullopt;
    }
    EditorSuggestionSet getSuggestions(const EditorSuggestTriggerInfo &ctx) override
    {
        EditorSuggestionSet set;
        set.filter = ctx.query;
        set.items.append({QStringLiteral("apple"),  QStringLiteral("apple!"),  {}});
        set.items.append({QStringLiteral("banana"), QStringLiteral("banana!"), {}});
        return set;
    }
};

struct Rig {
    EditorSuggestManager manager;
    AtSuggest suggest;
    FakeLeaf leaf;
    std::unique_ptr<NoteDocument> doc;
    CompletionController ctl;

    explicit Rig(const QString &markdown)
    {
        manager.registerSuggest(&suggest);
        doc = std::make_unique<NoteDocument>(QStringLiteral("/tmp/v"), QStringLiteral("n.md"));
        doc->setMarkdown(markdown);
        leaf.setDocument(doc->markoff());
        leaf.resize(400, 300);
        leaf.show();
        ctl.setManager(&manager);
        ctl.setLeaf(&leaf);
        ctl.setNoteDocument(doc.get());
    }

    // Simulate the user having typed up to (line, col) — document already
    // contains the text; we place the fake caret and let the controller's
    // cursor-change signal drive the refresh.
    void placeCursor(int line, int col) { leaf.setCursorPosition({line, col}); }
};

} // namespace

class CompletionControllerTest : public QObject {
    Q_OBJECT
private Q_SLOTS:

    void triggerShowsPopup_universeListed()
    {
        // Query "ap" so the fuzzy proxy keeps ONLY apple — "a" alone would
        // subsequence-match banana too.
        Rig rig(QStringLiteral("hello @ap"));
        rig.placeCursor(1, 10);                      // after "@ap"
        QTRY_VERIFY(rig.ctl.isActive());
        QVERIFY(rig.ctl.popup());
        QCOMPARE(rig.ctl.popup()->visibleRowCount(), 1);   // fuzzy 'ap' → apple only
    }

    void noTrigger_dismisses()
    {
        Rig rig(QStringLiteral("hello @a"));
        rig.placeCursor(1, 9);
        QTRY_VERIFY(rig.ctl.isActive());
        rig.placeCursor(1, 3);                       // cursor left the trigger
        QTRY_VERIFY(!rig.ctl.isActive());
    }

    void readOnly_neverTriggers()
    {
        Rig rig(QStringLiteral("hello @a"));
        rig.leaf.setReadOnly(true);
        rig.placeCursor(1, 9);
        QTest::qWait(30);
        QVERIFY(!rig.ctl.isActive());
    }

    void invalidCaretRect_suppresses()
    {
        Rig rig(QStringLiteral("hello @a"));
        rig.leaf.m_caret = QRect();                  // invalid
        rig.placeCursor(1, 9);
        QTest::qWait(30);
        QVERIFY(!rig.ctl.isActive());
    }

    void docDestroyed_whilePopupOpen_dismissesWithoutCrash()
    {
        Rig rig(QStringLiteral("hello @a"));
        rig.placeCursor(1, 9);
        QTRY_VERIFY(rig.ctl.isActive());
        rig.doc.reset();                             // free the document
        QTRY_VERIFY(!rig.ctl.isActive());
        rig.placeCursor(1, 2);                       // must not crash
        QTest::qWait(20);
    }

    // ---- Task 10 behavior (accept path) ----

    void accept_replacesRange_movesCaret_isOneUndoStep()
    {
        // "ap" → only apple visible, so selectNext deterministically
        // highlights it (with "a" alone, banana also fuzzy-matches and the
        // ranking order would be load-bearing).
        Rig rig(QStringLiteral("hello @ap"));
        rig.placeCursor(1, 10);
        QTRY_VERIFY(rig.ctl.isActive());
        rig.ctl.popup()->selectNext();               // highlight the only visible row
        QVERIFY(rig.ctl.popup()->acceptCurrent());
        // "@ap" → "@apple!" : replacement of [7,9) with "apple!"
        QTRY_COMPARE(QString::fromUtf8(rig.doc->markoff()->serializeForSave()),
                     QStringLiteral("hello @apple!\n"));
        QCOMPARE(rig.leaf.m_pos.column, 14);         // start(7)+len(6)+1 → after "apple!"
        QVERIFY(!rig.ctl.isActive());
        rig.doc->markoff()->undoD2();
        QTRY_COMPARE(QString::fromUtf8(rig.doc->markoff()->serializeForSave()),
                     QStringLiteral("hello @ap\n"));
    }

    void acceptWithStaleTrigger_abortsSilently()
    {
        Rig rig(QStringLiteral("hello @a"));
        rig.placeCursor(1, 9);
        QTRY_VERIFY(rig.ctl.isActive());
        auto *popup = rig.ctl.popup();
        popup->selectNext();
        rig.placeCursor(1, 3);                       // break the trigger…
        QTRY_VERIFY(!rig.ctl.isActive());            // …popup dismissed; nothing to accept
        QCOMPARE(QString::fromUtf8(rig.doc->markoff()->serializeForSave()),
                 QStringLiteral("hello @a\n"));
        Q_UNUSED(popup)
    }
};

QTEST_MAIN(CompletionControllerTest)
#include "tst_completion_controller.moc"
```

CMake:

```cmake
add_executable(tst_completion_controller tst_completion_controller.cpp)
target_include_directories(tst_completion_controller PRIVATE ${CMAKE_SOURCE_DIR}/src/editor)
target_link_libraries(tst_completion_controller PRIVATE
    Qt6::Test Qt6::Widgets CorbomiteApp Corbomite::Core markoff_core)
add_test(NAME tst_completion_controller COMMAND tst_completion_controller)
set_tests_properties(tst_completion_controller PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

NOTE on `accept_…` expected column: `"hello @a"` — trigger start is char 7 (line-relative, after `@` at 6), insertText `apple!` is 6 chars → expected caret column = 7 + 6 + 1 = **14**. The serialized form carries Markoff's canonical trailing newline (B1) — assertions include it.

- [ ] **Step 3: Verify compile failure**, then **Step 4: implement the .cpp** (everything except `accept`, which Task 10 fills — stub it as `{ Q_UNUSED(display) Q_UNUSED(insertText) dismiss(); }` for now):

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CompletionController.h"

#include "CompletionPopup.h"
#include "LineResolve.h"
#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/MarkoffDocument.h>

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QStandardItemModel>
#include <QTimer>

namespace Corbomite {

CompletionController::CompletionController(QObject *parent)
    : QObject(parent)
    , m_model(new QStandardItemModel(this))
{
}

CompletionController::~CompletionController()
{
    dismiss();
}

void CompletionController::setManager(EditorSuggestManager *manager)
{
    m_manager = manager;
}

void CompletionController::setLeaf(Markoff::MarkdownView *leaf)
{
    if (m_leaf == leaf) return;
    dismiss();
    if (m_leafCursorCon) QObject::disconnect(m_leafCursorCon);
    m_leaf = leaf;
    if (m_leaf) {
        m_leafCursorCon = connect(m_leaf, &Markoff::MarkdownView::cursorPositionChanged,
                                  this, [this](int, int) { scheduleRefresh(); });
    }
}

void CompletionController::setNoteDocument(NoteDocument *doc)
{
    if (m_doc == doc) return;
    dismiss();
    if (m_docChangedCon)   QObject::disconnect(m_docChangedCon);
    if (m_docDestroyedCon) QObject::disconnect(m_docDestroyedCon);
    m_doc = doc;
    if (m_doc && m_doc->markoff()) {
        m_docChangedCon = connect(m_doc->markoff(), &Markoff::MarkoffDocument::d2DocumentChanged,
                                  this, [this] { scheduleRefresh(); });
        // Retire-on-destroy — same lesson as the 2026-06-10 vault-teardown
        // UAF: never let a raw document pointer outlive its target.
        m_docDestroyedCon = connect(m_doc->markoff(), &QObject::destroyed, this, [this] {
            m_doc = nullptr;
            dismiss();
        });
    }
}

bool CompletionController::isActive() const
{
    return m_popup && m_popup->isVisible();
}

void CompletionController::dismiss()
{
    if (!m_popup) return;
    CompletionPopup *p = m_popup;
    m_popup = nullptr;                  // null first: hideEvent→dismissed→dismiss() recursion guard
    qApp->removeEventFilter(this);
    p->hide();
    p->deleteLater();
}

void CompletionController::scheduleRefresh()
{
    if (m_refreshPending) return;
    m_refreshPending = true;
    // Coalesce: a keystroke fires both d2DocumentChanged and
    // cursorPositionChanged — one refresh per event-loop spin.
    QTimer::singleShot(0, this, &CompletionController::refresh);
}

void CompletionController::refresh()
{
    m_refreshPending = false;
    if (!m_manager || !m_leaf || !m_doc || !m_doc->markoff() || !m_leaf->hasEditing()) {
        dismiss();
        return;
    }
    const QRect caret = m_leaf->caretRect();
    if (!caret.isValid()) {              // can't anchor ⇒ never a misplaced popup
        dismiss();
        return;
    }
    const Markoff::CursorPos pos = m_leaf->cursorPosition();
    const auto line = LineResolve::resolveLine(m_doc->markoff(), pos.line);
    if (!line) { dismiss(); return; }
    auto res = m_manager->dispatch(pos.column - 1, line->lineText, m_doc);
    if (!res) { dismiss(); return; }
    const EditorSuggestionSet set = res->suggester->getSuggestions(res->info);
    if (set.items.isEmpty()) { dismiss(); return; }

    ensurePopup();
    m_model->clear();
    for (const auto &it : set.items) {
        auto *item = new QStandardItem(it.display);
        item->setData(it.insertText, Qt::UserRole + 1);
        item->setData(it.detail, Qt::UserRole + 2);
        item->setEditable(false);
        m_model->appendRow(item);
    }
    m_popup->setFilterText(set.filter);
    if (m_popup->visibleRowCount() == 0) { dismiss(); return; }
    positionPopup(caret);
    m_popup->show();
}

void CompletionController::ensurePopup()
{
    if (m_popup) return;
    m_popup = new CompletionPopup(m_model, m_leaf);
    connect(m_popup, &CompletionPopup::itemSelected, this, &CompletionController::accept);
    connect(m_popup, &CompletionPopup::dismissed, this, &CompletionController::dismiss);
    // Scoped key interception: only while a popup lives. Installed AFTER
    // ScopeManager's app-wide filter, so ours runs FIRST (Qt runs
    // later-installed application filters first) — completion keys win
    // exactly while the popup is up.
    qApp->installEventFilter(this);
}

void CompletionController::positionPopup(const QRect &caret)
{
    QPoint anchor = caret.bottomLeft() + QPoint(0, 2);
    const QSize hint = m_popup->sizeHint();
    if (anchor.y() + hint.height() > m_leaf->height()
        && caret.top() - hint.height() - 2 >= 0) {
        anchor = caret.topLeft() - QPoint(0, hint.height() + 2);  // flip above
    }
    anchor.setX(qBound(0, anchor.x(), qMax(0, m_leaf->width() - hint.width())));
    m_popup->move(anchor);
}

bool CompletionController::eventFilter(QObject *obj, QEvent *event)
{
    if (!m_popup) return false;
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        switch (ke->key()) {
        case Qt::Key_Down:   m_popup->selectNext();     return true;
        case Qt::Key_Up:     m_popup->selectPrevious(); return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
            if (m_popup->acceptCurrent()) return true;
            break;
        case Qt::Key_Escape: dismiss();                 return true;
        default: break;
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        // Click anywhere outside the popup or the leaf ⇒ dismiss (clicks
        // inside the leaf re-trigger refresh via cursorPositionChanged).
        auto *w = qobject_cast<QWidget *>(obj);
        if (w && m_leaf && !m_popup->isAncestorOf(w) && w != m_popup
            && !m_leaf->isAncestorOf(w) && w != m_leaf) {
            dismiss();
        }
    } else if (event->type() == QEvent::ApplicationDeactivate) {
        dismiss();
    }
    return false;
}

void CompletionController::accept(const QString &display, const QString &insertText)
{
    Q_UNUSED(display)
    Q_UNUSED(insertText)
    dismiss();   // Task 10 implements the real accept path.
}

} // namespace Corbomite
```

Add `CompletionController.cpp` to the editor sources CMake list.

- [ ] **Step 5: Build; run the Task 9 slots** (the two Task 10 slots fail — expected):

```bash
cmake --build --preset dev --target tst_completion_controller -j 10 && \
cd build-dev && QT_QPA_PLATFORM=offscreen ./bin/tst_completion_controller 2>&1 | grep -E "PASS|FAIL|Totals" && cd ..
```
Expected: `triggerShowsPopup_universeListed`, `noTrigger_dismisses`, `readOnly_neverTriggers`, `invalidCaretRect_suppresses`, `docDestroyed_…` PASS; `accept_…` and `acceptWithStaleTrigger_…` FAIL.

- [ ] **Step 6: Commit:**

```bash
git add src/editor/CompletionController.h src/editor/CompletionController.cpp \
        tests/editor/tst_completion_controller.cpp tests/editor/CMakeLists.txt src/editor/CMakeLists.txt
git commit -m "feat(editor): CompletionController core — reactive trigger/dismiss over the MarkdownView base

Accept path lands next (its 2 test slots are committed red by design).

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task 10: CompletionController — accept path

**Files:**
- Modify: `src/editor/CompletionController.cpp` (the `accept` stub)

- [ ] **Step 1: Implement `accept`:**

```cpp
void CompletionController::accept(const QString &display, const QString &insertText)
{
    Q_UNUSED(display)
    // Re-resolve from the live snapshot — the popup's view of the world may
    // be one coalesced refresh stale. If the trigger no longer holds, abort
    // silently (spec §6): never insert against a guessed range.
    if (!m_manager || !m_leaf || !m_doc || !m_doc->markoff() || insertText.isEmpty()) {
        dismiss();
        return;
    }
    Markoff::MarkoffDocument *mdoc = m_doc->markoff();
    const Markoff::CursorPos pos = m_leaf->cursorPosition();
    const auto line = LineResolve::resolveLine(mdoc, pos.line);
    if (!line) { dismiss(); return; }
    auto res = m_manager->dispatch(pos.column - 1, line->lineText, m_doc);
    if (!res) { dismiss(); return; }
    const auto &info = res->info;
    const int replaceEnd = (info.replaceEnd >= info.end) ? info.replaceEnd : info.end;

    const QString blockStr = QString::fromUtf8(mdoc->blockText(line->blockId));
    const uint32_t b0 = LineResolve::byteOffsetForChar(
        blockStr, line->lineStartCharInBlock + info.start);
    const uint32_t b1 = LineResolve::byteOffsetForChar(
        blockStr, line->lineStartCharInBlock + replaceEnd);

    Markoff::BlockEdit edit;
    edit.blockId = line->blockId;
    edit.withinBlockByteOffset = b0;
    edit.removedBytes = b1 - b0;
    edit.insertedUtf8 = insertText.toUtf8();
    mdoc->applyBlockEdit(edit);          // undo-integrated; propagates to all leaves

    // Deterministic post-insert caret — never rely on a leaf's own
    // post-edit cursor behavior (spec §5).
    m_leaf->setCursorPosition({pos.line, info.start + int(insertText.length()) + 1});
    dismiss();
}
```

Add `#include <markoff/core/BlockEdit.h>` if `MarkoffDocument.h` doesn't already pull it (check; it does declare `applyBlockEdit(const Markoff::BlockEdit&)` so the type must be visible — include explicitly anyway).

- [ ] **Step 2: Run the full controller suite:**

```bash
cmake --build --preset dev --target tst_completion_controller -j 10 && \
cd build-dev && QT_QPA_PLATFORM=offscreen ./bin/tst_completion_controller 2>&1 | tail -3 && cd ..
```
Expected: all 7 slots PASS. If `accept_…` fails on the serialized string, print both sides — off-by-one in the byte math is the likely culprit; verify against the test's worked example (`start=7`, `@` at index 6) before touching the resolver.

- [ ] **Step 3: Falsifiability probe:** temporarily change `edit.removedBytes = b1 - b0;` to `edit.removedBytes = 0;` → `accept_…` must FAIL (produces "appleа" remnants). Restore; re-run; PASS.

- [ ] **Step 4: Commit:**

```bash
git add src/editor/CompletionController.cpp
git commit -m "feat(editor): completion accept — applyBlockEdit replacement + deterministic caret, one undo step

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task 11: Keyboard navigation through the scoped app filter

**Files:**
- Test: `tests/editor/tst_completion_controller.cpp` (new slots)

- [ ] **Step 1: Add the failing test slots:**

```cpp
    void keys_navigateAndAcceptViaAppFilter()
    {
        Rig rig(QStringLiteral("hello @"));
        rig.placeCursor(1, 8);                       // right after '@' (col = len+1)
        QTRY_VERIFY(rig.ctl.isActive());
        QCOMPARE(rig.ctl.popup()->visibleRowCount(), 2);   // empty query → both

        // Keys are sent to the LEAF (the focused editor in production);
        // the controller's app-level filter must intercept them.
        QTest::keyClick(&rig.leaf, Qt::Key_Down);
        QTest::keyClick(&rig.leaf, Qt::Key_Down);    // second row: banana
        QTest::keyClick(&rig.leaf, Qt::Key_Return);
        QTRY_COMPARE(QString::fromUtf8(rig.doc->markoff()->serializeForSave()),
                     QStringLiteral("hello @banana!\n"));
        QVERIFY(!rig.ctl.isActive());
    }

    void escape_dismissesWithoutEdit()
    {
        Rig rig(QStringLiteral("hello @"));
        rig.placeCursor(1, 8);
        QTRY_VERIFY(rig.ctl.isActive());
        QTest::keyClick(&rig.leaf, Qt::Key_Escape);
        QTRY_VERIFY(!rig.ctl.isActive());
        QCOMPARE(QString::fromUtf8(rig.doc->markoff()->serializeForSave()),
                 QStringLiteral("hello @\n"));
    }
```

- [ ] **Step 2: Run** — both should already PASS against the Task 9/10 implementation (the filter exists). If `keys_…` fails because the popup auto-selects row 0 on show, count the `Key_Down`s accordingly (read `CompletionPopup::showEvent` — adjust the number of key presses, not the controller).

```bash
cmake --build --preset dev --target tst_completion_controller -j 10 && \
cd build-dev && QT_QPA_PLATFORM=offscreen ./bin/tst_completion_controller 2>&1 | tail -3 && cd ..
```
Expected: 9/9 PASS.

- [ ] **Step 3: Commit:**

```bash
git add tests/editor/tst_completion_controller.cpp
git commit -m "test(editor): completion keyboard navigation via the scoped app filter

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task 12: Wire into NoteEditorWidget + MainWindow; delete the stubs

**Files:**
- Modify: `src/editor/NoteEditorWidget.h`, `src/editor/NoteEditorWidget.cpp`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: NoteEditorWidget.h.** Remove the declarations: `onTextChanged()`, `maybeActivateSuggester()`, `dismissCompletion()`, `onCompletionAccepted(...)`, `positionCompletionPopup()`, `updateCompletionFilter()`, `absoluteCursorPos()`, `currentLineText()`; remove members `m_completionPopup`, `m_activeSuggester`, `m_completionTriggerPos`; remove the `class CompletionPopup;` forward decl; add `class CompletionController;` forward decl and member `CompletionController *m_completion = nullptr;`. Keep `m_suggestManager` (still the conduit) — actually it can go too: `setEditorSuggestManager` forwards straight to the controller. Remove it.

- [ ] **Step 2: NoteEditorWidget.cpp.**
  - Add `#include "CompletionController.h"`; drop `#include "CompletionPopup.h"` if present.
  - Constructor (after `wireLeaf(m_editor);`):

```cpp
    m_completion = new CompletionController(this);
    m_completion->setLeaf(activeLeaf());
```

  - `setEditorSuggestManager` becomes:

```cpp
void NoteEditorWidget::setEditorSuggestManager(EditorSuggestManager *manager)
{
    m_suggestManager = manager;   // DELETE this line if the member was removed
    m_completion->setManager(manager);
}
```
(Remove the `m_suggestManager` member per Step 1 and keep only the forward.)
  - `setNoteDocument`: after the `m_doc = doc;` assignment block completes (end of function), add `m_completion->setNoteDocument(m_doc);`.
  - `setViewMode`: after step 4 (the attach block, before the find-bar reattach), add `m_completion->setLeaf(activeLeaf());`.
  - Delete the bodies + definitions of all eight stub methods and `onTextChanged`.
  - In `eventFilter`, delete the completion branches (the `FocusOut && m_completionPopup` block and the whole `KeyPress && m_completionPopup` switch). If nothing else remains in `eventFilter` beyond `return QWidget::eventFilter(obj, event);`, delete the override entirely plus the `installEventFilter(this)` call in the ctor.
  - Delete `dismissCompletion()` calls anywhere else in the file (grep).

- [ ] **Step 3: MainWindow.cpp** — in `onVaultOpened`, right after `if (m_wikiSuggest) m_wikiSuggest->setVault(m_vaultObj);` (≈line 2233), add:

```cpp
    if (m_wikiSuggest) {
        m_wikiSuggest->setLinkResolver(m_linkResolver);
        // setMetadataCache arrives in Phase A2 (aliases/headings).
    }
```

In `onVaultClosed`, next to `m_wikiSuggest->setVault(nullptr);` (≈line 2533), add `m_wikiSuggest->setLinkResolver(nullptr);`. Add `#include "WikiLinkSuggest.h"` if `MainWindow.cpp` doesn't already include it (it constructs `WikiLinkSuggest`, so it does).

- [ ] **Step 4: Build everything + full suite:**

```bash
cmake --build --preset dev -j 10 2>&1 | tail -2
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10 2>&1 | tail -4 && cd ..
```
Expected: build clean; **all tests pass** (260 + the new ones).

- [ ] **Step 5: Commit:**

```bash
git add src/editor/NoteEditorWidget.h src/editor/NoteEditorWidget.cpp src/app/MainWindow.cpp
git commit -m "feat(editor): wire CompletionController into NoteEditorWidget; delete port-era completion stubs

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task 13: Live-leaf integration test + A1 docs + push

**Files:**
- Create: `tests/editor/tst_note_editor_widget_completion.cpp`
- Modify: `tests/editor/CMakeLists.txt`, `docs/PARITY-MATRIX.md`, `docs/punch-list.md`

- [ ] **Step 1: Write the integration test** — real `NoteEditorWidget`, Live leaf, real `caretRect()`; a stub suggester keeps vault scaffolding out:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// Integration: CompletionController against the REAL Live leaf — real QML
// caretRect, real document propagation. Drives the document + base-contract
// cursor like tst_note_editor_widget_ephemeral (keyboard-level QML typing is
// covered upstream by markoff's harness; not re-tested here).
#include "NoteEditorWidget.h"
#include "CompletionController.h"
#include "CompletionPopup.h"
#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/MarkdownView.h>
#include <markoff/core/MarkoffDocument.h>

#include <QObject>
#include <QTest>

using namespace Corbomite;

namespace {
class AtSuggest : public EditorSuggest {
public:
    std::optional<EditorSuggestTriggerInfo> onTrigger(int cursorPos,
                                                        const QString &lineText,
                                                        NoteDocument *) override
    {
        int i = cursorPos - 1;
        while (i >= 0 && !lineText.at(i).isSpace()) {
            if (lineText.at(i) == QLatin1Char('@')) {
                EditorSuggestTriggerInfo info;
                info.start = i + 1;
                info.end = cursorPos;
                info.query = lineText.mid(info.start, info.end - info.start);
                return info;
            }
            --i;
        }
        return std::nullopt;
    }
    EditorSuggestionSet getSuggestions(const EditorSuggestTriggerInfo &ctx) override
    {
        EditorSuggestionSet set;
        set.filter = ctx.query;
        set.items.append({QStringLiteral("zebra"), QStringLiteral("zebra!"), {}});
        return set;
    }
};
} // namespace

class NoteEditorWidgetCompletionTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void liveLeaf_triggerInsertUndo_endToEnd()
    {
        EditorSuggestManager manager;
        AtSuggest suggest;
        manager.registerSuggest(&suggest);

        NoteEditorWidget widget;
        widget.setEditorSuggestManager(&manager);
        widget.resize(700, 400);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        // default mode is LivePreview (m_editor, the QML leaf)

        NoteDocument doc(QStringLiteral("/tmp/v"), QStringLiteral("n.md"));
        doc.setMarkdown(QStringLiteral("hello @z\n\nsecond paragraph"));
        widget.setNoteDocument(&doc);

        auto *leaf = widget.activeLeaf();
        QVERIFY(leaf);
        // Place the caret after "@z" (line 1, col 9) through the base
        // contract — the live attach-window contract guarantees this lands.
        leaf->setCursorPosition({1, 9});
        QTRY_VERIFY2(leaf->caretRect().isValid(), "live caretRect never became valid");

        // The popup must appear, anchored within the leaf.
        QTRY_VERIFY(widget.findChild<CompletionPopup *>() != nullptr);
        auto *popup = widget.findChild<CompletionPopup *>();
        QTRY_COMPARE(popup->visibleRowCount(), 1);

        // Accept via the app-level filter (keys to the focused window).
        QTest::keyClick(QApplication::focusWidget() ? QApplication::focusWidget()
                                                    : static_cast<QWidget *>(&widget),
                        Qt::Key_Down);
        QTest::keyClick(QApplication::focusWidget() ? QApplication::focusWidget()
                                                    : static_cast<QWidget *>(&widget),
                        Qt::Key_Return);
        QTRY_VERIFY(QString::fromUtf8(doc.markoff()->serializeForSave())
                        .startsWith(QStringLiteral("hello @zebra!")));

        // One undo step removes it.
        doc.markoff()->undoD2();
        QTRY_VERIFY(QString::fromUtf8(doc.markoff()->serializeForSave())
                        .startsWith(QStringLiteral("hello @z\n")));
    }
};

QTEST_MAIN(NoteEditorWidgetCompletionTest)
#include "tst_note_editor_widget_completion.moc"
```

CMake (mirror `tst_note_editor_widget_ephemeral`'s block including its link list, plus nothing extra).

- [ ] **Step 2: Run it:**

```bash
cmake --build --preset dev --target tst_note_editor_widget_completion -j 10 && \
cd build-dev && QT_QPA_PLATFORM=offscreen ./bin/tst_note_editor_widget_completion 2>&1 | tail -4 && cd ..
```
Expected: PASS. Debug notes if not: (a) popup never appears → check `caretRect()` validity timing (the QTRY covers the seed; if invalid forever, the QML focus item isn't a TextEdit — investigate upstream, don't loosen); (b) keys don't land → `QApplication::focusWidget()` may be the QQuickWidget; the app-level filter sees the event regardless of which widget receives it, so send to the active window: `QTest::keyClick(widget.window(), Qt::Key_Return)` is an acceptable substitution.

- [ ] **Step 3: Full suites, both repos** (markoff unchanged since Task 4 — Corbomite only):

```bash
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10 2>&1 | tail -3 && cd ..
```
Expected: 100% pass.

- [ ] **Step 4: A1 docs.**
  - `docs/PARITY-MATRIX.md`: update the completion rows (find them: `grep -n -i "completion\|suggest" docs/PARITY-MATRIX.md`) — `[[` note-name completion ✅ (note: aliases/headings/blocks pending A2/A3), `#` tag completion ✅, with a pointer to the spec.
  - `docs/punch-list.md`: mark the EditorSuggest clamp item resolved (search "EditorSuggest aborts on cursor-position"): append to its entry: `**[RESOLVED <date> — dispatch-side clamp, Task 6 of completion revival; production clamp now tested.]**` and flip to `[x]`.

- [ ] **Step 5: Commit + push (A1 exit):**

```bash
git add tests/editor/tst_note_editor_widget_completion.cpp tests/editor/CMakeLists.txt \
        docs/PARITY-MATRIX.md docs/punch-list.md
git commit -m "feat(editor): completion revival A1 — [[ names + # tags end-to-end, all editable leaves

Live-leaf integration test (real QML caretRect); PARITY-MATRIX + punch-list updated.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
git push origin master
```

---

# Phase A2

### Task 14: Aliases

**Files:**
- Modify: `src/editor/WikiLinkSuggest.cpp`
- Test: `tests/editor/tst_wikilink_suggest.cpp` (new slots)
- Modify: `src/app/MainWindow.cpp` (wire `setMetadataCache`)

- [ ] **Step 1: Failing test slots.** The fixture needs a `MetadataCache`. Constructing one requires a `LinkResolver` reference (`MetadataCache(*m_linkResolver, this)` per `MainWindow.cpp:2216`) and content fed via `onFileChanged`. Add to the test class members `std::unique_ptr<MetadataCache> m_cache;` and in `initTestCase`, after the resolver setup:

```cpp
        writeNote(QStringLiteral("Aliased.md"),
                  "---\naliases: [Nickname, Other Name]\n---\n# Aliased\n## Section One\n## Section Two\nText ^blockid1\n");
        m_vault->unload(); m_vault->load(m_dir.path());      // pick up the new file
        QStringList paths2;
        for (auto *tf : m_vault->getMarkdownFiles()) paths2 << tf->path;
        m_resolver.setVaultPaths(paths2);
        m_cache = std::make_unique<MetadataCache>(m_resolver, nullptr);
        // Feed content synchronously (no DB open needed for in-memory state —
        // verify: getFileCache must return after onFileChanged; if the cache
        // requires open(), use a temp-file DB path under m_dir).
        for (auto *tf : m_vault->getMarkdownFiles())
            m_cache->onFileChanged(tf->path, m_vault->read(tf), 1);
```

(EXECUTOR NOTE: verify `MetadataCache::onFileChanged` populates `getFileCache` without `open()`; if not, call `m_cache->open(m_dir.path() + "/cache.db")` first. `grep -n "open\|onFileChanged" libs/storage/include/corbomite/storage/MetadataCache.h` and read the comments.)

New slots:

```cpp
    void aliases_appearWithTargetDetail()
    {
        WikiLinkSuggest s(m_vault.get());
        s.setLinkResolver(&m_resolver);
        s.setMetadataCache(m_cache.get());
        auto info = s.onTrigger(2, QStringLiteral("[["), nullptr);
        const auto set = s.getSuggestions(*info);
        bool found = false;
        for (const auto &it : set.items) {
            if (it.display == QStringLiteral("Nickname")) {
                QCOMPARE(it.insertText, QStringLiteral("Aliased|Nickname]]"));
                found = true;
            }
        }
        QVERIFY2(found, "alias candidate missing");
    }

    void aliases_stringFormAccepted()
    {
        // "alias: Solo" (string, not array) must also surface.
        writeNote(QStringLiteral("Solo.md"), "---\nalias: TheOne\n---\nx\n");
        m_cache->onFileChanged(QStringLiteral("Solo.md"),
                               m_vault->read(m_vault->getFileByPath(QStringLiteral("Solo.md"))), 2);
        WikiLinkSuggest s(m_vault.get());
        s.setLinkResolver(&m_resolver);
        s.setMetadataCache(m_cache.get());
        auto info = s.onTrigger(2, QStringLiteral("[["), nullptr);
        const auto set = s.getSuggestions(*info);
        bool found = false;
        for (const auto &it : set.items)
            if (it.display == QStringLiteral("TheOne")) found = true;
        QVERIFY(found);
    }
```

(EXECUTOR NOTE: `Solo.md` is written after `m_vault->load`, so `getFileByPath` may return null unless the watcher catches it — write it in `initTestCase` *before* the reload instead, and feed it to the cache there; keep the slot's assertions.)

- [ ] **Step 2: Verify failure** (alias items absent).

- [ ] **Step 3: Implement** — in `WikiLinkSuggest.cpp` add the helper + extend names mode:

```cpp
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace {
QStringList aliasesFromFrontmatter(const QJsonObject &fm)
{
    QStringList out;
    for (const char *key : {"aliases", "alias"}) {
        const QJsonValue v = fm.value(QLatin1String(key));
        if (v.isString()) {
            out << v.toString();
        } else if (v.isArray()) {
            const QJsonArray arr = v.toArray();
            for (const QJsonValue &e : arr)
                if (e.isString()) out << e.toString();
        }
    }
    out.removeAll(QString());
    return out;
}
} // namespace
```

and inside the names-mode loop in `getSuggestions`, after appending the name item:

```cpp
        if (m_cache) {
            if (const auto md = m_cache->getFileCache(tf->path)) {
                if (md->frontmatter) {
                    const QStringList aliases = aliasesFromFrontmatter(*md->frontmatter);
                    for (const QString &alias : aliases) {
                        EditorSuggestItem ai;
                        ai.display = alias;
                        ai.insertText = target + QStringLiteral("|") + alias + QStringLiteral("]]");
                        ai.detail = QStringLiteral("→ ") + tf->basename;
                        set.items.append(ai);
                    }
                }
            }
        }
```

(`target` is the already-disambiguated link target computed just above.)

- [ ] **Step 4: MainWindow wiring** — in the Task 12 block in `onVaultOpened`, replace the A2 comment with `m_wikiSuggest->setMetadataCache(m_metadataCache);` and add the matching `setMetadataCache(nullptr)` in `onVaultClosed`.

- [ ] **Step 5: Run + commit:**

```bash
cmake --build --preset dev --target tst_wikilink_suggest -j 10 && \
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_wikilink_suggest --output-on-failure 2>&1 | tail -3 && cd ..
git add src/editor/WikiLinkSuggest.cpp src/app/MainWindow.cpp tests/editor/tst_wikilink_suggest.cpp
git commit -m "feat(editor): alias completion — frontmatter aliases insert target|alias

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task 15: Heading completion + A2 docs

**Files:**
- Modify: `src/editor/WikiLinkSuggest.cpp`
- Test: `tests/editor/tst_wikilink_suggest.cpp`
- Modify: `docs/PARITY-MATRIX.md`

- [ ] **Step 1: Failing test slots** (fixture from Task 14 — `Aliased.md` has `## Section One` / `## Section Two`):

```cpp
    void headings_listedForResolvedTarget()
    {
        WikiLinkSuggest s(m_vault.get());
        s.setLinkResolver(&m_resolver);
        s.setMetadataCache(m_cache.get());
        auto info = s.onTrigger(11, QStringLiteral("[[Aliased#S"), nullptr);
        QVERIFY(info.has_value());
        const auto set = s.getSuggestions(*info);
        QCOMPARE(set.filter, QStringLiteral("S"));
        QStringList displays;
        for (const auto &it : set.items) displays << it.display;
        QVERIFY(displays.contains(QStringLiteral("Section One")));
        QVERIFY(displays.contains(QStringLiteral("Section Two")));
        for (const auto &it : set.items)
            if (it.display == QStringLiteral("Section One"))
                QCOMPARE(it.insertText, QStringLiteral("Aliased#Section One]]"));
    }

    void headings_unresolvedTarget_emptyUniverse()
    {
        WikiLinkSuggest s(m_vault.get());
        s.setLinkResolver(&m_resolver);
        s.setMetadataCache(m_cache.get());
        auto info = s.onTrigger(9, QStringLiteral("[[Nope#x"), nullptr);
        QVERIFY(info.has_value());
        QVERIFY(s.getSuggestions(*info).items.isEmpty());
    }
```

- [ ] **Step 2: Verify failure** (names mode currently treats `Aliased#S` as a name query → wrong universe).

- [ ] **Step 3: Implement** — at the top of `getSuggestions`, before the names loop:

```cpp
    // Sub-target modes: `target#headingQuery` / `target#^blockQuery`.
    const int hash = ctx.query.indexOf(QLatin1Char('#'));
    if (hash >= 0) {
        const QString target = ctx.query.left(hash);
        const QString sub = ctx.query.mid(hash + 1);
        if (sub.startsWith(QLatin1Char('^'))) {
            return blockSuggestions(target, sub.mid(1));   // A3 (returns empty until then)
        }
        return headingSuggestions(target, sub);
    }
```

with the two private helpers (declare in the header; `blockSuggestions` returns `{}` with just the filter until Task 16):

```cpp
EditorSuggestionSet WikiLinkSuggest::headingSuggestions(const QString &target,
                                                        const QString &sub)
{
    EditorSuggestionSet set;
    set.filter = sub;
    if (!m_resolver || !m_cache) return set;
    const ResolvedLink link = m_resolver->resolve(m_sourcePath, target);
    if (!link.resolved) return set;
    const auto md = m_cache->getFileCache(link.path);
    if (!md || !md->headings) return set;
    for (const auto &h : *md->headings) {
        EditorSuggestItem item;
        item.display = h.heading;
        item.insertText = target + QStringLiteral("#") + h.heading + QStringLiteral("]]");
        item.detail = link.path;
        set.items.append(item);
    }
    return set;
}

EditorSuggestionSet WikiLinkSuggest::blockSuggestions(const QString &target,
                                                      const QString &sub)
{
    EditorSuggestionSet set;
    set.filter = sub;
    return set;   // Task 16 (A3) fills this in.
}
```

- [ ] **Step 4: Run + A2 docs + commit + push (A2 exit):**

```bash
cmake --build --preset dev -j 10 2>&1 | tail -2 && \
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark -j 10 2>&1 | tail -3 && cd ..
```
Update `docs/PARITY-MATRIX.md` completion rows: aliases ✅, headings ✅ (blocks pending A3).

```bash
git add src/editor/WikiLinkSuggest.h src/editor/WikiLinkSuggest.cpp tests/editor/tst_wikilink_suggest.cpp docs/PARITY-MATRIX.md
git commit -m "feat(editor): [[note# heading completion against the resolved target

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
git push origin master
```

---

# Phase A3

### Task 16: Existing-`^block` completion

**Files:**
- Modify: `src/editor/WikiLinkSuggest.cpp`
- Test: `tests/editor/tst_wikilink_suggest.cpp`

- [ ] **Step 1: Failing test slot** (`Aliased.md` carries `Text ^blockid1`):

```cpp
    void blocks_existingIdsListed()
    {
        WikiLinkSuggest s(m_vault.get());
        s.setLinkResolver(&m_resolver);
        s.setMetadataCache(m_cache.get());
        auto info = s.onTrigger(12, QStringLiteral("[[Aliased#^b"), nullptr);
        QVERIFY(info.has_value());
        const auto set = s.getSuggestions(*info);
        QCOMPARE(set.filter, QStringLiteral("b"));
        QVERIFY(!set.items.isEmpty());
        bool found = false;
        for (const auto &it : set.items) {
            if (it.display == QStringLiteral("^blockid1")) {
                QCOMPARE(it.insertText, QStringLiteral("Aliased#^blockid1]]"));
                found = true;
            }
        }
        QVERIFY2(found, "existing ^blockid1 not offered");
    }
```

- [ ] **Step 2: Verify it fails** (stub returns empty), **Step 3: implement** `blockSuggestions`:

```cpp
EditorSuggestionSet WikiLinkSuggest::blockSuggestions(const QString &target,
                                                      const QString &sub)
{
    EditorSuggestionSet set;
    set.filter = sub;
    if (!m_resolver || !m_cache) return set;
    const ResolvedLink link = m_resolver->resolve(m_sourcePath, target);
    if (!link.resolved) return set;
    const auto md = m_cache->getFileCache(link.path);
    if (!md || !md->blocks) return set;     // existing ids only (spec §1)
    for (auto it = md->blocks->cbegin(); it != md->blocks->cend(); ++it) {
        EditorSuggestItem item;
        item.display = QStringLiteral("^") + it.key();
        item.insertText = target + QStringLiteral("#^") + it.key() + QStringLiteral("]]");
        item.detail = link.path;
        set.items.append(item);
    }
    return set;
}
```

(EXECUTOR NOTE: if the test's `MetadataCache` fixture yields no `blocks` for `Text ^blockid1`, check `MetadataParser`'s block-id capture — `blocks` keys are stored sans `^` per `CachedMetadata.h:117`. The display string prepends `^`; the fuzzy filter `b` matches `^blockid1` because `FuzzyMatcher` is substring-tolerant — if it is NOT (strict prefix), change `set.filter = sub` stays correct since the user typed after `^`; verify the popup proxy filters `display` — if `^` prefix breaks matching, drop it from `display`.)

- [ ] **Step 4: Run + commit:**

```bash
cmake --build --preset dev --target tst_wikilink_suggest -j 10 && \
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_wikilink_suggest --output-on-failure 2>&1 | tail -3 && cd ..
git add src/editor/WikiLinkSuggest.cpp tests/editor/tst_wikilink_suggest.cpp
git commit -m "feat(editor): [[note#^ existing-block-id completion

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task 17: Closeout

**Files:**
- Modify: `docs/PARITY-MATRIX.md`, `docs/punch-list.md`, `docs/PROJECT-STATE.md`, `docs/decisions-archive.md`

- [ ] **Step 1: Full suites, both repos:**

```bash
cd /home/clinton/dev/Corbomite/libs/markoff-family && scripts/run-tests.sh -E 'tst_realistic|tst_benchmark' 2>&1 | tail -4
cd /home/clinton/dev/Corbomite/build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10 2>&1 | tail -3 && cd ..
```
Expected: markoff = 3 known reds only; Corbomite = 100%.

- [ ] **Step 2: Docs.**
  - `PARITY-MATRIX.md`: blocks row ✅ (existing ids; creation deferred).
  - `punch-list.md`: add under a dated header the follow-ups from spec §14: `^id` creation on block pick (P3, `[editor][completion]`), code-block trigger suppression via `EditorContext` (P4), popup `detail` rendering polish (P5).
  - `PROJECT-STATE.md` § Last touched: 3-sentence entry — completion revival shipped (A1–A3), spec/plan paths, suite counts. Replace the previous top entry per the no-cascade rule.
  - `decisions-archive.md`: full closeout paragraph (what shipped, the caretRect contract addition + markoff commits, interface-v2 ABI note, deferred items).

- [ ] **Step 3: Commit + push:**

```bash
git add docs/PARITY-MATRIX.md docs/punch-list.md docs/PROJECT-STATE.md docs/decisions-archive.md
git commit -m "docs: completion revival closeout — A1-A3 shipped; follow-ups punch-listed

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
git push origin master
```

- [ ] **Step 4: User eyeball note.** Completion popups shipped offscreen-verified only — add "completion popup look & feel ([[ / # / aliases / headings / ^blocks), all modes" to the PROJECT-STATE eyeball-verification backlog list (§ Open questions).
