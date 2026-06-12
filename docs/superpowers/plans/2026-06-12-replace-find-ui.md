# Replace for Corbomite — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Find/Replace for Corbomite — a host-owned replace row on the
existing `FindBar`, driven by a minimal coordinate-correct mutation primitive
upstream in Markoff, and wire the placebo Find… / Replace… menu actions.

**Architecture:** Option C from the design. Markoff gains two small additions —
`MarkoffDocument::replaceMatches(QList<SearchHit>, QString)` (one undo
transaction, synchronous post-state) and the mutation-free
`FindController::selectMatchAtOrAfter(BlockAnchor, offset)`. All UI and
replace/replace-all policy stay consumer-side: `Corbomite::FindBar` grows a
replace row, `NoteEditorWidget` owns the replace orchestration, `MainWindow`
adds Replace and wires the menu placebos.

**Tech Stack:** Qt6 Widgets, Markoff::Core, KStandardAction, QtTest.

**Spec:** [`docs/superpowers/specs/2026-06-12-replace-find-ui-design.md`](../specs/2026-06-12-replace-find-ui-design.md).

**Cross-repo ordering (Markoff-first, per `~/dev/CLAUDE.md`):** Phase 1 lands in
the **Markoff** submodule repo (`libs/markoff-family`, its own `master`); Phase 2
re-pins it; Phases 3–4 are Corbomite. Do not start Phase 3 until Phase 2's build
is green.

---

## Orientation

**Markoff repo:** `/home/clinton/dev/Markoff` (also reachable at
`/home/clinton/dev/Corbomite/libs/markoff-family`). Branch: `master`.
- Build: `cmake --build build-dev -j 8`
- Test one binary: `scripts/run-tests.sh --bin <name>` (sets `QT_QPA_PLATFORM=offscreen`).
- Commit trailer: `Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`

**Corbomite repo:** `/home/clinton/dev/Corbomite`. Branch: `feature/find-replace`.
- Build: `cmake --build --preset dev -j 10`
- Test one binary: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R <name> --output-on-failure`
- Baseline before starting Phase 3: **259/259** (excl. `benchmark`).

**Never `git add -A`** (Corbomite project rule) — stage explicit paths.

---

# Phase 1 — Markoff upstream (in `libs/markoff-family`)

### Task 0: Companion Markoff spec

Markoff requires a spec in its own `docs/specs/` for upstream work (its CLAUDE.md
rule). This is a one-file pointer to the Corbomite design, not a re-brainstorm.

**Files:**
- Create: `/home/clinton/dev/Markoff/docs/specs/2026-06-12-replace-matches-primitive-design.md`

- [ ] **Step 1: Write the spec pointer**

```markdown
# replaceMatches primitive + selectMatchAtOrAfter — design

**Date:** 2026-06-12
**Driving consumer:** Corbomite Find/Replace. Full design:
Corbomite `docs/superpowers/specs/2026-06-12-replace-find-ui-design.md`.

## Additions (markoff-core)

1. `void MarkoffDocument::replaceMatches(const QList<SearchHit> &matches,
   const QString &replacement)` — replace each match's byte span with literal
   `replacement` as ONE undo transaction. Block-local match offsets are mapped
   to global no-separator flat offsets via `iterateBlocks()` + `blockText()`
   accumulation (NOT `blockByteRange`, which is parse-source space). Edits are
   applied descending-by-start so earlier-applied edits never shift later ones;
   folded into one UndoLog entry via `coalesceLastUndo()`; ends with
   `flushPendingD2Changed()` so the post-state (and any active FindController
   recompute) is synchronous.

2. `void FindController::selectMatchAtOrAfter(Markoff::BlockAnchor block,
   quint32 offset)` — mutation-free; moves only `currentMatchIndex` to the first
   match at/after (block, offset), wrapping to 0. Emits `currentMatchChanged`.
   Preserves invariant D3 (no document/focus/cursor/scroll contact).

## Invariants
Touches the edit path, not the focus/caret/block-change seam — seam rules
(INVARIANTS §scope) do not bind. INVARIANT 4 (falsifiable, production-callsite
tests) applies: see `tst_replace_matches` (offset-shift case is falsifiable by
reversing the apply order).
```

- [ ] **Step 2: Commit**

```bash
cd /home/clinton/dev/Markoff
git add docs/specs/2026-06-12-replace-matches-primitive-design.md
git commit -m "docs(spec): replaceMatches primitive + selectMatchAtOrAfter

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 1: `MarkoffDocument::replaceMatches`

