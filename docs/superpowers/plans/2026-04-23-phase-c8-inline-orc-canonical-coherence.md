# Phase C8 — Inline-ORC Canonical Coherence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish a presentation-vs-content invariant in Markoff Live's per-block canonical bridge so that clicking an inline math formula (or toggling a checkbox, or interacting with a mermaid fence) never corrupts the canonical buffer or the on-disk file; and so that inbound canonical deltas splice cleanly into the local scene regardless of ORC structure.

**Architecture:** Two-layer fix: (1) in `libs/markoff-family/libs/markoff-live` + `libs/markoff-family/libs/markoff-core`, add a `PresentationScope` guard, a per-item substitution table + local↔canonical translators, outbound `U+FFFC` expansion, and debug `Q_ASSERT`s on the canonical buffer; (2) in `libs/vault`, a terminal `Vault::saveDocument` guard that refuses to write bytes containing `U+FFFC`. Inbound canonical deltas flatten their target item to source form, splice in source space, and re-substitute.

**Tech Stack:** Qt6.8 (`QTextDocument`, `QGraphicsItem`, `QTextCursor`, `QLoggingCategory`, QtTest), CMake, C++20, tree-sitter-based `markoff-parser`, KF6::SyntaxHighlighting.

**Spec references:**
- Primary: `libs/markoff-family/docs/specs/2026-04-23-inline-orc-canonical-coherence.md`
- Phase C3 addendum: `libs/markoff-family/docs/specs/2026-04-23-phase-c3-addendum-substitution-blind-spot.md`

**Prerequisites:**
- Markoff master at `df62ecf` (C8 specs landed) or later
- Corbomite master at `cbf08f2c` (submodule pin bumped) or later
- The exploratory `markoff.math.trace` logging currently in the working tree is unstaged. It is stripped entirely in Phase 7 Task 1; do not commit any trace code during earlier phases.

---

## File structure

### New files
- `libs/markoff-family/libs/markoff-live/src/PresentationScope.h` — RAII helper guarding presentation-layer mutations.
- `libs/markoff-family/libs/markoff-live/src/PresentationScope.cpp` — implementation.
- `libs/markoff-family/libs/markoff-live/src/Substitution.h` — `Substitution` struct + `SubstitutionTable` class + translator API.
- `libs/markoff-family/libs/markoff-live/src/Substitution.cpp` — implementation.
- `libs/markoff-family/libs/markoff-live/src/CanonicalSnapshot.h` — plain-data struct holding canonical cursor/selection/scroll.
- `libs/markoff-family/libs/markoff-live/tests/tst_substitution_table.cpp` — unit tests for translator.
- `libs/markoff-family/libs/markoff-live/tests/tst_math_click_canonical_coherence.cpp` — click-math-no-delta regression.
- `libs/markoff-family/libs/markoff-live/tests/tst_checkbox_toggle_canonical_coherence.cpp` — toggle-checkbox-no-delta regression.
- `libs/markoff-family/libs/markoff-live/tests/tst_mermaid_click_canonical_coherence.cpp` — click-mermaid-no-delta regression.
- `libs/markoff-family/libs/markoff-live/tests/tst_reveal_edit_collapse_roundtrip.cpp` — reveal/edit/collapse integrity.
- `libs/markoff-family/libs/markoff-live/tests/tst_edit_after_multiple_orcs.cpp` — offset-translation regression.
- `libs/markoff-family/libs/markoff-live/tests/tst_inbound_splice_preserves_cursor.cpp` — inbound splice preserves view state.
- `libs/markoff-family/libs/markoff-live/tests/tst_canonical_no_orc_invariant.cpp` — canonical buffer never holds `U+FFFC`.
- `libs/vault/tests/tst_vault_save_refuses_orc.cpp` — terminal guard refuses to write.

### Modified files
- `libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.h/.cpp` — `m_substitutions` field, `PresentationScope` migrations, new translator accessors, `snapshotViewStateAsCanonical` / `restoreViewStateFromCanonical`, `expandedTextForRange` helper.
- `libs/markoff-family/libs/markoff-live/src/SceneCoordinator.cpp` — `onLocalItemContentsChange` uses translator + expansion; `applyCanonicalDelta` single-item path rewritten strip-splice-reapply.
- `libs/markoff-family/libs/markoff-core/src/MarkoffDocument.cpp` — `Q_ASSERT`s on `applyCanonicalDelta`.
- `libs/markoff-family/libs/markoff-live/CMakeLists.txt` — new source files.
- `libs/markoff-family/libs/markoff-live/tests/CMakeLists.txt` — new test targets.
- `libs/vault/src/Vault.cpp` — terminal `U+FFFC` guard + new `saveFailed()` emission.
- `libs/vault/include/corbomite/core/NoteDocument.h` — `saveFailed()` signal.
- `libs/vault/tests/CMakeLists.txt` — new test target.
- `libs/markoff-family/docs/phase-c-status.md` — C8 status progression + closeout entries.
- `docs/PROJECT-STATE.md` — current-focus update after C8 closes.
- `docs/decisions-archive.md` — closeout entry after C8 closes.

### Tree-wide policy
- C++20. SPDX `GPL-3.0-or-later` on every new file.
- Build dir: `build` (Corbomite top-level) per `CLAUDE.md`. Always `cmake --build build -j 10`.
- Test run: `cd build && ctest -R <name> --output-on-failure -j 10`.
- Dev flag: `-DCORBOMITE_DEV_BUILD=ON` for any app-launching smoke.
- Submodule commits go into `libs/markoff-family` first; the Corbomite-side submodule pin bump is its own commit per `CONTRIBUTING-OPS.md` Ritual 5.

---

## Phase 1 — PresentationScope helper

Goal: single RAII type that every ORC-touching mutation wraps, so future paths cannot forget to block signals. Behavior-preserving migration of existing signal-blocking call sites.

### Task 1.1: Create the PresentationScope header

**Files:**
- Create: `libs/markoff-family/libs/markoff-live/src/PresentationScope.h`

- [ ] **Step 1: Write the header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_PRESENTATIONSCOPE_H
#define MARKOFF_PRESENTATIONSCOPE_H

#include <QPointer>

class QTextDocument;

namespace Markoff {

class MarkdownTextItem;

/// RAII helper for presentation-layer mutations in a MarkdownTextItem.
///
/// Presentation-layer mutations are reveal/collapse/strip/apply/toggle
/// operations that swap a U+FFFC glyph for its raw source text (or vice
/// versa). They are purely visual — the canonical MarkoffDocument buffer
/// already holds the source form and MUST NOT be notified.
///
/// Wrap every such mutation in a PresentationScope. The scope:
///   1. Sets MarkdownTextItem::m_inSubstitution so re-entrant callers
///      (apply firing while reveal is running, etc.) short-circuit.
///   2. Calls QTextDocument::blockSignals(true) on the item's document
///      so contentsChange does not fire through to SceneCoordinator's
///      outbound bridge.
/// On destruction, both are restored to their previous values.
///
/// Construct with the MarkdownTextItem pointer. Scope is a stack object.
class PresentationScope {
public:
    explicit PresentationScope(MarkdownTextItem *item);
    ~PresentationScope();

    PresentationScope(const PresentationScope &) = delete;
    PresentationScope &operator=(const PresentationScope &) = delete;
    PresentationScope(PresentationScope &&) = delete;
    PresentationScope &operator=(PresentationScope &&) = delete;

private:
    QPointer<MarkdownTextItem> m_item;
    bool m_prevInSubst;
    bool m_prevBlocked;
};

} // namespace Markoff

#endif // MARKOFF_PRESENTATIONSCOPE_H
```

- [ ] **Step 2: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/PresentationScope.h
git commit -m "markoff-live: add PresentationScope RAII header (C8 Task 1.1)"
```

### Task 1.2: Create the PresentationScope implementation

**Files:**
- Create: `libs/markoff-family/libs/markoff-live/src/PresentationScope.cpp`

- [ ] **Step 1: Write the implementation**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "PresentationScope.h"
#include "MarkdownTextItem.h"

#include <QTextDocument>

namespace Markoff {

PresentationScope::PresentationScope(MarkdownTextItem *item)
    : m_item(item)
    , m_prevInSubst(false)
    , m_prevBlocked(false)
{
    if (!m_item) return;
    m_prevInSubst = m_item->isInSubstitution();
    m_item->setInSubstitution(true);
    QTextDocument *doc = m_item->document();
    if (doc) {
        m_prevBlocked = doc->blockSignals(true);
    }
}

PresentationScope::~PresentationScope()
{
    if (!m_item) return;
    QTextDocument *doc = m_item->document();
    if (doc) {
        doc->blockSignals(m_prevBlocked);
    }
    m_item->setInSubstitution(m_prevInSubst);
}

} // namespace Markoff
```

- [ ] **Step 2: Commit (build failing is fine — accessors added in 1.3)**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/PresentationScope.cpp
git commit -m "markoff-live: add PresentationScope RAII impl (C8 Task 1.2)"
```

### Task 1.3: Expose isInSubstitution/setInSubstitution on MarkdownTextItem

**Files:**
- Modify: `libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.h`

- [ ] **Step 1: Find the `m_inSubstitution` field declaration**

Run: `grep -n "m_inSubstitution" libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.h`
Expected: one or two matches on a private member.

- [ ] **Step 2: Add public accessors next to the existing public API**

Locate the `public:` section of `class MarkdownTextItem` that contains `QTextDocument *document() const;` (grep for `QTextDocument \*document`). Add immediately after it:

```cpp
    /// True while a PresentationScope is active (see PresentationScope.h).
    /// Existing call sites in MarkdownTextItem.cpp use m_inSubstitution as
    /// a re-entrance guard; PresentationScope toggles it via these accessors.
    bool isInSubstitution() const { return m_inSubstitution; }
    void setInSubstitution(bool v) { m_inSubstitution = v; }
```

- [ ] **Step 3: Build to verify**

Run: `cmake --build build -j 10 --target markoff_live 2>&1 | tail -5`
Expected: `[100%] Built target markoff_live` (no errors).

