# Styled Headless Rendering Convergence — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Advance Corbomite's Markoff submodule to a build that contains `markoff-styled`, then make Corbomite's reading mode a real read-only `Markoff::Styled::Editor` leaf.

**Architecture:** `Markoff::Styled::Editor` is a QWidget (no QML) that subclasses `Markoff::MarkdownView`, the same polymorphic base Corbomite's `NoteEditorWidget` already swaps between for Live/Source leaves. So reading mode becomes a third leaf in the existing `QStackedWidget`, attached read-only via the existing `setDocument()` swap path. The submodule re-pin is a gating port-compat prerequisite.

**Tech Stack:** C++20, Qt6 Widgets, KDE Frameworks 6, CMake presets (`dev` → `build-dev/`), CTest, git submodule.

**Scope:** This plan covers ONLY the slice actionable now — the **submodule re-pin** and the **read-only styled reading leaf**. Canvas-card rendering and HoverPopover are blocked on Markoff shipping the `Markoff::Styled::DocumentRenderer` requested in [`docs/handoff/2026-05-29-to-markoff-styled-document-renderer.md`](../../handoff/2026-05-29-to-markoff-styled-document-renderer.md); they get a follow-up plan once that lands (see "Deferred" at the bottom). Design: [`docs/superpowers/specs/2026-05-29-styled-headless-rendering-convergence-design.md`](../specs/2026-05-29-styled-headless-rendering-convergence-design.md).

---

## File Structure

| File | Responsibility | Action |
|---|---|---|
| `libs/markoff-family` (submodule gitlink) | Pin to a Markoff master commit containing `markoff-styled` | Modify |
| `src/CMakeLists.txt` | Link the `markoff_styled` target into `CorbomiteApp` | Modify |
| `src/editor/NoteEditorWidget.h` | Declare the styled reading leaf member + forward-decl | Modify |
| `src/editor/NoteEditorWidget.cpp` | Construct/return/attach the read-only styled leaf for `ViewMode::Reading` | Modify |
| `tests/editor/tst_reading_styled_leaf.cpp` | Verify reading mode = read-only styled leaf reflecting canonical content | Create |
| `tests/editor/CMakeLists.txt` | Register the new test target | Modify |

---

## Phase 0 — Re-pin the submodule (port-compat prerequisite)

> **Nature of this phase:** This is a verification-driven port, not red-green TDD. The exact compile/link breakage from 79 commits of Markoff drift can't be pre-scripted — Step 2 discovers it, Step 3 fixes iteratively, and the verification gate (existing suite green) is the success criterion. If breakage is large, capture it as its own sub-punch-list and treat each fix as a commit.

### Task 0.1: Advance the gitlink and rebuild

**Files:**
- Modify: `libs/markoff-family` (submodule gitlink)

- [ ] **Step 1: Record the current pin and target commit**

Run:
```bash
cd /home/clinton/dev/Corbomite
git -C libs/markoff-family rev-parse HEAD          # current: dc86ca7… (freeze-14)
git -C /home/clinton/dev/Markoff rev-parse HEAD    # target master tip (e.g. 46643e7…)
```
Record both hashes in the eventual commit message.

- [ ] **Step 2: Move the submodule working tree to the target commit**

The submodule's `origin` is the Codeberg remote, but the freshest commits live in your local `~/dev/Markoff`. Fetch from the local clone, then check out the target commit:
```bash
cd /home/clinton/dev/Corbomite/libs/markoff-family
git fetch /home/clinton/dev/Markoff master
git checkout FETCH_HEAD          # detached at the target master tip
cd /home/clinton/dev/Corbomite
git -C libs/markoff-family rev-parse HEAD   # confirm it matches the target
```
Note: ensure the target commit is pushed to the submodule's real remote before this plan's work is shared, so other machines can resolve the new pin.

- [ ] **Step 3: Configure + build, capturing breakage**

Run:
```bash
cmake --preset dev
cmake --build --preset dev -j 10 2>&1 | tee /tmp/repin-build.log
```
Expected: may FAIL with compile/link errors from Markoff API drift. Triage `/tmp/repin-build.log`. Likely areas (from prior port notes): `markoff/*` include-path or type renames, `MarkoffDocument` signal/method changes, parser/`YamlValue` surface. Fix each error in the Corbomite consumer (NOT in the submodule) — adapt call sites to the new API. Re-run until the build is clean. Commit fixes in coherent chunks (e.g. one per consumer file/subsystem).

- [ ] **Step 4: Run the full existing test suite (the gate)**

Run:
```bash
cd build-dev && ctest --output-on-failure -j 10 2>&1 | tee /tmp/repin-ctest.log
```
Expected: same green/disabled set as before the re-pin. Any NEW failure is re-pin fallout — fix it. Pre-existing `if(FALSE)` disabled tests stay disabled (do not enable here).