**Files:**
- Create test: `/home/clinton/dev/Markoff/libs/markoff-core/tests/tst_replace_matches.cpp`
- Modify: `libs/markoff-core/tests/CMakeLists.txt` (register the binary)
- Modify: `libs/markoff-core/include/markoff/core/MarkoffDocument.h` (declare + include SearchEngine.h)
- Modify: `libs/markoff-core/src/MarkoffDocument.cpp` (implement)

- [ ] **Step 1: Write the failing test**

Create `tst_replace_matches.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include <markoff/core/MarkoffDocument.h>
#include <markoff/core/SearchEngine.h>

using namespace Markoff;

class TstReplaceMatches : public QObject {
    Q_OBJECT
    static QByteArray afterReplace(const char *md, const QString &needle,
                                   const QString &repl) {
        MarkoffDocument doc(1);
        doc.loadFromMarkdown(md);
        doc.replaceMatches(SearchEngine::findByBlock(doc, needle), repl);
        return doc.serializeForSave();
    }
private slots:
    void single_match() {
        QCOMPARE(afterReplace("Find me here\n", "me", "you"),
                 QByteArray("Find you here\n"));
    }
    void multiple_matches_one_block() {
        QCOMPARE(afterReplace("foo foo foo\n", "foo", "bar"),
                 QByteArray("bar bar bar\n"));
    }
    void matches_across_blocks() {
        QCOMPARE(afterReplace("alpha\n\nbeta alpha\n", "alpha", "X"),
                 QByteArray("X\n\nbeta X\n"));
    }
    void length_changing_replacement_keeps_offsets() {
        // Falsifiable: applying front-to-back without offset compensation
        // corrupts the second match. Back-to-front is correct.
        QCOMPARE(afterReplace("ab ab\n", "ab", "abcd"),
                 QByteArray("abcd abcd\n"));
    }
    void replace_all_is_one_undo() {
        MarkoffDocument doc(1);
        doc.loadFromMarkdown("foo foo foo\n");
        doc.replaceMatches(SearchEngine::findByBlock(doc, "foo"), "bar");
        QCOMPARE(doc.serializeForSave(), QByteArray("bar bar bar\n"));
        doc.undoD2();
        QCOMPARE(doc.serializeForSave(), QByteArray("foo foo foo\n"));
    }
    void empty_list_is_noop() {
        MarkoffDocument doc(1);
        doc.loadFromMarkdown("nothing here\n");
        doc.replaceMatches({}, "x");
        QCOMPARE(doc.serializeForSave(), QByteArray("nothing here\n"));
    }
};

QTEST_MAIN(TstReplaceMatches)
#include "tst_replace_matches.moc"
```

- [ ] **Step 2: Register the test binary**

In `libs/markoff-core/tests/CMakeLists.txt`, after the
`tst_foundation_find_controller` block (line ~67), add:

```cmake
add_executable(tst_replace_matches tst_replace_matches.cpp)
add_test(NAME tst_replace_matches COMMAND tst_replace_matches)
target_link_libraries(tst_replace_matches PRIVATE Qt6::Test markoff_core)
set_tests_properties(tst_replace_matches PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Run the test — verify it fails to compile**

Run: `cd /home/clinton/dev/Markoff && cmake --build build-dev -j 8 --target tst_replace_matches`
Expected: FAIL — `replaceMatches` is not a member of `MarkoffDocument`.

- [ ] **Step 4: Declare the method + include SearchHit**

In `libs/markoff-core/include/markoff/core/MarkoffDocument.h`, add the include
near the existing includes (after `#include <markoff/core/BlockAnchor.h>` at
line 22):

```cpp
#include <markoff/core/SearchEngine.h>   // SearchHit
```

Then, immediately after the `applyFlatEdit(...)` declaration (line ~187), add:

```cpp
    /// Replace each match's byte span with literal `replacement`, as ONE undo
    /// transaction. `matches` carry block-local byte offsets (SearchEngine
    /// coordinate space); they are mapped to global no-separator flat offsets
    /// and applied descending-by-start so earlier-applied edits never shift
    /// later ones. Stale matches (block no longer present) are skipped. Empty
    /// list is a no-op. Ends with a synchronous d2DocumentChanged flush so an
    /// active FindController has recomputed by the time this returns.
    void replaceMatches(const QList<SearchHit> &matches,
                        const QString &replacement);
```

- [ ] **Step 5: Implement**