- [ ] **Step 4: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/MarkdownTextItem.h
git commit -m "markoff-live: expose isInSubstitution accessors for PresentationScope (C8 Task 1.3)"
```

### Task 1.4: Register PresentationScope.cpp in CMake

**Files:**
- Modify: `libs/markoff-family/libs/markoff-live/CMakeLists.txt`

- [ ] **Step 1: Locate the source-file list**

Run: `grep -n "MarkdownTextItem.cpp" libs/markoff-family/libs/markoff-live/CMakeLists.txt`
Expected: one match inside a `set(` or `target_sources(` list.

- [ ] **Step 2: Add PresentationScope.cpp alongside MarkdownTextItem.cpp**

In the same list (alphabetically positioned if the list is sorted, else adjacent to `MarkdownTextItem.cpp`), add:

```cmake
    src/PresentationScope.cpp
    src/PresentationScope.h
```

(Match the existing indentation and whether headers are listed — if the existing list omits headers, only add the `.cpp`.)

- [ ] **Step 3: Reconfigure + build**

Run:
```bash
cmake --build build -j 10 --target markoff_live 2>&1 | tail -3
```
Expected: `[100%] Built target markoff_live`.

- [ ] **Step 4: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/CMakeLists.txt
git commit -m "markoff-live: register PresentationScope.cpp (C8 Task 1.4)"
```

### Task 1.5: Migrate stripInlineSubstitutions to PresentationScope

**Files:**
- Modify: `libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.cpp`

- [ ] **Step 1: Add the include**

At the top of `MarkdownTextItem.cpp`, inside the existing include block (after `#include "TextControl.h"`), add:

```cpp
#include "PresentationScope.h"
```

- [ ] **Step 2: Find the strip body and wrap in a scope**

Find `int MarkdownTextItem::stripInlineSubstitutions()`. The body that sets `m_inSubstitution = true;` through `m_inSubstitution = false;` (including the existing `blockSignals(true)/(blocked)` pair) is the existing guard block.

Replace:
```cpp
    m_inSubstitution = true;
    const bool blocked = m_document->blockSignals(true);
    int delta = 0;
    QTextCursor cursor(m_document);
    cursor.beginEditBlock();
    for (int i = hits.size() - 1; i >= 0; --i) {
        const Hit &h = hits[i];
        cursor.setPosition(h.pos);
        cursor.setPosition(h.pos + 1, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(h.raw);
        delta += h.raw.size() - 1;
    }
    cursor.endEditBlock();
    m_document->blockSignals(blocked);
    m_inSubstitution = false;
    return delta;
```

With:
```cpp
    int delta = 0;
    {
        PresentationScope scope(this);
        QTextCursor cursor(m_document);
        cursor.beginEditBlock();
        for (int i = hits.size() - 1; i >= 0; --i) {
            const Hit &h = hits[i];
            cursor.setPosition(h.pos);
            cursor.setPosition(h.pos + 1, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText(h.raw);
            delta += h.raw.size() - 1;
        }
        cursor.endEditBlock();
    }
    return delta;
```

- [ ] **Step 3: Build and run existing strip-related tests**

Run:
```bash
cmake --build build -j 10 --target markoff_live 2>&1 | tail -3
cd build && ctest -R "markoff_inline_math|markoff_mermaid_substitution" --output-on-failure
cd ..
```
Expected: all matched tests still pass (behavior preserved).

- [ ] **Step 4: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/MarkdownTextItem.cpp
git commit -m "markoff-live: migrate stripInlineSubstitutions to PresentationScope (C8 Task 1.5)"
```

### Task 1.6: Migrate applyInlineSubstitutions to PresentationScope

**Files:**
- Modify: `libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.cpp`

- [ ] **Step 1: Find the apply body**

Find `void MarkdownTextItem::applyInlineSubstitutions()`. Locate the block that sets `m_inSubstitution = true;` and the paired `m_inSubstitution = false;`. The block also has a `const bool blocked = m_document->blockSignals(true);` near the cursor-edit-block.

- [ ] **Step 2: Replace with a PresentationScope wrapping the mutation region**

Change:
```cpp
    m_inSubstitution = true;
    // ...
    const bool blocked = m_document->blockSignals(true);
    QTextCursor cursor(m_document);
    cursor.beginEditBlock();
    // ...edit loop...
    cursor.endEditBlock();
    m_document->blockSignals(blocked);
    // ...span-adjustment code remaining OUTSIDE the scope, it does not
    //   mutate the document...
    m_inSubstitution = false;
```

To:
```cpp
    {
        PresentationScope scope(this);
        QTextCursor cursor(m_document);
        cursor.beginEditBlock();
        // ...edit loop (unchanged)...
        cursor.endEditBlock();
    }
    // ...span-adjustment code unchanged (runs outside the scope so the
    //   highlighter can observe the post-mutation document if it needs to)...
```

Keep `m_sourcePositionSpans = hl->spans();` and the span-adjustment loop outside the scope — they do not mutate the document.

- [ ] **Step 3: Build and run apply-related tests**

Run:
```bash
cmake --build build -j 10 --target markoff_live 2>&1 | tail -3
cd build && ctest -R "markoff_inline_math|markoff_mermaid_substitution|markoff_scene_offset_map" --output-on-failure
cd ..
```
Expected: all matched tests pass.

- [ ] **Step 4: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/MarkdownTextItem.cpp
git commit -m "markoff-live: migrate applyInlineSubstitutions to PresentationScope (C8 Task 1.6)"
```

### Task 1.7: Wrap updateReveal Case 2 (collapse) in PresentationScope

**Files:**
- Modify: `libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.cpp`

- [ ] **Step 1: Find Case 2**

In `void MarkdownTextItem::updateReveal()`, find the block that begins with the `if (!latex.isEmpty()) {` line and currently contains:

```cpp
                m_inSubstitution = true;
                c.beginEditBlock();
                c.removeSelectedText();
                c.insertText(QString(QChar::ObjectReplacementCharacter), fmt);
                c.endEditBlock();
                m_inSubstitution = false;
```

This is the **currently-unguarded** collapse path — it does not block signals. Replace with:

```cpp
                {
                    PresentationScope scope(this);
                    c.beginEditBlock();
                    c.removeSelectedText();
                    c.insertText(QString(QChar::ObjectReplacementCharacter), fmt);
                    c.endEditBlock();
                }
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j 10 --target markoff_live 2>&1 | tail -3`
Expected: green.

- [ ] **Step 3: Commit (no test change yet — Phase 6 adds the pinning tests)**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/MarkdownTextItem.cpp
git commit -m "markoff-live: guard updateReveal Case 2 collapse with PresentationScope (C8 Task 1.7)"
```

### Task 1.8: Wrap updateReveal Case 3 (expand) in PresentationScope

**Files:**
- Modify: `libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.cpp`

- [ ] **Step 1: Find Case 3**

In `updateReveal()`, locate the block beginning `m_inSubstitution = true;` that immediately precedes `c.setCharFormat(plain);` and ends with `m_inSubstitution = false;`. Replace:

```cpp
    m_inSubstitution = true;
    QTextCursor c(m_document);
    c.setPosition(glyphPos);
    c.setPosition(glyphPos + 1, QTextCursor::KeepAnchor);
    QTextCharFormat plain;
    c.setCharFormat(plain);
    c.beginEditBlock();
    c.removeSelectedText();
    c.insertText(raw);
    c.endEditBlock();
    m_inSubstitution = false;
```

With:

```cpp
    {
        PresentationScope scope(this);
        QTextCursor c(m_document);
        c.setPosition(glyphPos);
        c.setPosition(glyphPos + 1, QTextCursor::KeepAnchor);
        QTextCharFormat plain;
        c.setCharFormat(plain);
        c.beginEditBlock();
        c.removeSelectedText();
        c.insertText(raw);
        c.endEditBlock();
    }
```

Leave the subsequent `m_revealedStart = glyphPos;` etc. outside the scope (they don't mutate the document, and they need to persist after the scope destructs).

- [ ] **Step 2: Build**

Run: `cmake --build build -j 10 --target markoff_live 2>&1 | tail -3`
Expected: green.

- [ ] **Step 3: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/MarkdownTextItem.cpp
git commit -m "markoff-live: guard updateReveal Case 3 expand with PresentationScope (C8 Task 1.8 — stops reveal leakage)"
```

### Task 1.9: Wrap checkbox-toggle in PresentationScope

**Files:**
- Modify: `libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.cpp`

- [ ] **Step 1: Find the checkbox block in mousePressEvent**

In `void MarkdownTextItem::mousePressEvent(...)`, locate the block that sets a new QTextCharFormat on the checkbox glyph and currently uses `const bool blocked = m_document->blockSignals(true); ... m_document->blockSignals(blocked);`.

Replace:
```cpp
            const bool blocked = m_document->blockSignals(true);
            c.setCharFormat(newFmt);
            m_document->blockSignals(blocked);
```

With:
```cpp
            {
                PresentationScope scope(this);
                c.setCharFormat(newFmt);
            }
```

- [ ] **Step 2: Build and run existing checkbox tests**

Run:
```bash
cmake --build build -j 10 --target markoff_live 2>&1 | tail -3
cd build && ctest -R "markoff_checkbox" --output-on-failure
cd ..
```
Expected: existing checkbox tests pass.

- [ ] **Step 3: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/MarkdownTextItem.cpp
git commit -m "markoff-live: guard checkbox-toggle with PresentationScope (C8 Task 1.9)"
```

---

## Phase 2 — Substitution table + translators (TDD, unit-tested first)

Goal: introduce the `Substitution` struct and `SubstitutionTable` class with `localToCanonical`, `canonicalToLocal`, `localRangeToCanonical`. Build the test first, then the implementation.

### Task 2.1: Create Substitution.h with the data types

**Files:**
- Create: `libs/markoff-family/libs/markoff-live/src/Substitution.h`

- [ ] **Step 1: Write the header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_SUBSTITUTION_H
#define MARKOFF_SUBSTITUTION_H

#include <QList>

namespace Markoff {

/// One entry in the per-item substitution table.
///
/// Each entry records a U+FFFC glyph's position in the local QTextDocument
/// and the length of the raw source text the glyph stands in for.
///
/// Invariant: localPos is the document position of the U+FFFC character
/// itself; rawLen is the canonical source length (always > 1 for a valid
/// substitution — rawLen == 1 would be a pointless substitution).
struct Substitution {
    int localPos = 0;
    int rawLen = 0;
};

/// Translator table for local↔canonical position conversion within a single
/// MarkdownTextItem's local QTextDocument.
///
/// Each MarkdownTextItem owns one SubstitutionTable. It is rebuilt at the
/// end of applyInlineSubstitutions() and cleared by stripInlineSubstitutions().
/// The table MUST be sorted ascending by localPos; insertion helpers enforce
/// this.
class SubstitutionTable {
public:
    void clear() { m_entries.clear(); }
    int size() const { return m_entries.size(); }
    bool isEmpty() const { return m_entries.isEmpty(); }
    const QList<Substitution> &entries() const { return m_entries; }

    /// Append in sorted order. Caller guarantees monotonically non-decreasing
    /// localPos (rebuild walks the document in order).
    void append(Substitution s) { m_entries.append(s); }

    /// Translate a local QTextDocument position to the corresponding
    /// canonical source offset within the same block. Sums (rawLen − 1)
    /// for every substitution whose localPos is strictly less than the
    /// input position.
    int localToCanonical(int localPos) const;

    /// Inverse of localToCanonical. Subtracts (rawLen − 1) for every
    /// substitution whose canonical range ends at or before canonicalPos.
    /// If canonicalPos lands inside an ORC's canonical span, returns the
    /// local position of the ORC glyph itself.
    int canonicalToLocal(int canonicalPos) const;

    /// Translate a local range [localPos, localPos + localLen) to a
    /// canonical range [canonicalStart, canonicalStart + canonicalLen).
    /// canonicalLen >= localLen because the range may contain ORCs whose
    /// canonical raw length exceeds their single-char local footprint.
    /// Returns {canonicalStart, canonicalLen}.
    struct CanonicalRange { int start = 0; int length = 0; };
    CanonicalRange localRangeToCanonical(int localPos, int localLen) const;

    /// Return true if canonicalPos lands inside an ORC's canonical span
    /// (i.e. strictly between the ORC's canonical start and canonical end).
    bool isInsideOrcCanonicalSpan(int canonicalPos) const;

private:
    QList<Substitution> m_entries;
};

} // namespace Markoff

#endif // MARKOFF_SUBSTITUTION_H
```

- [ ] **Step 2: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/Substitution.h
git commit -m "markoff-live: add Substitution + SubstitutionTable header (C8 Task 2.1)"
```

### Task 2.2: Write failing unit tests for SubstitutionTable

**Files:**
- Create: `libs/markoff-family/libs/markoff-live/tests/tst_substitution_table.cpp`

- [ ] **Step 1: Write the test**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include "Substitution.h"

using namespace Markoff;

class TstSubstitutionTable : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void emptyTable_isIdentity();
    void singleSubstitution_localToCanonical();
    void singleSubstitution_canonicalToLocal();
    void multipleSubstitutions_localToCanonical_atEachBoundary();
    void multipleSubstitutions_canonicalToLocal_atEachBoundary();
    void localRangeToCanonical_rangeContainsOrc();
    void localRangeToCanonical_rangeOutsideAllOrcs();
    void isInsideOrcCanonicalSpan_detects_strict_interior();
};

void TstSubstitutionTable::emptyTable_isIdentity() {
    SubstitutionTable t;
    QCOMPARE(t.localToCanonical(0), 0);
    QCOMPARE(t.localToCanonical(100), 100);
    QCOMPARE(t.canonicalToLocal(0), 0);
    QCOMPARE(t.canonicalToLocal(100), 100);
}

void TstSubstitutionTable::singleSubstitution_localToCanonical() {
    // Source: "abc$E=mc^2$def" (len 14), with "$E=mc^2$" at canonical [3,11).
    // After substitution: "abc⟨ORC⟩def" (len 7), ORC at local 3.
    SubstitutionTable t;
    t.append({3, 8});  // localPos=3, rawLen=8 ("$E=mc^2$")

    // Positions before/at ORC: no shift.
    QCOMPARE(t.localToCanonical(0), 0);
    QCOMPARE(t.localToCanonical(3), 3);
    // Position after the ORC glyph (local 4 == just past ORC == canonical 11).
    QCOMPARE(t.localToCanonical(4), 11);
    QCOMPARE(t.localToCanonical(7), 14);
}

void TstSubstitutionTable::singleSubstitution_canonicalToLocal() {
    SubstitutionTable t;
    t.append({3, 8});  // ORC at local 3, canonical [3,11).

    QCOMPARE(t.canonicalToLocal(0), 0);
    QCOMPARE(t.canonicalToLocal(3), 3);   // start of ORC span
    QCOMPARE(t.canonicalToLocal(11), 4);  // just past ORC span
    QCOMPARE(t.canonicalToLocal(14), 7);  // end of doc
}

void TstSubstitutionTable::multipleSubstitutions_localToCanonical_atEachBoundary() {
    // Two ORCs: $a$ at canonical [0,3), and $b$ at canonical [5,8).
    // After substitution: "⟨ORC⟩xx⟨ORC⟩" (len 4), ORCs at local 0 and 3.
    SubstitutionTable t;
    t.append({0, 3});
    t.append({3, 3});

    QCOMPARE(t.localToCanonical(0), 0);  // first ORC start
    QCOMPARE(t.localToCanonical(1), 3);  // just past first ORC
    QCOMPARE(t.localToCanonical(2), 4);
    QCOMPARE(t.localToCanonical(3), 5);  // second ORC start
    QCOMPARE(t.localToCanonical(4), 8);  // just past second ORC
}

void TstSubstitutionTable::multipleSubstitutions_canonicalToLocal_atEachBoundary() {
    SubstitutionTable t;
    t.append({0, 3});
    t.append({3, 3});

    QCOMPARE(t.canonicalToLocal(0), 0);
    QCOMPARE(t.canonicalToLocal(3), 1);  // just past first ORC in canonical
    QCOMPARE(t.canonicalToLocal(4), 2);
    QCOMPARE(t.canonicalToLocal(5), 3);  // second ORC start in canonical
    QCOMPARE(t.canonicalToLocal(8), 4);  // just past second ORC in canonical
}

void TstSubstitutionTable::localRangeToCanonical_rangeContainsOrc() {
    // ORC at local 3, canonical [3,11).
    SubstitutionTable t;
    t.append({3, 8});

    // Local range [2, 5): covers 1 char before ORC, the ORC (1 local char),
    // and 1 char after. Canonical range: start=2, length = 2→2 before,
    // +8 (ORC raw) + 1 (after) = canonical length 10.
    auto r = t.localRangeToCanonical(2, 3);
    QCOMPARE(r.start, 2);
    QCOMPARE(r.length, 10);
}

void TstSubstitutionTable::localRangeToCanonical_rangeOutsideAllOrcs() {
    SubstitutionTable t;
    t.append({3, 8});

    // Local range [5, 7) (after ORC, entirely outside). Canonical length
    // equals local length.
    auto r = t.localRangeToCanonical(5, 2);
    QCOMPARE(r.start, 12);   // local 5 → canonical 12
    QCOMPARE(r.length, 2);
}

void TstSubstitutionTable::isInsideOrcCanonicalSpan_detects_strict_interior() {
    // ORC at local 3, canonical [3,11).
    SubstitutionTable t;
    t.append({3, 8});

    QVERIFY(!t.isInsideOrcCanonicalSpan(2));  // before ORC
    QVERIFY(!t.isInsideOrcCanonicalSpan(3));  // boundary is NOT "inside"
    QVERIFY(t.isInsideOrcCanonicalSpan(4));   // interior
    QVERIFY(t.isInsideOrcCanonicalSpan(10));  // interior
    QVERIFY(!t.isInsideOrcCanonicalSpan(11)); // boundary is NOT "inside"
    QVERIFY(!t.isInsideOrcCanonicalSpan(12)); // after ORC
}

QTEST_MAIN(TstSubstitutionTable)
#include "tst_substitution_table.moc"
```

- [ ] **Step 2: Register the test in CMake**

Edit `libs/markoff-family/libs/markoff-live/tests/CMakeLists.txt`. Find an existing `add_markoff_live_test(` or `qt_add_test(` invocation for a small unit test (e.g. `tst_checkbox_text_object`). Add an analogous entry:

```cmake
add_markoff_live_test(tst_substitution_table tst_substitution_table.cpp)
```

(If the helper has a different name — grep for it — use that name. Match the style of the closest existing unit test.)

- [ ] **Step 3: Configure + attempt build; expect linker failure**

Run:
```bash
cmake --build build -j 10 --target tst_substitution_table 2>&1 | tail -8
```
Expected: undefined references to `Markoff::SubstitutionTable::localToCanonical` et al — no `Substitution.cpp` exists yet. Good.

- [ ] **Step 4: Commit the failing test**

```bash
cd libs/markoff-family
git add libs/markoff-live/tests/tst_substitution_table.cpp libs/markoff-live/tests/CMakeLists.txt
git commit -m "markoff-live: add failing SubstitutionTable unit tests (C8 Task 2.2)"
```

### Task 2.3: Implement SubstitutionTable to make the tests pass

**Files:**
- Create: `libs/markoff-family/libs/markoff-live/src/Substitution.cpp`

- [ ] **Step 1: Write the implementation**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Substitution.h"

namespace Markoff {

int SubstitutionTable::localToCanonical(int localPos) const {
    int shift = 0;
    for (const Substitution &s : m_entries) {
        if (s.localPos < localPos) {
            shift += s.rawLen - 1;
        } else {
            break;
        }
    }
    return localPos + shift;
}

int SubstitutionTable::canonicalToLocal(int canonicalPos) const {
    int canonicalShift = 0;
    for (const Substitution &s : m_entries) {
        const int canonicalStart = s.localPos + canonicalShift;
        const int canonicalEnd = canonicalStart + s.rawLen;
        if (canonicalPos < canonicalStart) {
            // Position lies before this substitution — final shift is accumulated shift.
            return canonicalPos - canonicalShift;
        }
        if (canonicalPos < canonicalEnd) {
            // Inside the ORC's canonical span — return the ORC glyph's local position.
            return s.localPos;
        }
        canonicalShift += s.rawLen - 1;
    }
    return canonicalPos - canonicalShift;
}

SubstitutionTable::CanonicalRange
SubstitutionTable::localRangeToCanonical(int localPos, int localLen) const {
    const int start = localToCanonical(localPos);
    const int end = localToCanonical(localPos + localLen);
    return { start, end - start };
}

bool SubstitutionTable::isInsideOrcCanonicalSpan(int canonicalPos) const {
    int canonicalShift = 0;
    for (const Substitution &s : m_entries) {
        const int canonicalStart = s.localPos + canonicalShift;
        const int canonicalEnd = canonicalStart + s.rawLen;
        if (canonicalPos <= canonicalStart) return false;
        if (canonicalPos < canonicalEnd) return true;
        canonicalShift += s.rawLen - 1;
    }
    return false;
}

} // namespace Markoff
```

- [ ] **Step 2: Register Substitution.cpp in the markoff-live CMake source list**

Edit `libs/markoff-family/libs/markoff-live/CMakeLists.txt`. Add `src/Substitution.cpp` alongside `src/PresentationScope.cpp`.

- [ ] **Step 3: Build the test**

Run:
```bash
cmake --build build -j 10 --target tst_substitution_table 2>&1 | tail -3
```
Expected: `[100%] Built target tst_substitution_table`.

- [ ] **Step 4: Run the test**

Run:
```bash
cd build && ctest -R "substitution_table" --output-on-failure && cd ..
```
Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 5: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/Substitution.cpp libs/markoff-live/CMakeLists.txt
git commit -m "markoff-live: implement SubstitutionTable translator (C8 Task 2.3)"
```

### Task 2.4: Wire m_substitutions into MarkdownTextItem

**Files:**
- Modify: `libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.h/.cpp`

- [ ] **Step 1: Add the include + field in the header**

In `MarkdownTextItem.h`, add near the top with other includes:
```cpp
#include "Substitution.h"
```

Add in the `private:` section, alongside `m_inSubstitution`:
```cpp
    SubstitutionTable m_substitutions;
```

Add in the `public:` section (next to `isInSubstitution`):
```cpp
    /// The per-item substitution table. Rebuilt by applyInlineSubstitutions,
    /// cleared by stripInlineSubstitutions. SceneCoordinator consults it
    /// to translate between local and canonical positions.
    const SubstitutionTable &substitutions() const { return m_substitutions; }
```

- [ ] **Step 2: Clear the table inside stripInlineSubstitutions**

In `MarkdownTextItem::stripInlineSubstitutions`, add immediately **after** the `PresentationScope` block closes:
```cpp
    m_substitutions.clear();
```

- [ ] **Step 3: Rebuild the table at the tail of applyInlineSubstitutions**

At the very end of `applyInlineSubstitutions()`, after `m_revealedIsDisplay = newRevealedIsDisplay;`, add:
```cpp
    // Rebuild the substitution table from the now-substituted document.
    m_substitutions.clear();
    for (QTextBlock block = m_document->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            const QTextCharFormat fmt = frag.charFormat();
            const QString raw = fmt.property(MathTextObject::RawProperty).toString();
            if (raw.isEmpty()) continue;
            const QString text = frag.text();
            for (int i = 0; i < text.size(); ++i) {
                if (text.at(i) == QChar::ObjectReplacementCharacter) {
                    m_substitutions.append({ frag.position() + i, int(raw.size()) });
                }
            }
        }
    }
```

(If the apply function returns early on `entries.isEmpty()`, the table stays cleared — correct state when there are no substitutions.)

- [ ] **Step 4: Build and run existing tests**

Run:
```bash
cmake --build build -j 10 --target markoff_live 2>&1 | tail -3
cd build && ctest -R "markoff_inline_math|markoff_scene_offset_map|markoff_checkbox" --output-on-failure && cd ..
```
Expected: green.

- [ ] **Step 5: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/MarkdownTextItem.h libs/markoff-live/src/MarkdownTextItem.cpp
git commit -m "markoff-live: maintain SubstitutionTable on strip/apply (C8 Task 2.4)"
```

---

## Phase 3 — Outbound bridge + insertedText expansion

Goal: rewrite `SceneCoordinator::onLocalItemContentsChange` to use the translator and expand `U+FFFC` in `insertedText` before pushing a canonical delta.

### Task 3.1: Add expandedTextForRange helper to MarkdownTextItem

**Files:**
- Modify: `libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.h/.cpp`

- [ ] **Step 1: Declare in the header**

Add to the `public:` section of `MarkdownTextItem`:
```cpp
    /// Return the text in the local document between [localPos, localPos+localLen),
    /// expanding any U+FFFC fragments to their RawProperty-stored raw source.
    /// The returned string contains no U+FFFC characters.
    QString expandedTextForRange(int localPos, int localLen) const;
```

- [ ] **Step 2: Implement in the .cpp**

Add below the existing `allMarkdown()` implementation:

```cpp
QString MarkdownTextItem::expandedTextForRange(int localPos, int localLen) const
{
    if (localLen <= 0) return {};
    QString out;
    out.reserve(localLen);

    const int endPos = localPos + localLen;
    for (QTextBlock block = m_document->begin(); block.isValid(); block = block.next()) {
        const int blockStart = block.position();
        const int blockEnd = blockStart + block.length() - 1;  // exclude trailing paragraph separator
        if (blockEnd < localPos) continue;
        if (blockStart >= endPos) break;

        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            const int fragStart = frag.position();
            const int fragEnd = fragStart + frag.length();
            if (fragEnd <= localPos) continue;
            if (fragStart >= endPos) break;

            const QTextCharFormat fmt = frag.charFormat();
            const QString fragText = frag.text();
            const int sliceStart = qMax(0, localPos - fragStart);
            const int sliceEnd = qMin(fragText.size(), endPos - fragStart);

            const QString raw = fmt.property(MathTextObject::RawProperty).toString();
            if (!raw.isEmpty()) {
                for (int i = sliceStart; i < sliceEnd; ++i) {
                    if (fragText.at(i) == QChar::ObjectReplacementCharacter) {
                        out += raw;
                    } else {
                        out += fragText.at(i);
                    }
                }
            } else {
                for (int i = sliceStart; i < sliceEnd; ++i) {
                    const QChar c = fragText.at(i);
                    if (c != QChar::ObjectReplacementCharacter) {
                        out += c;
                    }
                }
            }
        }

        // Preserve the paragraph-separator between blocks as '\n'
        if (blockEnd < endPos && block.next().isValid()) {
            out += QLatin1Char('\n');
        }
    }
    return out;
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j 10 --target markoff_live 2>&1 | tail -3`
Expected: green.

- [ ] **Step 4: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/MarkdownTextItem.h libs/markoff-live/src/MarkdownTextItem.cpp
git commit -m "markoff-live: add MarkdownTextItem::expandedTextForRange helper (C8 Task 3.1)"
```

### Task 3.2: Rewrite onLocalItemContentsChange with translator + expansion

**Files:**
- Modify: `libs/markoff-family/libs/markoff-live/src/SceneCoordinator.cpp`

- [ ] **Step 1: Locate the function**

Run: `grep -n "onLocalItemContentsChange" libs/markoff-family/libs/markoff-live/src/SceneCoordinator.cpp`
Expected: declaration + definition.

- [ ] **Step 2: Replace the body**

Replace the current implementation of `SceneCoordinator::onLocalItemContentsChange(int itemIndex, int localPos, int charsRemoved, int charsAdded)` with:

```cpp
void SceneCoordinator::onLocalItemContentsChange(int itemIndex, int localPos,
                                                  int charsRemoved, int charsAdded)
{
    if (m_applyingCanonicalDelta) return;
    if (!m_boundDoc) return;
    if (itemIndex < 0 || itemIndex >= m_itemMap.size()) return;

    const auto &entry = m_itemMap[itemIndex];
    if (!entry.item || !entry.item->isTextItem()) return;

    auto *mti = static_cast<MarkdownTextItem *>(entry.item);

    // Translate local → canonical through the substitution table.
    const auto canonicalRange =
        mti->substitutions().localRangeToCanonical(localPos, charsRemoved);
    const qsizetype canonicalOffset = entry.canonicalStart + canonicalRange.start;
    const qsizetype canonicalRemoved = canonicalRange.length;

    QString insertedText;
    if (charsAdded > 0) {
        insertedText = mti->expandedTextForRange(localPos, charsAdded);
        insertedText.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    }

    // Debug invariant: after expansion, insertedText must not contain U+FFFC.
    Q_ASSERT(!insertedText.contains(QChar::ObjectReplacementCharacter));

    m_applyingCanonicalDelta = true;
    m_boundDoc->undoStack()->push(
        new Markoff::MarkdownDelta(m_boundDoc,
                                   canonicalOffset,
                                   canonicalRemoved,
                                   insertedText));
    m_applyingCanonicalDelta = false;

    if (entry.item && entry.item->isTextItem()) {
        m_dirtyHighlightItems.insert(mti);
    }
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j 10 --target markoff_live 2>&1 | tail -3`
Expected: green.

- [ ] **Step 4: Run all existing markoff-live tests**

Run:
```bash
cd build && ctest -R "markoff_" --output-on-failure -j 10 2>&1 | tail -20
cd ..
```
Expected: all previously-passing tests still pass. Some canonical-bridge tests may produce new behavior; if any fails, halt and investigate (do not move on).

- [ ] **Step 5: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/SceneCoordinator.cpp
git commit -m "markoff-live: wire SubstitutionTable into onLocalItemContentsChange (C8 Task 3.2)"
```

---

## Phase 4 — Inbound strip-then-splice-then-reapply

Goal: rewrite `SceneCoordinator::applyCanonicalDelta` single-item path per spec §3.4. Add `snapshotViewStateAsCanonical` / `restoreViewStateFromCanonical` to `MarkdownTextItem`.

### Task 4.1: Add CanonicalSnapshot struct

**Files:**
- Create: `libs/markoff-family/libs/markoff-live/src/CanonicalSnapshot.h`

- [ ] **Step 1: Write the header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_CANONICALSNAPSHOT_H
#define MARKOFF_CANONICALSNAPSHOT_H

namespace Markoff {

/// View-state snapshot for a MarkdownTextItem, expressed in canonical
/// (source-space) coordinates within the item's block.
///
/// Canonical coordinates are stable across strip-then-splice-then-reapply
/// cycles: they refer to positions in the source text, not the
/// substitution-decorated local QTextDocument. After the cycle, the
/// caller translates back to local via the rebuilt SubstitutionTable.
struct CanonicalSnapshot {
    bool valid = false;
    int canonicalCursor = -1;       // cursor position in canonical (-1 == no cursor)
    int canonicalAnchor = -1;       // selection anchor in canonical (-1 == no selection)
    bool hasSelection = false;
};

} // namespace Markoff

#endif // MARKOFF_CANONICALSNAPSHOT_H
```

- [ ] **Step 2: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/CanonicalSnapshot.h
git commit -m "markoff-live: add CanonicalSnapshot struct (C8 Task 4.1)"
```

### Task 4.2: Add snapshot/restore on MarkdownTextItem

**Files:**
- Modify: `libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.h/.cpp`

- [ ] **Step 1: Declare in header**

Include `CanonicalSnapshot.h` alongside other includes. Add to the `public:` section:

```cpp
    /// Snapshot the item's cursor + selection in canonical coordinates.
    /// Call BEFORE stripInlineSubstitutions so the current m_substitutions
    /// table can still translate.
    CanonicalSnapshot snapshotViewStateAsCanonical() const;

    /// Restore cursor + selection from a canonical snapshot. Call AFTER
    /// refreshInlineSubstitutions has rebuilt m_substitutions. Positions
    /// inside an ORC's canonical span are clamped to the ORC's nearest
    /// boundary on the side the cursor was previously closest to.
    void restoreViewStateFromCanonical(const CanonicalSnapshot &snap);
```

- [ ] **Step 2: Implement in the .cpp**

Append:

```cpp
CanonicalSnapshot MarkdownTextItem::snapshotViewStateAsCanonical() const
{
    CanonicalSnapshot snap;
    if (!m_control) return snap;
    const QTextCursor cursor = m_control->textCursor();
    snap.canonicalCursor = m_substitutions.localToCanonical(cursor.position());
    if (cursor.hasSelection()) {
        snap.canonicalAnchor = m_substitutions.localToCanonical(cursor.anchor());
        snap.hasSelection = true;
    }
    snap.valid = true;
    return snap;
}

void MarkdownTextItem::restoreViewStateFromCanonical(const CanonicalSnapshot &snap)
{
    if (!snap.valid || !m_control) return;

    auto clampToBoundary = [&](int canonicalPos) -> int {
        // Use canonicalToLocal; if the result falls on an ORC glyph, that
        // already represents the nearest boundary (SubstitutionTable maps
        // canonical-interior to the ORC's local position, which is the
        // left boundary; advancing by 1 gives the right boundary).
        // canonicalToLocal already clamps to the ORC glyph local pos,
        // which is the boundary just BEFORE the ORC's canonical span.
        const int local = m_substitutions.canonicalToLocal(canonicalPos);
        // If the pre-snapshot canonical position is strictly inside an
        // ORC span, decide which side is closer.
        if (m_substitutions.isInsideOrcCanonicalSpan(canonicalPos)) {
            // canonicalToLocal returned the ORC's local position (left
            // boundary). Decide between left (at ORC glyph) and right
            // (one past ORC glyph) by comparing proximity in canonical
            // space.
            // Walk entries to find the containing ORC.
            int accumShift = 0;
            for (const auto &s : m_substitutions.entries()) {
                const int canonStart = s.localPos + accumShift;
                const int canonEnd = canonStart + s.rawLen;
                if (canonicalPos > canonStart && canonicalPos < canonEnd) {
                    const int distLeft = canonicalPos - canonStart;
                    const int distRight = canonEnd - canonicalPos;
                    return (distRight < distLeft) ? (s.localPos + 1) : s.localPos;
                }
                accumShift += s.rawLen - 1;
            }
        }
        return local;
    };

    const int localCursor = clampToBoundary(snap.canonicalCursor);
    QTextCursor cursor(m_document);
    if (snap.hasSelection) {
        const int localAnchor = clampToBoundary(snap.canonicalAnchor);
        cursor.setPosition(localAnchor);
        cursor.setPosition(localCursor, QTextCursor::KeepAnchor);
    } else {
        cursor.setPosition(localCursor);
    }
    m_control->setTextCursor(cursor);
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j 10 --target markoff_live 2>&1 | tail -3`
Expected: green.

- [ ] **Step 4: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/MarkdownTextItem.h libs/markoff-live/src/MarkdownTextItem.cpp
git commit -m "markoff-live: add snapshot/restore view state in canonical coords (C8 Task 4.2)"
```

### Task 4.3: Rewrite applyCanonicalDelta single-item path

**Files:**
- Modify: `libs/markoff-family/libs/markoff-live/src/SceneCoordinator.cpp`

- [ ] **Step 1: Find the function**

Run: `grep -n "SceneCoordinator::applyCanonicalDelta" libs/markoff-family/libs/markoff-live/src/SceneCoordinator.cpp`

- [ ] **Step 2: Replace the single-item splice body**

The function has three early-return paths (multi-item, non-text, out-of-range) that each set `m_sceneNeedsFullRebuildOnNextParse = true;`. Those paths are UNCHANGED.

Replace the current single-item splice tail (the block that does `c.setPosition(localPos); c.setPosition(localPos + int(removed), KeepAnchor); const QString insertedText = m_boundDoc->substring(offset, inserted); c.insertText(insertedText);`) with:

```cpp
    // Single-item splice: flatten the affected item to source form, splice
    // in source space (where local positions equal canonical-for-this-block),
    // then re-substitute. View-state preserved via canonical-coordinate
    // snapshot over the reshape.
    auto *mti = static_cast<MarkdownTextItem *>(block.item);
    const CanonicalSnapshot snap = mti->snapshotViewStateAsCanonical();

    m_applyingCanonicalDelta = true;
    mti->stripInlineSubstitutions();

    const int localPos = int(offset) - block.canonicalStart;
    QTextCursor c(mti->document());
    c.setPosition(localPos);
    c.setPosition(localPos + int(removed), QTextCursor::KeepAnchor);
    const QString insertedText = m_boundDoc->substring(offset, inserted);
    c.insertText(insertedText);

    mti->refreshInlineSubstitutions();
    m_applyingCanonicalDelta = false;

    mti->restoreViewStateFromCanonical(snap);

    if (block.item && block.item->isTextItem()) {
        m_dirtyHighlightItems.insert(mti);
    }

    qCDebug(lcSceneRebuild) << "splice (strip-reapply):"
                            << "offset=" << offset
                            << "removed=" << removed
                            << "inserted=" << inserted
                            << "itemIdx=" << blockIdx;

    block.canonicalEnd += int(inserted - removed);
    shiftItemsAfter(blockIdx, int(inserted - removed));
```

- [ ] **Step 3: Ensure include**

Near the top of `SceneCoordinator.cpp`, verify/add:
```cpp
#include "CanonicalSnapshot.h"
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j 10 --target markoff_live 2>&1 | tail -3`
Expected: green.

- [ ] **Step 5: Run all markoff-live tests**

Run:
```bash
cd build && ctest -R "markoff_" --output-on-failure -j 10 2>&1 | tail -20
cd ..
```
Expected: all previously-passing tests still pass.

- [ ] **Step 6: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/src/SceneCoordinator.cpp
git commit -m "markoff-live: rewrite inbound applyCanonicalDelta single-item as strip-splice-reapply (C8 Task 4.3)"
```

---

## Phase 5 — Debug assertions + Vault terminal guard + saveFailed signal

Goal: canonical buffer is asserted ORC-free in debug; `Vault::saveDocument` refuses to write ORC bytes in all builds.

### Task 5.1: Add Q_ASSERTs on MarkoffDocument::applyCanonicalDelta

**Files:**
- Modify: `libs/markoff-family/libs/markoff-core/src/MarkoffDocument.cpp`

- [ ] **Step 1: Find the function**

Run: `grep -n "MarkoffDocument::applyCanonicalDelta" libs/markoff-family/libs/markoff-core/src/MarkoffDocument.cpp`

- [ ] **Step 2: Add pre- and post-condition Q_ASSERTs**

At the top of the function body (first line), add:
```cpp
    Q_ASSERT(!inserted.contains(QChar::ObjectReplacementCharacter));
```

After `d->buffer->applyDelta(offset, removedLength, inserted);` add:
```cpp
    Q_ASSERT(!d->buffer->toMarkdown().contains(QChar::ObjectReplacementCharacter));
```

Include `<QChar>` if not already included (it comes in via `<QString>`).

- [ ] **Step 3: Build**

Run: `cmake --build build -j 10 --target markoff_core 2>&1 | tail -3`
Expected: green.

- [ ] **Step 4: Run all markoff tests**

Run:
```bash
cd build && ctest -R "markoff_" --output-on-failure -j 10 2>&1 | tail -10
cd ..
```
Expected: all pass. (If an assertion trips, we have a regression in Phases 1-4 and must halt here.)

- [ ] **Step 5: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-core/src/MarkoffDocument.cpp
git commit -m "markoff-core: Q_ASSERT canonical buffer is ORC-free around applyCanonicalDelta (C8 Task 5.1)"
```

### Task 5.2: Add saveFailed() signal on NoteDocument

**Files:**
- Modify: `libs/core/include/corbomite/core/NoteDocument.h` (path may differ — confirm with grep)

- [ ] **Step 1: Locate the NoteDocument header**

Run: `find /home/clinton/dev/Corbomite -name NoteDocument.h -not -path "*/build*" -not -path "*/.worktrees*" 2>/dev/null`
Expected: one path.

- [ ] **Step 2: Find the signals block**

Open that file. Find the `signals:` section that includes `void saved();`.

- [ ] **Step 3: Add the new signal**

Adjacent to `void saved();`, add:
```cpp
    /// Emitted when Vault::saveDocument aborts the write (e.g. canonical
    /// buffer contains invalid bytes). The file on disk is unchanged.
    void saveFailed();
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j 10 --target corbomite_core 2>&1 | tail -3`
Expected: green.

- [ ] **Step 5: Commit**

```bash
cd /home/clinton/dev/Corbomite
git add libs/core/include/corbomite/core/NoteDocument.h
git commit -m "core: add NoteDocument::saveFailed() signal (C8 Task 5.2)"
```

### Task 5.3: Write failing test for Vault terminal guard

**Files:**
- Create: `libs/vault/tests/tst_vault_save_refuses_orc.cpp`

- [ ] **Step 1: Write the test**

First inspect existing vault tests for test-app/fixture idiom:
```bash
ls libs/vault/tests/
head -40 libs/vault/tests/tst_vault_saveDocument.cpp 2>/dev/null || ls libs/vault/tests/ | head
```

Copy the closest save-flow test's setup preamble (Vault construction, temp dir, NoteDocument creation). Then the new test:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include "corbomite/vault/Vault.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include <markoff/MarkoffDocument.h>

using namespace Corbomite;

class TstVaultSaveRefusesOrc : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void saveDocument_withOrcInCanonical_returnsFalseAndDoesNotWrite();
};

void TstVaultSaveRefusesOrc::saveDocument_withOrcInCanonical_returnsFalseAndDoesNotWrite()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Seed a vault with a clean file
    const QString vaultPath = tmp.path();
    const QString relPath = QStringLiteral("Note.md");
    const QString cleanContent = QStringLiteral("Hello, world.\n");
    {
        QFile f(vaultPath + QLatin1Char('/') + relPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(cleanContent.toUtf8());
    }

    FileSystemAdapter adapter;
    Vault vault(&adapter);
    QVERIFY(vault.open(vaultPath));
    NoteDocument *doc = vault.openDocument(relPath);
    QVERIFY(doc);

    // Inject U+FFFC into the canonical buffer via the document's markoff handle
    // (this simulates a regression that bypassed every earlier guard).
    const QString pollutedCanonical = QStringLiteral("Hello, ￼world.\n");
    doc->markoff()->resetContent(pollutedCanonical, Markoff::MarkoffDocument::Origin::TestFixture);

    QSignalSpy failedSpy(doc, &NoteDocument::saveFailed);
    const bool ok = vault.saveDocument(doc);
    QCOMPARE(ok, false);
    QCOMPARE(failedSpy.count(), 1);

    // File on disk unchanged.
    QFile f(vaultPath + QLatin1Char('/') + relPath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(f.readAll()), cleanContent);
}

QTEST_MAIN(TstVaultSaveRefusesOrc)
#include "tst_vault_save_refuses_orc.moc"
```

- [ ] **Step 2: Register in CMake**

Edit `libs/vault/tests/CMakeLists.txt`. Add an analogous entry to the other save tests:
```cmake
add_vault_test(tst_vault_save_refuses_orc tst_vault_save_refuses_orc.cpp)
```

(Use the actual helper name from the existing file.)

- [ ] **Step 3: Build and confirm the test fails (guard doesn't exist yet)**

Run:
```bash
cmake --build build -j 10 --target tst_vault_save_refuses_orc 2>&1 | tail -5
cd build && ctest -R tst_vault_save_refuses_orc --output-on-failure
cd ..
```
Expected: FAIL (the save currently succeeds).

- [ ] **Step 4: Commit the failing test**

```bash
cd /home/clinton/dev/Corbomite
git add libs/vault/tests/tst_vault_save_refuses_orc.cpp libs/vault/tests/CMakeLists.txt
git commit -m "vault: add failing test for saveDocument U+FFFC refusal (C8 Task 5.3)"
```

### Task 5.4: Implement the Vault terminal guard

**Files:**
- Modify: `libs/vault/src/Vault.cpp`

- [ ] **Step 1: Locate saveDocument**

Run: `grep -n "Vault::saveDocument" libs/vault/src/Vault.cpp`

- [ ] **Step 2: Add the U+FFFC refusal check**

Immediately after the line `const QByteArray bytes = doc->markoff()->toMarkdown().toUtf8();`, replace that line and the `const QString abs = ...;` line with:

```cpp
    const QString markdown = doc->markoff()->toMarkdown();
    if (markdown.contains(QChar::ObjectReplacementCharacter)) {
        qCCritical(lcVaultSafety,
            "Vault::saveDocument REFUSED: canonical buffer contains U+FFFC "
            "for rel=\"%s\" (chars=%lld). Aborting write; file unchanged.",
            qUtf8Printable(rel), (long long)markdown.size());
        Q_EMIT doc->saveFailed();
        return false;
    }
    const QByteArray bytes = markdown.toUtf8();
    const QString abs = m_basePath + QLatin1Char('/') + rel;
```

- [ ] **Step 3: Add the logging category declaration**

At the file's top include block, after `#include <QLoggingCategory>` (add if missing), add below:
```cpp
Q_LOGGING_CATEGORY(lcVaultSafety, "corbomite.vault.safety")
```

(This category is new; distinct from the earlier exploratory `markoff.math.trace`.)

- [ ] **Step 4: Build and run the refusal test**

Run:
```bash
cmake --build build -j 10 --target tst_vault_save_refuses_orc 2>&1 | tail -3
cd build && ctest -R tst_vault_save_refuses_orc --output-on-failure
cd ..
```
Expected: PASS.

- [ ] **Step 5: Full Corbomite suite sanity**

Run:
```bash
cd build && ctest --output-on-failure -j 10 2>&1 | tail -15
cd ..
```
Expected: all previously-passing tests still pass (allow pre-existing flakes listed in CLAUDE.md).

- [ ] **Step 6: Commit**

```bash
cd /home/clinton/dev/Corbomite
git add libs/vault/src/Vault.cpp
git commit -m "vault: refuse saveDocument when canonical buffer contains U+FFFC (C8 Task 5.4)"
```

---

## Phase 6 — Regression test suite

Goal: pin every invariant the spec §2 establishes with dedicated tests.

### Task 6.1: Write failing click-math-no-canonical-delta test

**Files:**
- Create: `libs/markoff-family/libs/markoff-live/tests/tst_math_click_canonical_coherence.cpp`

- [ ] **Step 1: Write the test**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <QSignalSpy>
#include <QGraphicsSceneMouseEvent>
#include <QTextCursor>
#include <markoff/Editor.h>
#include <markoff/MarkoffDocument.h>
#include "MarkdownTextItem.h"
#include "SceneCoordinator.h"
#include "MathTextObject.h"

using namespace Markoff;

class TstMathClickCanonicalCoherence : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void clickOnInlineMathGlyph_doesNotChangeCanonicalBuffer();
};

void TstMathClickCanonicalCoherence::clickOnInlineMathGlyph_doesNotChangeCanonicalBuffer()
{
    const QString src = QStringLiteral("Before $a + b$ middle $c^2$ after.\n");
    auto doc = std::make_unique<MarkoffDocument>();
    doc->resetContent(src, MarkoffDocument::Origin::TestFixture);

    Editor editor;
    editor.setDocument(doc.get());
    // Wait for the parse pool to land the first parse so the scene builds.
    QTRY_VERIFY(editor.coordinator()->items().size() > 0);

    auto *mti = editor.firstTextItem();
    QVERIFY(mti);
    QTRY_VERIFY(mti->substitutions().size() >= 2);

    const QString canonicalBefore = doc->toMarkdown();
    const qsizetype lenBefore = doc->length();

    // Locate the first math glyph's local position.
    const int glyphLocalPos = mti->substitutions().entries().first().localPos;
    Q_ASSERT(glyphLocalPos >= 0);

    // Synthesize a mouse click at that glyph's approximate position in scene
    // coords. Using the documentLayout to find the pixel pos.
    const QRectF rect = mti->document()->documentLayout()
        ->blockBoundingRect(mti->document()->findBlock(glyphLocalPos));
    const QPointF scenePoint = rect.center();

    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setButton(Qt::LeftButton);
    press.setPos(scenePoint);
    press.setScenePos(scenePoint);
    mti->mousePressEvent(&press);

    // After click, canonical buffer and length must be unchanged.
    QCOMPARE(doc->length(), lenBefore);
    QCOMPARE(doc->toMarkdown(), canonicalBefore);
}

QTEST_MAIN(TstMathClickCanonicalCoherence)
#include "tst_math_click_canonical_coherence.moc"
```

- [ ] **Step 2: Register in CMake**

Edit `libs/markoff-family/libs/markoff-live/tests/CMakeLists.txt`:
```cmake
add_markoff_live_test(tst_math_click_canonical_coherence tst_math_click_canonical_coherence.cpp)
```

- [ ] **Step 3: Build and run**

Run:
```bash
cmake --build build -j 10 --target tst_math_click_canonical_coherence 2>&1 | tail -3
cd build && ctest -R tst_math_click_canonical_coherence --output-on-failure
cd ..
```
Expected: PASS (the Phase-1 and Phase-3 changes already stop the leakage). If it fails, investigate — there is a leak path we missed.

- [ ] **Step 4: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/tests/tst_math_click_canonical_coherence.cpp libs/markoff-live/tests/CMakeLists.txt
git commit -m "markoff-live: pin click-math-no-canonical-delta invariant (C8 Task 6.1)"
```

### Task 6.2: Write checkbox-toggle-no-canonical-delta test

**Files:**
- Create: `libs/markoff-family/libs/markoff-live/tests/tst_checkbox_toggle_canonical_coherence.cpp`

- [ ] **Step 1: Write the test**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <QGraphicsSceneMouseEvent>
#include <markoff/Editor.h>
#include <markoff/MarkoffDocument.h>
#include "MarkdownTextItem.h"
#include "CheckboxTextObject.h"

using namespace Markoff;

class TstCheckboxToggleCanonicalCoherence : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void toggleCheckbox_doesNotDesyncCanonicalLength();
    void toggleCheckbox_flipsCheckedState();
};