- [ ] **Step 5: Smoke-launch offscreen**

Run:
```bash
QT_QPA_PLATFORM=offscreen ./build-dev/Corbomite --version 2>&1 | head || true
QT_QPA_PLATFORM=offscreen timeout 5 ./build-dev/Corbomite 2>&1 | tail -20
```
Expected: launches without crash (a clean exit on timeout is fine).

- [ ] **Step 6: Commit the re-pin**

```bash
cd /home/clinton/dev/Corbomite
git add libs/markoff-family <any consumer files you fixed>
git commit -m "chore(submodule): advance markoff-family to master (styled available)

Re-pin <old-hash> -> <new-hash>. Port-compat fixes for API drift.
Unblocks the read-only styled reading leaf."
```

---

## Phase 1 — Read-only styled reading leaf

> TDD. The styled leaf is a drop-in `MarkdownView`; we add it as the third stack page and return it from `activeLeaf()` for `ViewMode::Reading`. Confirm exact accessor names (`textEdit()`, `setReadOnly`) against the re-pinned `libs/markoff-family/libs/markoff-styled/include/markoff/styled/Editor.h` — they are taken from that header.

### Task 1.1: Confirm the styled target + Editor API

**Files:**
- Read only (no edit): `libs/markoff-family/libs/markoff-styled/CMakeLists.txt`, `.../include/markoff/styled/Editor.h`

- [ ] **Step 1: Find the CMake target name and the public API**

Run:
```bash
cd /home/clinton/dev/Corbomite
grep -nE 'add_library|add_alias|Markoff::|qt_add' libs/markoff-family/libs/markoff-styled/CMakeLists.txt
grep -nE 'class .*Editor|setReadOnly|setDocument|textEdit|MarkdownView' libs/markoff-family/libs/markoff-styled/include/markoff/styled/Editor.h
```
Expected: a target (mirroring `markoff_live`/`markoff_source` — likely `markoff_styled`, possibly an alias `Markoff::Styled`) and confirmation that `Markoff::Styled::Editor : Markoff::MarkdownView` with `setReadOnly(bool)`, `setDocument(MarkoffDocument*)`, and a `textEdit()` accessor. Record the exact target name + accessor; use it consistently below (this plan assumes target `markoff_styled` and accessor `textEdit()`).

### Task 1.2: Failing test — reading mode is a read-only styled leaf

**Files:**
- Create: `tests/editor/tst_reading_styled_leaf.cpp`
- Modify: `tests/editor/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/editor/tst_reading_styled_leaf.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reading mode is a read-only Markoff::Styled::Editor leaf (no QML).
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"

#include <markoff/styled/Editor.h>
#include <markoff/core/MarkdownView.h>
#include <markoff/core/MarkoffDocument.h>

#include "corbomite/core/NoteDocument.h"

#include <QSignalSpy>
#include <QTest>
#include <QTextEdit>

using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;

class ReadingStyledLeafTest : public QObject {
    Q_OBJECT
private Q_SLOTS:

    void readingMode_constructsReadOnlyStyledLeaf()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("# Title\n\nbody text here"));
        widget.setNoteDocument(&doc);

        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);

        auto *leaf = widget.activeLeaf();
        QVERIFY(leaf);
        auto *styled = qobject_cast<Markoff::Styled::Editor *>(leaf);
        QVERIFY2(styled, "Reading mode leaf must be a Markoff::Styled::Editor");
        QVERIFY2(styled->isReadOnly(), "Reading leaf must be read-only");
    }

    void readingLeaf_reflectsCanonicalContent()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("# Title\n\nbody text here"));
        widget.setNoteDocument(&doc);

        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);
        QTest::qWait(50);  // styled applies formats on d2DocumentChanged

        auto *styled =
            qobject_cast<Markoff::Styled::Editor *>(widget.activeLeaf());
        QVERIFY(styled);
        // Styled keeps delimiters visible, so plain text contains the body.
        QVERIFY2(styled->textEdit()->toPlainText().contains(
                     QStringLiteral("body text here")),
                 "Reading leaf must show canonical content");
    }

    void switchingAwayFromReading_stillEmitsAndSwaps()
    {
        NoteEditorWidget widget;
        widget.resize(400, 200);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("hi"));
        widget.setNoteDocument(&doc);

        QSignalSpy spy(&widget, &NoteEditorWidget::viewModeChanged);
        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);
        QCOMPARE(spy.count(), 1);
        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);
        QCOMPARE(spy.count(), 2);
        QVERIFY(qobject_cast<Markoff::Styled::Editor *>(widget.activeLeaf())
                == nullptr);  // back on the Live leaf
    }
};

QTEST_MAIN(ReadingStyledLeafTest)
#include "tst_reading_styled_leaf.moc"
```