In `libs/markoff-core/src/MarkoffDocument.cpp`, ensure `#include <QHash>` and
`#include <algorithm>` are present (add if missing), then add (next to
`applyFlatEdit`'s definition, after its closing brace near line ~1560):

```cpp
void MarkoffDocument::replaceMatches(const QList<SearchHit> &matches,
                                     const QString &replacement)
{
    if (matches.isEmpty())
        return;

    // Block-local match offsets → global no-separator flat offsets. Base of a
    // block is the running sum of blockText sizes in iterateBlocks() order —
    // the exact coordinate space applyFlatEdit consumes.
    QHash<BlockId, quint32> baseOf;
    quint32 cursor = 0;
    for (BlockId id : iterateBlocks()) {
        baseOf.insert(id, cursor);
        cursor += static_cast<quint32>(blockText(id).size());
    }

    struct Edit { quint32 start; quint32 end; };
    QList<Edit> edits;
    edits.reserve(matches.size());
    for (const SearchHit &m : matches) {
        const auto it = baseOf.constFind(m.blockId);
        if (it == baseOf.constEnd())
            continue;  // stale match — block gone since the search
        const quint32 g = *it + m.matchStart;
        edits.append(Edit{ g, g + m.matchLen });
    }
    if (edits.isEmpty())
        return;

    // Apply back-to-front so an earlier-applied edit never shifts the offsets
    // of an edit not yet applied; coalesce all into one undo entry.
    std::sort(edits.begin(), edits.end(),
              [](const Edit &a, const Edit &b) { return a.start > b.start; });

    const QByteArray repl = replacement.toUtf8();
    bool first = true;
    for (const Edit &e : edits) {
        applyFlatEdit(e.start, e.end, repl, Origin::UserEdit);
        if (!first)
            coalesceLastUndo();
        first = false;
    }

    // applyFlatEdit debounces d2DocumentChanged (QTimer::singleShot(0)); flush
    // once so the post-state — and any active FindController recompute — is
    // synchronous for the caller.
    flushPendingD2Changed();
}
```

- [ ] **Step 6: Run the test — verify it passes**

Run: `cd /home/clinton/dev/Markoff && scripts/run-tests.sh --bin tst_replace_matches`
Expected: PASS (6/6).

- [ ] **Step 7: Commit**

```bash
cd /home/clinton/dev/Markoff
git add libs/markoff-core/tests/tst_replace_matches.cpp \
        libs/markoff-core/tests/CMakeLists.txt \
        libs/markoff-core/include/markoff/core/MarkoffDocument.h \
        libs/markoff-core/src/MarkoffDocument.cpp
git commit -m "feat(core): MarkoffDocument::replaceMatches — one-undo literal replace

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: `FindController::selectMatchAtOrAfter`

**Files:**
- Modify test: `libs/markoff-core/tests/tst_foundation_find_controller.cpp`
- Modify: `libs/markoff-core/include/markoff/core/FindController.h`
- Modify: `libs/markoff-core/src/FindController.cpp`

- [ ] **Step 1: Write the failing test**

Append these slots inside `TstFoundationFindController` (before the closing `};`):

```cpp
    void selectMatchAtOrAfter_lands_on_first_at_or_after() {
        MarkoffDocument doc(1);
        doc.loadFromMarkdown("aa bb aa bb aa\n");  // "aa" at byte 0, 6, 12
        FindController fc(&doc);
        fc.activate();
        fc.setNeedle("aa");
        QCOMPARE(fc.matchCount(), 3);
        const auto block = fc.matches().at(0).block;
        fc.selectMatchAtOrAfter(block, 6);
        QCOMPARE(fc.currentMatchIndex(), 1);
        fc.selectMatchAtOrAfter(block, 7);   // first >= 7 is the match at 12
        QCOMPARE(fc.currentMatchIndex(), 2);
    }
    void selectMatchAtOrAfter_wraps_to_zero() {
        MarkoffDocument doc(1);
        doc.loadFromMarkdown("aa aa\n");
        FindController fc(&doc);
        fc.activate();
        fc.setNeedle("aa");
        const auto block = fc.matches().at(0).block;
        fc.selectMatchAtOrAfter(block, 9999);  // past end → wrap
        QCOMPARE(fc.currentMatchIndex(), 0);
    }