static Editor *makeEditor(MarkoffDocument *doc) {
    auto *e = new Editor;
    e->setDocument(doc);
    return e;
}

void TstCheckboxToggleCanonicalCoherence::toggleCheckbox_doesNotDesyncCanonicalLength()
{
    const QString src = QStringLiteral("- [ ] first task\n- [x] done task\n");
    auto doc = std::make_unique<MarkoffDocument>();
    doc->resetContent(src, MarkoffDocument::Origin::TestFixture);

    std::unique_ptr<Editor> editor(makeEditor(doc.get()));
    QTRY_VERIFY(editor->coordinator()->items().size() > 0);
    auto *mti = editor->firstTextItem();
    QVERIFY(mti);

    const qsizetype lenBefore = doc->length();

    // Click on the first checkbox glyph.
    QTRY_VERIFY(mti->substitutions().size() >= 2);
    const int glyphLocalPos = mti->substitutions().entries().first().localPos;
    const QRectF rect = mti->document()->documentLayout()
        ->blockBoundingRect(mti->document()->findBlock(glyphLocalPos));
    const QPointF pt = rect.topLeft() + QPointF(2, rect.height() / 2);

    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setButton(Qt::LeftButton);
    press.setPos(pt);
    press.setScenePos(pt);
    mti->mousePressEvent(&press);

    QCOMPARE(doc->length(), lenBefore);
}