- [ ] **Step 2: Register the test target**

Append to `tests/editor/CMakeLists.txt` (after the `tst_findbar` block):
```cmake
# 2026-05-29 — Reading mode is a read-only Markoff::Styled::Editor leaf.
add_executable(tst_reading_styled_leaf
    tst_reading_styled_leaf.cpp
)
target_include_directories(tst_reading_styled_leaf PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_reading_styled_leaf PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
    markoff_styled
    Corbomite::Core
)
add_test(NAME tst_reading_styled_leaf COMMAND tst_reading_styled_leaf)
set_tests_properties(tst_reading_styled_leaf PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

- [ ] **Step 3: Build + run to verify it FAILS**

Run:
```bash
cmake --preset dev
cmake --build --preset dev -j 10 --target tst_reading_styled_leaf
cd build-dev && ctest -R tst_reading_styled_leaf --output-on-failure
```
Expected: FAIL — `activeLeaf()` returns the Live `EditorWidget`, so `qobject_cast<Markoff::Styled::Editor*>` is null. (If it fails to *compile* on `markoff_styled` not being a known target, fix the target name per Task 1.1 first.)

### Task 1.3: Add the styled reading leaf to NoteEditorWidget

**Files:**
- Modify: `src/editor/NoteEditorWidget.h`
- Modify: `src/editor/NoteEditorWidget.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Link the styled target into CorbomiteApp**

In `src/CMakeLists.txt`, in the `target_link_libraries(CorbomiteApp …)` block, add `markoff_styled` next to `Markoff::Source` (around the `markoff_live`/`Markoff::Source` lines):
```cmake
        markoff_live
        markoff_liveplugin
        markoff_liveplugin_init
        Markoff::Source
        markoff_styled
```

- [ ] **Step 2: Declare the styled leaf in the header**

In `src/editor/NoteEditorWidget.h`, add a forward decl after the `Markoff::Source` block (around line 21):
```cpp
namespace Markoff::Styled {
class Editor;
}
```
And add the member next to `m_sourceEditor` (around line 169):
```cpp
    // Reading mode widget — a read-only Markoff::Styled::Editor (QWidget, no
    // QML). Lazy, same pattern as m_sourceEditor. Replaces the retired
    // Markoff::Reading::ReadingView stub.
    Markoff::Styled::Editor *m_styledReadingView = nullptr;
```
(Leave the existing `m_readingView` / `m_readingIndex` for now; Phase 2 removes them.)

- [ ] **Step 3: Include the styled header in the .cpp**

In `src/editor/NoteEditorWidget.cpp`, add near the other Markoff includes:
```cpp
#include <markoff/styled/Editor.h>
```

- [ ] **Step 4: Construct the leaf in `ensureWidgetConstructed`**

Replace the `case ViewMode::Reading:` body in `ensureWidgetConstructed` with:
```cpp
    case ViewMode::Reading:
        if (!m_styledReadingView) {
            m_styledReadingView = new Markoff::Styled::Editor(this);
            m_styledReadingView->setReadOnly(true);
            m_readingIndex = m_stack->addWidget(m_styledReadingView);
        }
        break;
```

- [ ] **Step 5: Return the leaf from `activeLeaf`**

Replace the `case ViewMode::Reading:` body in `activeLeaf()` with:
```cpp
    case ViewMode::Reading:
        return m_styledReadingView;  // may be nullptr if not yet constructed
```

- [ ] **Step 6: Fix `stackIndexFor` for Reading (if needed)**

Confirm `stackIndexFor(ViewMode::Reading)` returns `m_readingIndex` (it should already, since the member existed). If it referenced the old `m_readingView`, point it at `m_readingIndex`. Remove the now-moot `m_editor->setReadOnly(newMode == ViewMode::Reading)` line in `setViewMode` (the styled leaf owns read-only now; leaving it forces the Live leaf read-only needlessly).

- [ ] **Step 7: Build + run to verify it PASSES**

Run:
```bash
cmake --build --preset dev -j 10 --target tst_reading_styled_leaf
cd build-dev && ctest -R tst_reading_styled_leaf --output-on-failure
```
Expected: PASS (all 3 cases).

- [ ] **Step 8: Commit**

```bash
cd /home/clinton/dev/Corbomite
git add src/editor/NoteEditorWidget.h src/editor/NoteEditorWidget.cpp \
        src/CMakeLists.txt tests/editor/tst_reading_styled_leaf.cpp \
        tests/editor/CMakeLists.txt
git commit -m "feat(editor): reading mode is a read-only styled leaf

Replaces the no-op Reading stub with a read-only Markoff::Styled::Editor
(QWidget, no QML). Resolves the 'Reading-mode dead-end' audit finding;
retires the read-only-Live dependency for reading."
```