```

- [ ] **Step 2: Run — verify it fails to compile**

Run: `cd /home/clinton/dev/Markoff && cmake --build build-dev -j 8 --target tst_foundation_find_controller`
Expected: FAIL — `selectMatchAtOrAfter` not a member.

- [ ] **Step 3: Declare the method**

In `FindController.h`, after `Q_INVOKABLE void findPrevious();` (line 63), add:

```cpp
    /// Move the current-match selection to the first match at or after
    /// (block, offset) in document order, wrapping to index 0 if none.
    /// Mutation-free: touches only currentMatchIndex (emits currentMatchChanged).
    /// Never touches the document, focus, cursor, or scroll (invariant D3).
    void selectMatchAtOrAfter(Markoff::BlockAnchor block, quint32 offset);
```

- [ ] **Step 4: Implement**

In `FindController.cpp`, add `#include <QHash>` and `#include <climits>` at the
top, then add (before `recomputeMatches`):

```cpp
void FindController::selectMatchAtOrAfter(Markoff::BlockAnchor block,
                                          quint32 offset)
{
    if (m_matches.isEmpty() || !m_doc)
        return;

    // Document-order index per block, for cross-block comparison.
    QHash<Markoff::BlockId, int> order;
    int n = 0;
    for (Markoff::BlockId id : m_doc->iterateBlocks())
        order.insert(id, n++);
    const int targetOrd = order.value(block, INT_MAX);

    int chosen = -1;
    for (int k = 0; k < m_matches.size(); ++k) {
        const int mOrd = order.value(m_matches[k].block, INT_MAX);
        if (mOrd > targetOrd ||
            (mOrd == targetOrd && m_matches[k].byteOffset >= offset)) {
            chosen = k;
            break;
        }
    }
    if (chosen < 0)
        chosen = 0;  // wrap
    if (chosen == m_currentIndex)
        return;
    m_currentIndex = chosen;
    Q_EMIT currentMatchChanged();
}
```

- [ ] **Step 5: Run — verify it passes**

Run: `cd /home/clinton/dev/Markoff && scripts/run-tests.sh --bin tst_foundation_find_controller`
Expected: PASS (all slots, including the 2 new ones).

- [ ] **Step 6: Commit**

```bash
cd /home/clinton/dev/Markoff
git add libs/markoff-core/include/markoff/core/FindController.h \
        libs/markoff-core/src/FindController.cpp \
        libs/markoff-core/tests/tst_foundation_find_controller.cpp
git commit -m "feat(core): FindController::selectMatchAtOrAfter — mutation-free re-anchor

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Push Markoff

- [ ] **Step 1: Full suite green**

Run: `cd /home/clinton/dev/Markoff && scripts/run-tests.sh -E 'tst_realistic|tst_benchmark'`
Expected: baseline + new tests pass (the 3 known-red queue-#10 binaries excepted).

- [ ] **Step 2: Push**

```bash
cd /home/clinton/dev/Markoff
git push origin master
git rev-parse HEAD   # record this SHA for Phase 2
```

---

# Phase 2 — Corbomite submodule re-pin

### Task 4: Re-pin `libs/markoff-family`

**Files:**
- Modify: `/home/clinton/dev/Corbomite` gitlink for `libs/markoff-family`

- [ ] **Step 1: Move the submodule to the pushed master**

```bash
cd /home/clinton/dev/Corbomite/libs/markoff-family
git fetch origin && git checkout master && git pull --ff-only origin master
git rev-parse HEAD   # must equal the SHA from Task 3 Step 2
```

- [ ] **Step 2: Rebuild Corbomite**

Run: `cd /home/clinton/dev/Corbomite && cmake --build --preset dev -j 10`
Expected: clean build.

- [ ] **Step 3: Baseline regression check**

Run: `cd /home/clinton/dev/Corbomite/build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10`
Expected: **259/259** (unchanged — the re-pin only adds new API).

- [ ] **Step 4: Commit the gitlink**

```bash
cd /home/clinton/dev/Corbomite
git add libs/markoff-family
git commit -m "build: re-pin markoff-family for replaceMatches + selectMatchAtOrAfter

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

# Phase 3 — Corbomite Replace UI

### Task 5: `FindBar` replace row + `setReplaceMode`

**Files:**
- Modify: `src/editor/FindBar.h`
- Modify: `src/editor/FindBar.cpp`
- Modify test: `tests/editor/tst_findbar.cpp`

- [ ] **Step 1: Write the failing test**

Add these slot declarations to `TstFindBar` (after `closeButton_emitsCloseRequested();`):

```cpp
    void replaceMode_hiddenByDefault_shownWhenEnabled();
    void replaceButtons_emitSignals();
```