void TstCheckboxToggleCanonicalCoherence::toggleCheckbox_flipsCheckedState()
{
    const QString src = QStringLiteral("- [ ] task\n");
    auto doc = std::make_unique<MarkoffDocument>();
    doc->resetContent(src, MarkoffDocument::Origin::TestFixture);

    std::unique_ptr<Editor> editor(makeEditor(doc.get()));
    QTRY_VERIFY(editor->coordinator()->items().size() > 0);
    auto *mti = editor->firstTextItem();
    QVERIFY(mti);
    QTRY_VERIFY(mti->substitutions().size() == 1);

    const int glyphLocalPos = mti->substitutions().entries().first().localPos;
    const QRectF rect = mti->document()->documentLayout()
        ->blockBoundingRect(mti->document()->findBlock(glyphLocalPos));
    const QPointF pt = rect.topLeft() + QPointF(2, rect.height() / 2);

    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setButton(Qt::LeftButton);
    press.setPos(pt);
    press.setScenePos(pt);
    mti->mousePressEvent(&press);

    QTextCursor c(mti->document());
    c.setPosition(glyphLocalPos);
    c.setPosition(glyphLocalPos + 1, QTextCursor::KeepAnchor);
    QCOMPARE(c.charFormat().property(CheckboxTextObject::CheckedProperty).toBool(), true);
}