---

## Phase 2 — Retire the dead Reading stubs + stale references

> Cleanup. The `Markoff::Reading::ReadingView` stub and the read-only-Live language are now superseded.

### Task 2.1: Remove the ReadingView stub plumbing

**Files:**
- Modify: `src/editor/NoteEditorWidget.h`
- Modify: `src/editor/NoteEditorWidget.cpp`

- [ ] **Step 1: Remove the dead members + accessor**

In `NoteEditorWidget.h`: delete the `namespace Markoff::Reading { class ReadingView; }` forward-decl block (lines ~23-28), the `readingView()` accessor declaration (line ~79), and the `Markoff::Reading::ReadingView *m_readingView` member (line ~171).
In `NoteEditorWidget.cpp`: delete the `readingView()` definition (the `return nullptr;` stub).

- [ ] **Step 2: Build the whole app + run the full suite**

Run:
```bash
cmake --build --preset dev -j 10
cd build-dev && ctest --output-on-failure -j 10
```
Expected: clean build; suite green (same set as Phase 0, plus the new `tst_reading_styled_leaf`). Fix any caller of `readingView()` that surfaces (grep `readingView(` across `src/` first if the build errors).

- [ ] **Step 3: Update the stale read-only-Live comments**

In `NoteEditorWidget.h`, update the `ViewMode` doc-comment (lines ~50-55) so `Reading` is described as "a read-only `Markoff::Styled::Editor` leaf" rather than "the ReadingView widget".

- [ ] **Step 4: Commit**

```bash
git add src/editor/NoteEditorWidget.h src/editor/NoteEditorWidget.cpp
git commit -m "refactor(editor): drop retired Markoff::Reading::ReadingView stub

Reading mode now owns a styled leaf; the nullptr-returning stub and its
forward-decl are dead."
```

### Task 2.2: Update tracking docs

**Files:**
- Modify: `docs/PROJECT-STATE.md`
- Modify: `docs/punch-list.md`

- [ ] **Step 1: Record the reading-mode resolution**

In `docs/PROJECT-STATE.md` "Open questions", update the Reading-mode line: it's now **resolved** as a read-only styled leaf (not read-only Live); the `Capabilities::Editable` steer to Markoff is retired (see the 2026-05-29 handoff).

- [ ] **Step 2: Tick the audit's Reading-mode fork**

In `docs/punch-list.md`, add a `[x]` entry under the appropriate section noting the "Reading-mode dead-end" fork (audit-2026-05-29) is resolved by the styled reading leaf.

- [ ] **Step 3: Commit**

```bash
git add docs/PROJECT-STATE.md docs/punch-list.md
git commit -m "docs: reading mode resolved as read-only styled leaf"
```

---

## Deferred to a follow-up plan (blocked on Markoff)

Not in this plan — gated on Markoff shipping `Markoff::Styled::DocumentRenderer` (T1+T2) per [`docs/handoff/2026-05-29-to-markoff-styled-document-renderer.md`](../../handoff/2026-05-29-to-markoff-styled-document-renderer.md):

- **Canvas cards:** a `StyledRenderEngine : MarkdownRenderEngine` wrapping the renderer; wire the missing `setRenderEngine` call (`MainWindow` → `CanvasViewTab`/`CanvasFileView` → `CanvasScene`) and make `CanvasScene::setRenderEngine` re-render existing cards.
- **HoverPopover:** repoint its render path from the retired `Markoff::Reading::*` to the headless renderer.

When the renderer lands, write `docs/superpowers/plans/<date>-styled-card-rendering.md` and re-pin again to the commit that contains it.

---

## Self-Review

- **Spec coverage:** Re-pin prerequisite → Phase 0. Read-only styled reading leaf → Phase 1. Retire read-only-Live / Reading stub → Phase 2. Canvas cards + HoverPopover → explicitly deferred (blocked on Markoff), matching the spec's sequencing. ✔
- **Placeholder scan:** Phase 0 is intentionally discovery-driven (port-compat) — its "fix each error" steps are real procedure, not placeholders, because the drift set is unknowable until built. All TDD steps (Phase 1) carry complete code. ✔
- **Type consistency:** `Markoff::Styled::Editor`, `m_styledReadingView`, `m_readingIndex`, `markoff_styled` (target), `textEdit()`/`setReadOnly()`/`isReadOnly()` used consistently; Task 1.1 confirms the target name + accessors against the re-pinned header before they're relied on. ✔
- **Known assumption:** target name `markoff_styled` and accessor `textEdit()` are from the live-repo investigation; Task 1.1 Step 1 verifies them against the re-pinned submodule and the plan says to correct if they differ.