Add the slot definitions before `QTEST_MAIN`:

```cpp
void TstFindBar::replaceMode_hiddenByDefault_shownWhenEnabled()
{
    FindBar bar;
    auto *replaceEdit = bar.findChild<QLineEdit*>("findBarReplaceLineEdit");
    QVERIFY(replaceEdit != nullptr);
    QVERIFY(!replaceEdit->isVisibleTo(&bar));   // hidden until replace mode
    bar.setReplaceMode(true);
    QVERIFY(bar.isReplaceMode());
    QVERIFY(replaceEdit->isVisibleTo(&bar));
    bar.setReplaceMode(false);
    QVERIFY(!replaceEdit->isVisibleTo(&bar));
}

void TstFindBar::replaceButtons_emitSignals()
{
    FindBar bar;
    bar.setReplaceMode(true);
    auto *replaceEdit = bar.findChild<QLineEdit*>("findBarReplaceLineEdit");
    auto *replaceBtn  = bar.findChild<QPushButton*>("findBarReplace");
    auto *replaceAll  = bar.findChild<QPushButton*>("findBarReplaceAll");
    replaceEdit->setText("zzz");
    QSignalSpy replaceSpy(&bar, &FindBar::replaceRequested);
    QSignalSpy allSpy(&bar, &FindBar::replaceAllRequested);
    replaceBtn->click();
    replaceAll->click();
    QCOMPARE(replaceSpy.count(), 1);
    QCOMPARE(allSpy.count(), 1);
    QCOMPARE(bar.replacementText(), QString("zzz"));
}
```

- [ ] **Step 2: Run — verify it fails to compile**

Run: `cd /home/clinton/dev/Corbomite && cmake --build --preset dev -j 10 --target tst_findbar`
Expected: FAIL — `setReplaceMode`/`isReplaceMode`/`replacementText`/`replaceRequested` undeclared.

- [ ] **Step 3: Extend `FindBar.h`**

In the public section after `void focusLineEdit();` (line 31) add:

```cpp
    /// Toggle the replace row (a second row with a replacement field + Replace
    /// / Replace All). Hidden by default; find-only behavior is unchanged.
    void setReplaceMode(bool on);
    bool isReplaceMode() const { return m_replaceMode; }
    QString replacementText() const;
```

In the `Q_SIGNALS:` block after `void closeRequested();` add:

```cpp
    void replaceRequested();
    void replaceAllRequested();
```

In the private members block (after `QToolButton *m_closeButton = nullptr;`) add:

```cpp
    QWidget     *m_replaceRow       = nullptr;
    QLineEdit   *m_replaceLineEdit  = nullptr;
    QPushButton *m_replaceButton    = nullptr;
    QPushButton *m_replaceAllButton = nullptr;
    bool         m_replaceMode      = false;
```

- [ ] **Step 4: Restructure the layout in `FindBar.cpp`**

Replace the layout block (lines 51–59, the single `QHBoxLayout` added to `this`)
with a two-row vertical layout. Replace:

```cpp
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(4);
    layout->addWidget(m_closeButton);
    layout->addWidget(label);
    layout->addWidget(m_lineEdit, 1);
    layout->addWidget(m_countLabel);
    layout->addWidget(m_prevButton);
    layout->addWidget(m_nextButton);
```

with:

```cpp
    // --- find row ---
    auto *findRow = new QWidget(this);
    auto *findLayout = new QHBoxLayout(findRow);
    findLayout->setContentsMargins(0, 0, 0, 0);
    findLayout->setSpacing(4);
    findLayout->addWidget(m_closeButton);
    findLayout->addWidget(label);
    findLayout->addWidget(m_lineEdit, 1);
    findLayout->addWidget(m_countLabel);
    findLayout->addWidget(m_prevButton);
    findLayout->addWidget(m_nextButton);

    // --- replace row (hidden until setReplaceMode(true)) ---
    m_replaceRow = new QWidget(this);
    auto *replaceLabel = new QLabel(tr("Repla&ce:"), m_replaceRow);
    m_replaceLineEdit = new QLineEdit(m_replaceRow);
    m_replaceLineEdit->setObjectName(QStringLiteral("findBarReplaceLineEdit"));
    m_replaceLineEdit->setClearButtonEnabled(true);
    replaceLabel->setBuddy(m_replaceLineEdit);
    m_replaceButton = new QPushButton(tr("Replace"), m_replaceRow);
    m_replaceButton->setObjectName(QStringLiteral("findBarReplace"));
    m_replaceAllButton = new QPushButton(tr("Replace All"), m_replaceRow);
    m_replaceAllButton->setObjectName(QStringLiteral("findBarReplaceAll"));
    auto *replaceLayout = new QHBoxLayout(m_replaceRow);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(4);
    // Pad-left so the replace field aligns under the find field.
    replaceLayout->addSpacing(m_closeButton->sizeHint().width() + 4);
    replaceLayout->addWidget(replaceLabel);
    replaceLayout->addWidget(m_replaceLineEdit, 1);
    replaceLayout->addWidget(m_replaceButton);
    replaceLayout->addWidget(m_replaceAllButton);
    m_replaceRow->setVisible(false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(2);
    layout->addWidget(findRow);
    layout->addWidget(m_replaceRow);

    QObject::connect(m_replaceButton, &QPushButton::clicked,
                     this, &FindBar::replaceRequested);
    QObject::connect(m_replaceAllButton, &QPushButton::clicked,
                     this, &FindBar::replaceAllRequested);
```