QTEST_MAIN(TstCheckboxToggleCanonicalCoherence)
#include "tst_checkbox_toggle_canonical_coherence.moc"
```

- [ ] **Step 2: Register + build + run**

```cmake
add_markoff_live_test(tst_checkbox_toggle_canonical_coherence tst_checkbox_toggle_canonical_coherence.cpp)
```

Run:
```bash
cmake --build build -j 10 --target tst_checkbox_toggle_canonical_coherence 2>&1 | tail -3
cd build && ctest -R tst_checkbox_toggle_canonical_coherence --output-on-failure
cd ..
```
Expected: PASS.

- [ ] **Step 3: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/tests/tst_checkbox_toggle_canonical_coherence.cpp libs/markoff-live/tests/CMakeLists.txt
git commit -m "markoff-live: pin checkbox-toggle canonical-coherence invariant (C8 Task 6.2)"
```

### Task 6.3: Write mermaid-click-no-canonical-delta test

**Files:**
- Create: `libs/markoff-family/libs/markoff-live/tests/tst_mermaid_click_canonical_coherence.cpp`

- [ ] **Step 1: Write the test**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <QGraphicsSceneMouseEvent>
#include <markoff/Editor.h>
#include <markoff/MarkoffDocument.h>
#include "MarkdownTextItem.h"

using namespace Markoff;

class TstMermaidClickCanonicalCoherence : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void clickOnMermaidGlyph_doesNotChangeCanonicalBuffer();
};

void TstMermaidClickCanonicalCoherence::clickOnMermaidGlyph_doesNotChangeCanonicalBuffer()
{
    const QString src = QStringLiteral(
        "Before.\n\n```mermaid\ngraph TD\nA --> B\n```\n\nAfter.\n");
    auto doc = std::make_unique<MarkoffDocument>();
    doc->resetContent(src, MarkoffDocument::Origin::TestFixture);

    Editor editor;
    editor.setDocument(doc.get());
    QTRY_VERIFY(editor.coordinator()->items().size() > 0);

    // Find the text item whose substitution table contains the mermaid ORC.
    MarkdownTextItem *mermaidItem = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        [&] {
            for (auto *it : editor.coordinator()->items()) {
                if (!it->isTextItem()) continue;
                auto *mti = static_cast<MarkdownTextItem *>(it);
                for (const auto &s : mti->substitutions().entries()) {
                    if (s.rawLen > 20) {  // mermaid block raw is long
                        mermaidItem = mti;
                        return true;
                    }
                }
            }
            return false;
        }(),
        5000);

    QVERIFY(mermaidItem);
    const qsizetype lenBefore = doc->length();
    const QString canonicalBefore = doc->toMarkdown();

    // Click on the mermaid glyph.
    int glyphLocalPos = -1;
    for (const auto &s : mermaidItem->substitutions().entries()) {
        if (s.rawLen > 20) { glyphLocalPos = s.localPos; break; }
    }
    QVERIFY(glyphLocalPos >= 0);
    const QRectF rect = mermaidItem->document()->documentLayout()
        ->blockBoundingRect(mermaidItem->document()->findBlock(glyphLocalPos));
    const QPointF pt = rect.center();

    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setButton(Qt::LeftButton);
    press.setPos(pt);
    press.setScenePos(pt);
    mermaidItem->mousePressEvent(&press);

    QCOMPARE(doc->length(), lenBefore);
    QCOMPARE(doc->toMarkdown(), canonicalBefore);
}

QTEST_MAIN(TstMermaidClickCanonicalCoherence)
#include "tst_mermaid_click_canonical_coherence.moc"
```

- [ ] **Step 2: Register + build + run + commit**

```cmake
add_markoff_live_test(tst_mermaid_click_canonical_coherence tst_mermaid_click_canonical_coherence.cpp)
```

Run:
```bash
cmake --build build -j 10 --target tst_mermaid_click_canonical_coherence 2>&1 | tail -3
cd build && ctest -R tst_mermaid_click_canonical_coherence --output-on-failure
cd ..
```
Expected: PASS.

```bash
cd libs/markoff-family
git add libs/markoff-live/tests/tst_mermaid_click_canonical_coherence.cpp libs/markoff-live/tests/CMakeLists.txt
git commit -m "markoff-live: pin mermaid-click canonical-coherence invariant (C8 Task 6.3)"
```

### Task 6.4: Write reveal-edit-collapse round-trip test

**Files:**
- Create: `libs/markoff-family/libs/markoff-live/tests/tst_reveal_edit_collapse_roundtrip.cpp`

- [ ] **Step 1: Write the test**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <markoff/Editor.h>
#include <markoff/MarkoffDocument.h>
#include "MarkdownTextItem.h"

using namespace Markoff;

class TstRevealEditCollapseRoundtrip : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void revealMath_editLatex_collapseProducesEditedCanonical();
};

void TstRevealEditCollapseRoundtrip::revealMath_editLatex_collapseProducesEditedCanonical()
{
    const QString src = QStringLiteral("Text $E = mc^2$ tail.\n");
    auto doc = std::make_unique<MarkoffDocument>();
    doc->resetContent(src, MarkoffDocument::Origin::TestFixture);

    Editor editor;
    editor.setDocument(doc.get());
    QTRY_VERIFY(editor.coordinator()->items().size() > 0);
    auto *mti = editor.firstTextItem();
    QTRY_VERIFY(mti->substitutions().size() == 1);

    // 1. Click the math glyph to reveal.
    const int glyphLocalPos = mti->substitutions().entries().first().localPos;
    const QRectF rect = mti->document()->documentLayout()
        ->blockBoundingRect(mti->document()->findBlock(glyphLocalPos));
    const QPointF pt = rect.center();
    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setButton(Qt::LeftButton);
    press.setPos(pt);
    press.setScenePos(pt);
    mti->mousePressEvent(&press);

    // 2. Reveal should have expanded the ORC. Substitution table should now
    // exclude this formula.
    QVERIFY(mti->substitutions().size() == 0);

    // 3. Type over the "2" in mc^2, replacing with "3". We expect the
    // cursor to be positioned just past the opening `$` after reveal.
    // Find the `^2` sequence and position the cursor there, replace 2 with 3.
    QTextCursor c(mti->document());
    const QString docText = mti->document()->toPlainText();
    const int twoIdx = docText.indexOf(QStringLiteral("^2")) + 1;
    c.setPosition(twoIdx);
    c.setPosition(twoIdx + 1, QTextCursor::KeepAnchor);
    c.insertText(QStringLiteral("3"));

    // 4. Move cursor out of the revealed region to trigger collapse.
    QTextCursor out(mti->document());
    out.setPosition(0);
    mti->setTextCursor(out);
    mti->updateReveal();

    // 5. Canonical now reflects the edited LaTeX.
    QCOMPARE(doc->toMarkdown(), QStringLiteral("Text $E = mc^3$ tail.\n"));
}

QTEST_MAIN(TstRevealEditCollapseRoundtrip)
#include "tst_reveal_edit_collapse_roundtrip.moc"
```

If the test needs a `setTextCursor` accessor on `MarkdownTextItem` that isn't already public, add a test-only or permanent accessor as needed (grep first for an existing public path). `updateReveal()` is already public on the item per C3's reveal design — confirm with `grep "void updateReveal" libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.h`.

- [ ] **Step 2: Register + build + run**

```cmake
add_markoff_live_test(tst_reveal_edit_collapse_roundtrip tst_reveal_edit_collapse_roundtrip.cpp)
```

Run:
```bash
cmake --build build -j 10 --target tst_reveal_edit_collapse_roundtrip 2>&1 | tail -3
cd build && ctest -R tst_reveal_edit_collapse_roundtrip --output-on-failure
cd ..
```
Expected: PASS.

- [ ] **Step 3: Commit**

```bash
cd libs/markoff-family
git add libs/markoff-live/tests/tst_reveal_edit_collapse_roundtrip.cpp libs/markoff-live/tests/CMakeLists.txt
git commit -m "markoff-live: pin reveal-edit-collapse round-trip (C8 Task 6.4)"
```

### Task 6.5: Write edit-after-multiple-ORCs offset-translation test

**Files:**
- Create: `libs/markoff-family/libs/markoff-live/tests/tst_edit_after_multiple_orcs.cpp`