Note: the existing `QHBoxLayout` include is already present; add `#include <QVBoxLayout>`
to the includes block.

- [ ] **Step 5: Implement the accessors in `FindBar.cpp`**

After `FindBar::focusLineEdit()` (line ~109) add:

```cpp
void FindBar::setReplaceMode(bool on)
{
    m_replaceMode = on;
    m_replaceRow->setVisible(on);
}

QString FindBar::replacementText() const
{
    return m_replaceLineEdit->text();
}
```

- [ ] **Step 6: Run — verify it passes**

Run: `cd /home/clinton/dev/Corbomite/build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_findbar --output-on-failure`
Expected: PASS (existing 11 slots + 2 new).

- [ ] **Step 7: Commit**

```bash
cd /home/clinton/dev/Corbomite
git add src/editor/FindBar.h src/editor/FindBar.cpp tests/editor/tst_findbar.cpp
git commit -m "feat(editor): FindBar replace row + setReplaceMode

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: `NoteEditorWidget` replace orchestration + `showReplaceBar`

**Files:**
- Modify: `src/editor/NoteEditorWidget.h`
- Modify: `src/editor/NoteEditorWidget.cpp`

`NoteEditorWidget` owns both the `FindBar` and the `NoteDocument`, so the replace
orchestration (which needs `markoff()` + the `FindController`) lives here, keeping
`FindBar` a UI-only shell.

- [ ] **Step 1: Declare in `NoteEditorWidget.h`**

After `void showFindBar();` (line 65) add:

```cpp
    void showReplaceBar();
```

In the private slots/methods area (near the existing find members) add:

```cpp
    void onReplaceRequested();
    void onReplaceAllRequested();
```

- [ ] **Step 2: Connect the FindBar signals where it is constructed**

In `NoteEditorWidget.cpp`, find where `m_findBar` is created and the
`closeRequested` connection is made (around line 60–64) and add:

```cpp
    connect(m_findBar, &FindBar::replaceRequested,
            this, &NoteEditorWidget::onReplaceRequested);
    connect(m_findBar, &FindBar::replaceAllRequested,
            this, &NoteEditorWidget::onReplaceAllRequested);
```

- [ ] **Step 3: Add `showReplaceBar` + replace slots, and ensure `showFindBar` resets mode**

In the `// --- Find UI ---` section, add to the END of `showFindBar()` body
(after `m_findBar->focusLineEdit();`) a mode reset so Ctrl+F never inherits a
prior replace state:

```cpp
    m_findBar->setReplaceMode(false);
```

Then add, after `hideFindBar()`:

```cpp
void NoteEditorWidget::showReplaceBar()
{
    showFindBar();                  // shares attach/activate/focus path
    m_findBar->setReplaceMode(true);
}

void NoteEditorWidget::onReplaceRequested()
{
    if (!m_doc) return;
    auto *fc = m_doc->findController();
    const int idx = fc->currentMatchIndex();
    if (idx < 0 || idx >= fc->matchCount()) return;
    const auto cur = fc->matches().at(idx);
    const QString repl = m_findBar->replacementText();

    m_doc->markoff()->replaceMatches(
        { Markoff::SearchHit{ cur.block, cur.byteOffset, cur.byteLength } },
        repl);
    // replaceMatches flushes synchronously, so fc has already recomputed.
    // Advance past the inserted replacement (avoids re-selecting a replacement
    // that itself contains the needle).
    fc->selectMatchAtOrAfter(cur.block,
                             cur.byteOffset + static_cast<quint32>(repl.toUtf8().size()));
}

void NoteEditorWidget::onReplaceAllRequested()
{
    if (!m_doc) return;
    auto *fc = m_doc->findController();
    QList<Markoff::SearchHit> hits;
    hits.reserve(fc->matchCount());
    for (const auto &m : fc->matches())
        hits.append(Markoff::SearchHit{ m.block, m.byteOffset, m.byteLength });
    m_doc->markoff()->replaceMatches(hits, m_findBar->replacementText());
}
```