- [ ] **Step 1: Write the test**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <markoff/Editor.h>
#include <markoff/MarkoffDocument.h>
#include "MarkdownTextItem.h"

using namespace Markoff;

class TstEditAfterMultipleOrcs : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void typingAfterSeveralMathGlyphs_producesCorrectCanonicalOffset();
};

void TstEditAfterMultipleOrcs::typingAfterSeveralMathGlyphs_producesCorrectCanonicalOffset()
{
    const QString src = QStringLiteral(
        "First $a$ second $b$ third $c$ tail here.\n");
    auto doc = std::make_unique<MarkoffDocument>();
    doc->resetContent(src, MarkoffDocument::Origin::TestFixture);

    Editor editor;
    editor.setDocument(doc.get());
    QTRY_VERIFY(editor.coordinator()->items().size() > 0);
    auto *mti = editor.firstTextItem();
    QTRY_VERIFY(mti->substitutions().size() == 3);

    // Insert "!" at the local position corresponding to canonical offset
    // just before "tail". In canonical, "tail" starts at offset
    // strlen("First $a$ second $b$ third $c$ ") = 31.
    const int canonicalInsertPos = 31;
    const int localInsertPos = mti->substitutions().canonicalToLocal(canonicalInsertPos);

    QTextCursor c(mti->document());
    c.setPosition(localInsertPos);
    c.insertText(QStringLiteral("!"));

    // Canonical must contain "First $a$ second $b$ third $c$ !tail here.\n".
    QCOMPARE(doc->toMarkdown(),
             QStringLiteral("First $a$ second $b$ third $c$ !tail here.\n"));
}

QTEST_MAIN(TstEditAfterMultipleOrcs)
#include "tst_edit_after_multiple_orcs.moc"
```

- [ ] **Step 2: Register + build + run + commit**

```cmake
add_markoff_live_test(tst_edit_after_multiple_orcs tst_edit_after_multiple_orcs.cpp)
```

Run:
```bash
cmake --build build -j 10 --target tst_edit_after_multiple_orcs 2>&1 | tail -3
cd build && ctest -R tst_edit_after_multiple_orcs --output-on-failure
cd ..
```
Expected: PASS.

```bash
cd libs/markoff-family
git add libs/markoff-live/tests/tst_edit_after_multiple_orcs.cpp libs/markoff-live/tests/CMakeLists.txt
git commit -m "markoff-live: pin edit-after-multiple-ORCs offset translation (C8 Task 6.5)"
```

### Task 6.6: Write inbound-splice-preserves-cursor test

**Files:**
- Create: `libs/markoff-family/libs/markoff-live/tests/tst_inbound_splice_preserves_cursor.cpp`

- [ ] **Step 1: Write the test**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <markoff/Editor.h>
#include <markoff/MarkoffDocument.h>
#include <markoff/MarkdownDelta.h>
#include "MarkdownTextItem.h"
#include "SceneCoordinator.h"

using namespace Markoff;

class TstInboundSplicePreservesCursor : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void inboundDeltaOutsideAllOrcs_cursorUnchangedInLocal();
    void inboundDeltaStraddlingOrcBoundary_cursorPreservedInCanonical();
    void inboundDeltaInsideOrcSpan_cursorClampedToNearestBoundary();
};

static Editor *makeEditor(MarkoffDocument *doc) {
    auto *e = new Editor;
    e->setDocument(doc);
    return e;
}

void TstInboundSplicePreservesCursor::inboundDeltaOutsideAllOrcs_cursorUnchangedInLocal()
{
    const QString src = QStringLiteral("pre $a$ middle $b$ post.\n");
    auto doc = std::make_unique<MarkoffDocument>();
    doc->resetContent(src, MarkoffDocument::Origin::TestFixture);
    std::unique_ptr<Editor> e(makeEditor(doc.get()));
    QTRY_VERIFY(e->coordinator()->items().size() > 0);
    auto *mti = e->firstTextItem();
    QTRY_VERIFY(mti->substitutions().size() == 2);

    // Put cursor at local position == "post" 's first char (outside any ORC).
    const int canonicalCursor = src.indexOf(QStringLiteral("post."));
    QTextCursor c(mti->document());
    c.setPosition(mti->substitutions().canonicalToLocal(canonicalCursor));
    mti->setTextCursor(c);

    // Inbound delta: insert "X" at canonical offset 4 (inside "pre ").
    doc->undoStack()->push(
        new Markoff::MarkdownDelta(doc.get(), 4, 0, QStringLiteral("X")));

    // Cursor's canonical position should still be the same "post." start,
    // shifted by +1 canonical.
    const int cursorCanonicalAfter = mti->substitutions().localToCanonical(
        mti->textCursor().position());
    QCOMPARE(cursorCanonicalAfter, canonicalCursor + 1);
}

void TstInboundSplicePreservesCursor::inboundDeltaStraddlingOrcBoundary_cursorPreservedInCanonical()
{
    const QString src = QStringLiteral("pre $a$ tail.\n");
    auto doc = std::make_unique<MarkoffDocument>();
    doc->resetContent(src, MarkoffDocument::Origin::TestFixture);
    std::unique_ptr<Editor> e(makeEditor(doc.get()));
    QTRY_VERIFY(e->coordinator()->items().size() > 0);
    auto *mti = e->firstTextItem();
    QTRY_VERIFY(mti->substitutions().size() == 1);

    // Put cursor at canonical 10 (inside " tail.").
    const int canonicalCursor = 10;
    QTextCursor c(mti->document());
    c.setPosition(mti->substitutions().canonicalToLocal(canonicalCursor));
    mti->setTextCursor(c);

    // Inbound delta replacing the "$a$" and the preceding space at canonical
    // [3, 7) with "XX" — straddles the ORC boundary.
    doc->undoStack()->push(
        new Markoff::MarkdownDelta(doc.get(), 3, 4, QStringLiteral("XX")));

    // Cursor's canonical position shifted by (2 - 4) = -2 → canonical 8.
    const int cursorCanonicalAfter = mti->substitutions().localToCanonical(
        mti->textCursor().position());
    QCOMPARE(cursorCanonicalAfter, 8);
    QCOMPARE(doc->toMarkdown(), QStringLiteral("preXXtail.\n"));
}

void TstInboundSplicePreservesCursor::inboundDeltaInsideOrcSpan_cursorClampedToNearestBoundary()
{
    const QString src = QStringLiteral("pre $abc$ tail.\n");
    auto doc = std::make_unique<MarkoffDocument>();
    doc->resetContent(src, MarkoffDocument::Origin::TestFixture);
    std::unique_ptr<Editor> e(makeEditor(doc.get()));
    QTRY_VERIFY(e->coordinator()->items().size() > 0);
    auto *mti = e->firstTextItem();
    QTRY_VERIFY(mti->substitutions().size() == 1);

    // Put cursor just past the ORC (canonical 9 = start of " tail").
    const int canonicalCursor = 9;
    QTextCursor c(mti->document());
    c.setPosition(mti->substitutions().canonicalToLocal(canonicalCursor));
    mti->setTextCursor(c);

    // Inbound delta inside the ORC's canonical span: replace "b" (canonical 5)
    // with "X". The splice strips the ORC, edits, reapplies — the math region
    // is still intact as "$aXc$".
    doc->undoStack()->push(
        new Markoff::MarkdownDelta(doc.get(), 5, 1, QStringLiteral("X")));

    QCOMPARE(doc->toMarkdown(), QStringLiteral("pre $aXc$ tail.\n"));

    // Cursor's canonical position unchanged (9).
    const int cursorCanonicalAfter = mti->substitutions().localToCanonical(
        mti->textCursor().position());
    QCOMPARE(cursorCanonicalAfter, 9);

    // Substitution still present — math region survives the edit.
    QCOMPARE(mti->substitutions().size(), 1);
}

QTEST_MAIN(TstInboundSplicePreservesCursor)
#include "tst_inbound_splice_preserves_cursor.moc"
```

- [ ] **Step 2: Register + build + run + commit**

```cmake
add_markoff_live_test(tst_inbound_splice_preserves_cursor tst_inbound_splice_preserves_cursor.cpp)
```

Run:
```bash
cmake --build build -j 10 --target tst_inbound_splice_preserves_cursor 2>&1 | tail -3
cd build && ctest -R tst_inbound_splice_preserves_cursor --output-on-failure
cd ..
```
Expected: PASS.

```bash
cd libs/markoff-family
git add libs/markoff-live/tests/tst_inbound_splice_preserves_cursor.cpp libs/markoff-live/tests/CMakeLists.txt
git commit -m "markoff-live: pin inbound splice preserves cursor (C8 Task 6.6)"
```

### Task 6.7: Write canonical-never-contains-ORC soak test

**Files:**
- Create: `libs/markoff-family/libs/markoff-live/tests/tst_canonical_no_orc_invariant.cpp`

- [ ] **Step 1: Write the test**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <QGraphicsSceneMouseEvent>
#include <markoff/Editor.h>
#include <markoff/MarkoffDocument.h>
#include "MarkdownTextItem.h"

using namespace Markoff;

class TstCanonicalNoOrcInvariant : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void soakInteractions_canonicalStaysOrcFree();
};

void TstCanonicalNoOrcInvariant::soakInteractions_canonicalStaysOrcFree()
{
    const QString src = QStringLiteral(
        "Math $a$ and $b$ and [x] checkbox.\n"
        "Also $$c^2 + d^2 = e^2$$ display math.\n"
        "```mermaid\ngraph TD\nA --> B\n```\n");
    auto doc = std::make_unique<MarkoffDocument>();
    doc->resetContent(src, MarkoffDocument::Origin::TestFixture);
    Editor editor;
    editor.setDocument(doc.get());
    QTRY_VERIFY(editor.coordinator()->items().size() > 0);

    auto assertCanonicalClean = [&](const char *where) {
        QVERIFY2(!doc->toMarkdown().contains(QChar::ObjectReplacementCharacter),
                 where);
    };

    assertCanonicalClean("initial state");

    // Click every substitution glyph in every text item.
    for (auto *it : editor.coordinator()->items()) {
        if (!it->isTextItem()) continue;
        auto *mti = static_cast<MarkdownTextItem *>(it);
        QTRY_VERIFY(mti->substitutions().size() >= 0);
        for (const auto &s : mti->substitutions().entries()) {
            const QRectF rect = mti->document()->documentLayout()
                ->blockBoundingRect(mti->document()->findBlock(s.localPos));
            const QPointF pt = rect.center();
            QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
            press.setButton(Qt::LeftButton);
            press.setPos(pt);
            press.setScenePos(pt);
            mti->mousePressEvent(&press);
            assertCanonicalClean("after click");
        }
    }

    // Type prose after all ORCs.
    auto *mti = editor.firstTextItem();
    QTextCursor c(mti->document());
    c.movePosition(QTextCursor::End);
    mti->setTextCursor(c);
    QTest::keyClicks(editor.focusWidget() ? editor.focusWidget() : nullptr,
                     QStringLiteral("hello"));
    assertCanonicalClean("after typing");
}

QTEST_MAIN(TstCanonicalNoOrcInvariant)
#include "tst_canonical_no_orc_invariant.moc"
```

- [ ] **Step 2: Register + build + run + commit**

```cmake
add_markoff_live_test(tst_canonical_no_orc_invariant tst_canonical_no_orc_invariant.cpp)
```

Run:
```bash
cmake --build build -j 10 --target tst_canonical_no_orc_invariant 2>&1 | tail -3
cd build && ctest -R tst_canonical_no_orc_invariant --output-on-failure
cd ..
```
Expected: PASS.

```bash
cd libs/markoff-family
git add libs/markoff-live/tests/tst_canonical_no_orc_invariant.cpp libs/markoff-live/tests/CMakeLists.txt
git commit -m "markoff-live: soak-test canonical-never-contains-ORC invariant (C8 Task 6.7)"
```

---

## Phase 7 — Closeout

Goal: strip exploratory tracing, restore corrupted fixtures, tag, and document.

### Task 7.1: Strip exploratory `markoff.math.trace` logging

**Files:**
- Modify: `libs/markoff-family/libs/markoff-live/src/MarkdownTextItem.cpp`
- Modify: `libs/markoff-family/libs/markoff-live/src/SceneCoordinator.cpp`
- Modify: `libs/markoff-family/libs/markoff-core/src/MarkoffDocument.cpp`
- Modify: `libs/vault/src/Vault.cpp`

- [ ] **Step 1: Remove the trace category declarations**

In each of the four files, remove the `Q_LOGGING_CATEGORY(lcMathTrace*, "markoff.math.trace")` line and its associated `#define lcMathTrace lcMathTrace*` plus the `traceSnip` helper lambda/namespace.