Add `#include <markoff/core/SearchEngine.h>` to the includes block (for `SearchHit`).

- [ ] **Step 4: Build**

Run: `cd /home/clinton/dev/Corbomite && cmake --build --preset dev -j 10`
Expected: clean build.

- [ ] **Step 5: Regression check**

Run: `cd /home/clinton/dev/Corbomite/build-dev && QT_QPA_PLATFORM=offscreen ctest -R 'tst_findbar|tst_noteeditor' --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
cd /home/clinton/dev/Corbomite
git add src/editor/NoteEditorWidget.h src/editor/NoteEditorWidget.cpp
git commit -m "feat(editor): NoteEditorWidget replace orchestration + showReplaceBar

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

# Phase 4 — Menu wiring (placebo removal)

### Task 7: `MainWindow` Replace action + `onReplace`

**Files:**
- Modify: `src/app/MainWindow.h` (declare `onReplace`)
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Declare `onReplace`**

In `MainWindow.h`, next to `void onFind();` add `void onReplace();`.

- [ ] **Step 2: Implement `onReplace`**

In `MainWindow.cpp`, after `onFind()` (line ~665–670) add:

```cpp
void MainWindow::onReplace()
{
    auto *neWidget = activeEditor();
    if (!neWidget) return;
    neWidget->showReplaceBar();
}
```

- [ ] **Step 3: Register the standard Replace action**

In the find-actions setup, next to `KStandardAction::find(this, &MainWindow::onFind, ac);`
(line 1305) add:

```cpp
    KStandardAction::replace(this, &MainWindow::onReplace, ac);
```

- [ ] **Step 4: Build**

Run: `cd /home/clinton/dev/Corbomite && cmake --build --preset dev -j 10`
Expected: clean build. (Behavior is covered by ctest in Task 8 Step 5 and the
manual verification section; do not launch the GUI binary here.)

- [ ] **Step 5: Commit**

```bash
cd /home/clinton/dev/Corbomite
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat(app): wire KStandardAction::replace to showReplaceBar

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: Wire the hamburger Find… / Replace… placebos

The placebo `findAct` / `replaceAct` in `src/editor/MarkdownView.cpp` (lines
298–312) are created but never connected. Wire them to the host via a trigger
function, mirroring the existing `m_pdfExportTrigger` pattern.

**Files:**
- Modify: `src/editor/MarkdownView.h`
- Modify: `src/editor/MarkdownView.cpp`
- Modify: `src/app/MainWindow.cpp` (set the triggers)

- [ ] **Step 1: Declare triggers in `MarkdownView.h`**

After the `PdfExportTrigger` declarations (lines 72–73, 88) add:

```cpp
    using FindTrigger = std::function<void(QWidget *parent)>;
    void setFindTrigger(FindTrigger trigger);
    void setReplaceTrigger(FindTrigger trigger);
```

and in the private members (near `m_pdfExportTrigger`):

```cpp
    FindTrigger m_findTrigger;
    FindTrigger m_replaceTrigger;
```

- [ ] **Step 2: Implement the setters + connect the actions**

In `MarkdownView.cpp`, next to `setPdfExportTrigger` (line ~192) add:

```cpp
void MarkdownView::setFindTrigger(FindTrigger trigger)
{
    m_findTrigger = std::move(trigger);
}

void MarkdownView::setReplaceTrigger(FindTrigger trigger)
{
    m_replaceTrigger = std::move(trigger);
}
```

Replace the placebo `findAct`/`replaceAct` block (lines 298–312) — keep the
`QAction` creation but add the `connect` calls and drop the stale TODO comment:

```cpp
    auto *findAct = new QAction(
        QIcon::fromTheme(QStringLiteral("edit-find")),
        i18n("Find..."), this);
    connect(findAct, &QAction::triggered, this, [this] {
        if (m_findTrigger) m_findTrigger(this);
    });
    helper.addToSection(findAct, QStringLiteral("find"));

    auto *replaceAct = new QAction(
        QIcon::fromTheme(QStringLiteral("edit-find-replace")),
        i18n("Replace..."), this);
    connect(replaceAct, &QAction::triggered, this, [this] {
        if (m_replaceTrigger) m_replaceTrigger(this);
    });
    helper.addToSection(replaceAct, QStringLiteral("find"));
```

- [ ] **Step 3: Set the triggers from `MainWindow`**

In `MainWindow.cpp` where `mv->setPdfExportTrigger(...)` is wired (line ~1093),
add alongside it:

```cpp
        mv->setFindTrigger([this](QWidget *) { onFind(); });
        mv->setReplaceTrigger([this](QWidget *) { onReplace(); });
```

- [ ] **Step 4: Build**

Run: `cd /home/clinton/dev/Corbomite && cmake --build --preset dev -j 10`
Expected: clean build.

- [ ] **Step 5: Full regression**

Run: `cd /home/clinton/dev/Corbomite/build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10`
Expected: **≥259/259** (no regressions; new FindBar slots add to the count).

- [ ] **Step 6: Commit**

```bash
cd /home/clinton/dev/Corbomite
git add src/editor/MarkdownView.h src/editor/MarkdownView.cpp src/app/MainWindow.cpp
git commit -m "feat(editor): wire hamburger Find…/Replace… to host find/replace

Removes the find-section placebos (punch-list P3, 2026-06-10).

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

- [ ] **Step 7: Update punch list**

In `docs/punch-list.md` line 212, strike the Find…/Replace… clause of the
placebo item (leave the Insert Table/Callout clause — Part B below). Commit:

```bash
git add docs/punch-list.md
git commit -m "docs: mark hamburger Find/Replace placebo closed

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

# Manual verification (after Phase 4)

Offscreen ctest cannot drive real clicks. Before closing, eyeball in a real
session (per `~/dev/Markoff` rules use the Corbomite dev binary directly):

- [ ] Ctrl+F shows the find bar with NO replace row.
- [ ] Ctrl+R (KStandardAction::replace default) shows the bar WITH the replace row.
- [ ] Hamburger **Find…** / **Replace…** open the bar in the correct mode.
- [ ] Type a needle, type a replacement, **Replace** replaces the current match and
      advances to the next; **Replace All** replaces every match.
- [ ] A single Ctrl+Z (undo) reverses an entire Replace All.

---

# Part B — follow-on mechanical items (separate tasks; spec'd when reached)

These are independent of Replace and of each other. They are listed here with
concrete targets so they can be expanded into full TDD tasks (or a sibling plan)
without re-discovery. **Do not implement from this outline alone — expand each to
test-first tasks first.**

### B1: Insert Table / Insert Callout — apply the dialog result
- Site: `src/app/MainWindow.cpp` handlers around lines 505–530 (`InsertTableDialog dlg(this); if (dlg.exec() != QDialog::Accepted) return;` at 524–525) — the accepted result is currently discarded.
- Approach: take the dialog's produced markdown and insert it at the caret via
  `MarkoffDocument::applyFlatEdit`, reusing the template-at-cursor `LineResolve`
  caret→byte bridge shipped in `082589ae` (status-bar/template Phase 2 work).
- Enable already wired (`dialogActionIds`, line 557); on success, remove the
  "no-op until Markoff lands" comment.
- Test: a MainWindow-level or NoteDocument-level test asserting the document gains
  the table/callout markdown at the caret. (Needs the `InsertTableDialog` public
  accessor for its markdown — read `src/app/dialogs/InsertTableDialog.h` when expanding.)

### B2: Multi-snippet search results
- Site: `libs/storage/src/SQLiteIndex.cpp` ~line 461 — projection uses
  `snippet(notes_fts, 2, …)`, which yields exactly one snippet per file row.
- Approach: in the post-row loop, additionally scan the already-available
  `content` column (pulled in the `postFilter` path; pull it on the fast path too)
  for each positive FTS term occurrence and emit one `SearchMatch` per occurrence
  (cap at a sensible N per file; `log`/comment the cap). `SearchResultsModel`
  already groups by file, so multiple child rows render with no model change.
- Test: extend the SQLiteIndex search test with a multi-hit-in-one-file fixture,
  asserting >1 `SearchMatch` for that path.
- Note: this is the heaviest item — FTS5 `snippet()` does not give N snippets, so
  the occurrence scan is hand-rolled. Expand carefully; consider its own plan.