- [ ] **Step 2: Remove every `qCDebug(lcMathTrace) ...` statement**

Run:
```bash
grep -rn "qCDebug(lcMathTrace)\|lcMathTrace\|traceSnip" libs/markoff-family/libs/markoff-{live,core}/src/ libs/vault/src/
```

Delete every match. The permanent diagnostic channels (`lcSceneRebuild` in markoff-live, `lcVaultSafety` added in Task 5.4) stay.

- [ ] **Step 3: Build standalone + Corbomite**

Run:
```bash
cd libs/markoff-family && cmake --build build-dev -j 10 2>&1 | tail -3 && cd -
cmake --build build -j 10 2>&1 | tail -3
```
Expected: both green.

- [ ] **Step 4: Run full test suites**

Run:
```bash
cd libs/markoff-family/build-dev && ctest -j 10 --output-on-failure 2>&1 | tail -10 && cd -
cd build && ctest -j 10 --output-on-failure 2>&1 | tail -15 && cd -
```
Expected: all pass (allowing pre-existing flakes).

- [ ] **Step 5: Commit (two commits — one per repo)**

```bash
cd /home/clinton/dev/Corbomite/libs/markoff-family
git add libs/markoff-live/src/MarkdownTextItem.cpp libs/markoff-live/src/SceneCoordinator.cpp libs/markoff-core/src/MarkoffDocument.cpp
git commit -m "markoff: strip exploratory markoff.math.trace logging (C8 Task 7.1)"
cd /home/clinton/dev/Corbomite
git add libs/vault/src/Vault.cpp libs/markoff-family
git commit -m "vault: strip exploratory markoff.math.trace logging; bump markoff (C8 Task 7.1)"
```

### Task 7.2: Restore the corrupted test-vault fixtures

**Files:**
- Modify: `testvaults/starter-vault/Parser Tests/Math.md`
- Modify: `testvaults/starter-vault/Parser Tests/Tables.md`

- [ ] **Step 1: Verify Math.md is still corrupted**

Run: `head -15 "testvaults/starter-vault/Parser Tests/Math.md"`
Expected: visible duplication in the inline-math line.

- [ ] **Step 2: Overwrite with the clean content**

Use `Write` tool to replace the entire file contents with the original clean version as captured in the session that created the fixture (the file was first authored at the start of this session; its original content is preserved in the git commit history of the `testvaults/starter-vault/Parser Tests/` directory prior to corruption). If not yet committed, restore from the in-session clean version.

Likewise for `Tables.md`.

- [ ] **Step 3: Diff sanity**

Run: `grep -c "mc\^2" "testvaults/starter-vault/Parser Tests/Math.md"`
Expected: `1` (single occurrence of `mc^2` in the inline-math example).

- [ ] **Step 4: Commit**

```bash
cd /home/clinton/dev/Corbomite
git add "testvaults/starter-vault/Parser Tests/Math.md" "testvaults/starter-vault/Parser Tests/Tables.md"
git commit -m "testvaults: restore Math.md + Tables.md after C8 guards landed"
```

### Task 7.3: Cut Markoff tag v0.9.1

**Files:** no file changes — tagging only.

- [ ] **Step 1: Verify Markoff master state**

```bash
cd /home/clinton/dev/Corbomite/libs/markoff-family
git status
git log --oneline -8
```
Expected: clean tree; recent commits show C8 Tasks 1-7.

- [ ] **Step 2: Full standalone test pass**

```bash
cmake --build build-dev -j 10 2>&1 | tail -3
cd build-dev && ctest -j 10 --output-on-failure 2>&1 | tail -5 && cd -
```
Expected: all pass.

- [ ] **Step 3: Cut the tag**

```bash
cd /home/clinton/dev/Corbomite/libs/markoff-family
git tag -a v0.9.1 -m "v0.9.1 — Phase C8 inline-ORC canonical coherence"
```

(Do NOT push to a remote automatically. User confirms push separately per CLAUDE.md "don't push unless asked".)

- [ ] **Step 4: Bump the Corbomite submodule pin**

```bash
cd /home/clinton/dev/Corbomite
git add libs/markoff-family
git commit -m "submodule: bump markoff to v0.9.1 — Phase C8 closed"
```

### Task 7.4: Update Markoff phase-c-status activity log

**Files:**
- Modify: `libs/markoff-family/docs/phase-c-status.md`

- [ ] **Step 1: Append a closeout activity-log entry**

At the top of the `## Activity log` section, above the existing C8-opening entry from 2026-04-23, add:

```markdown
### 2026-04-23 — C8 done end-to-end (inline-ORC canonical coherence)

Shipped across ~16 commits per the C8 implementation plan phases 1-7
(PresentationScope helper → substitution table + translator TDD →
outbound bridge wiring → inbound strip-splice-reapply → debug asserts +
Vault terminal guard + saveFailed signal → 7 regression tests → trace
stripping + fixture restoration + tag).

Invariants pinned:
- Presentation-plane mutations do not reach canonical. `PresentationScope`
  helper guards every reveal/collapse/strip/apply/toggle. Six call sites
  migrated in Phase 1.
- Local↔canonical position translation is routed through
  `MarkdownTextItem::substitutions()` — a `SubstitutionTable` with
  `localToCanonical` / `canonicalToLocal` / `localRangeToCanonical`. Unit-
  tested in `tst_substitution_table`.
- Outbound `onLocalItemContentsChange` expands `U+FFFC` in `insertedText`
  to raw source via `MarkdownTextItem::expandedTextForRange`. `Q_ASSERT`s
  on `MarkoffDocument::applyCanonicalDelta` catch any regression.
- Inbound `applyCanonicalDelta` single-item path flattens → splices in
  source space → re-substitutes. Cursor/selection preserved via
  `CanonicalSnapshot`. Works for inside-ORC / straddling / outside cases.
- `Vault::saveDocument` refuses to write bytes containing `U+FFFC`
  (release-build too). New `NoteDocument::saveFailed()` signal.

Seven regression tests pinning every invariant. All previously-passing
Markoff tests still green; full Corbomite ctest run unchanged from pre-C8
modulo pre-existing flakes.

Markoff tag `v0.9.1` cut. Submodule pin bumped on Corbomite side.
`testvaults/starter-vault/Parser Tests/{Math,Tables}.md` restored to
clean content now that guards prevent re-corruption on save.

Phase C now closed again. Cluster X (block-substitution widget promotion)
unblocked — expansion trigger met per its scouting doc.
```

Also update the work-unit status row from `spec drafted` to `done` and the Markoff PR column to `v0.9.1`.

- [ ] **Step 2: Commit**

```bash
cd /home/clinton/dev/Corbomite/libs/markoff-family
git add docs/phase-c-status.md
git commit -m "phase-c-status: C8 done; v0.9.1 tagged"
```

- [ ] **Step 3: Bump submodule pin on Corbomite**

```bash
cd /home/clinton/dev/Corbomite
git add libs/markoff-family
git commit -m "submodule: bump markoff for phase-c-status closeout"
```

### Task 7.5: Update PROJECT-STATE and decisions-archive

**Files:**
- Modify: `docs/PROJECT-STATE.md`
- Modify: `docs/decisions-archive.md`

- [ ] **Step 1: PROJECT-STATE current-focus replacement**

Replace the `## Current focus` paragraph with a 2-3 sentence update:

```markdown
## Current focus

**Markoff Phase C closed (again).** C8 ("Inline-ORC canonical coherence")
shipped end-to-end 2026-04-23, tag `v0.9.1`. Presentation-vs-content
invariant established; every ORC-touching mutation guarded; local↔canonical
translator live; terminal `Vault::saveDocument` refusal-on-ORC guard in
place. Next: pick from backlog — natural candidate is Cluster X (block-
substitution widget promotion), now unblocked.
```

Update `**Last updated:**` to a one-sentence summary pointing at the
decisions-archive entry.

- [ ] **Step 2: decisions-archive closeout**

At the top of `decisions-archive.md` (above the existing 2026-04-23 #1
entry that opened C8), add a new H2 entry:

```markdown
## 2026-04-23 — Phase C8 (Inline-ORC canonical coherence) done end-to-end; Markoff v0.9.1.

The inline-ORC canonical-coherence bug discovered earlier the same day is
now fixed and pinned. Phase C8 landed per the implementation plan at
`docs/superpowers/plans/2026-04-23-phase-c8-inline-orc-canonical-coherence.md`
across ~16 commits. Markoff tagged `v0.9.1`. Submodule pin bumped.

[Full invariants + file list: see phase-c-status activity log entry of the
same date.]

Cluster X expansion trigger satisfied — Phase C8 on master, regression
tests green. Cluster X brainstorm scheduled for a dedicated session.
```

- [ ] **Step 3: Commit**

```bash
cd /home/clinton/dev/Corbomite
git add docs/PROJECT-STATE.md docs/decisions-archive.md
git commit -m "docs: Phase C8 closeout — v0.9.1; Cluster X unblocked"
```

---

## Self-review

**Spec coverage:**
- §2 invariant 1 (presentation guard) → Phase 1 Tasks 1.1-1.9
- §2 invariant 2 (offset translation) → Phase 2 Tasks 2.1-2.4, Phase 3 Task 3.2
- §2 invariant 3 (insertedText ORC expansion) → Phase 3 Tasks 3.1-3.2
- §2 invariant 4 (Vault terminal guard) → Phase 5 Tasks 5.2-5.4
- §3.1 PresentationScope → Phase 1 Tasks 1.1-1.9
- §3.2 Substitution table → Phase 2 Tasks 2.1-2.4
- §3.3 Outbound hygiene → Phase 3 Tasks 3.1-3.2
- §3.4 Inbound strip-splice-reapply → Phase 4 Tasks 4.1-4.3
- §3.5 Debug Q_ASSERTs → Phase 5 Task 5.1
- §3.6 Vault terminal guard → Phase 5 Tasks 5.2-5.4
- §4 Substitution types (math, display math, mermaid, checkbox) → covered uniformly by Phase 1 guards + Phase 2 table
- §5.4 Tests (7 regression tests + 1 vault test) → Phase 6 Tasks 6.1-6.7 + Phase 5 Task 5.3
- §6 Rollout (6-commit shape) → actual count ~16 commits across 7 phases; sequencing preserves spec intent (presentation guard first, then translator, then wiring, then tests)
- §7 Follow-ups (Cluster X unblocked, fixture restore, tag) → Phase 7 Tasks 7.2-7.5

**Placeholder scan:** No `TBD` / `TODO` / `fill in` strings in any task body. Every code step shows exact code.

**Type consistency:** `SubstitutionTable`, `Substitution`, `CanonicalSnapshot`, `PresentationScope`, `localToCanonical`, `canonicalToLocal`, `localRangeToCanonical`, `isInsideOrcCanonicalSpan`, `snapshotViewStateAsCanonical`, `restoreViewStateFromCanonical`, `expandedTextForRange`, `saveFailed` — each used identically in header declaration, implementation, and consuming tests.

**Ambiguity check:** `canonicalToLocal`'s behavior when the input position lands inside an ORC's canonical span is specified twice consistently (returns the ORC's local position; §3.2 and Phase 2 Task 2.3 both).
