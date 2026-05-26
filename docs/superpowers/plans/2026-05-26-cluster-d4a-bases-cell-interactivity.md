# Cluster D.4a — Bases Cell Interactivity — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the interactive content *within* Bases cells live — click a wikilink to navigate, a checkbox to toggle, a tag to search, a URL to open — plus a right-click file context menu and drag-out-as-wikilink, while whitespace clicks still select and double-click still edits.

**Architecture:** A pure, widget-free `CellHitTest` helper owns the geometry of every interactive sub-region; `BasesCellDelegate::paint()` draws into those rects and `editorEvent()` hit-tests against them, so click targets never drift from what's drawn. The checkbox toggle rides the model's existing `setData`→`processFrontMatter` path; link/tag/URL clicks emit delegate signals that `BasesView` routes to leaf-navigation, host callbacks, or `QDesktopServices`. Drag mime comes from `BasesTreeModel::mimeData`.

**Tech Stack:** C++20, Qt6 (Widgets: QStyledItemDelegate, QTreeView, QMenu, QMimeData, QFontMetrics; Gui: QDesktopServices; Core), QtTest, CMake. Library: `libs/bases` (`Corbomite::Bases`). Spec: [`docs/superpowers/specs/2026-05-26-cluster-d4a-bases-cell-interactivity-design.md`](../specs/2026-05-26-cluster-d4a-bases-cell-interactivity-design.md).

---

## File structure

- **Create** `libs/bases/include/corbomite/bases/CellHitTest.h` + `src/CellHitTest.cpp` — pure geometry: `CellHit` struct, `hitTestCell`, rect helpers. No widgets.
- **Modify** `libs/bases/src/BasesCellDelegate.cpp` + `.h` — `editorEvent`, signals, paint refactor onto the rect helpers, tag-chip rendering, remove boolean editor.
- **Modify** `libs/bases/include/corbomite/bases/BasesTreeModel.h` + `src/BasesTreeModel.cpp` — `mimeTypes`/`mimeData`, `ItemIsDragEnabled` in `flags`.
- **Modify** `libs/bases/include/corbomite/bases/BasesView.h` + `src/BasesView.cpp` — connect delegate signals, link resolution, leaf navigation, context menu, drag enable, host callbacks.
- **Modify** `src/app/MainWindow.cpp` — set the new BasesView callbacks.
- **Modify** `libs/bases/CMakeLists.txt` — add `src/CellHitTest.cpp`.
- **Create test** `libs/bases/tests/tst_bases_cell_hittest.cpp`; **extend** `tst_bases_tree_model.cpp`; register in `libs/bases/tests/CMakeLists.txt`.

Build: `cmake --build --preset dev -j 10`. Test: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R <name> --output-on-failure`.

---

### Task 1: `CellHitTest` — pure geometry helper + unit test

**Files:**
- Create: `libs/bases/include/corbomite/bases/CellHitTest.h`, `libs/bases/src/CellHitTest.cpp`
- Modify: `libs/bases/CMakeLists.txt`
- Test: `libs/bases/tests/tst_bases_cell_hittest.cpp` (+ register)

- [ ] **Step 1: Write the failing test**

Create `libs/bases/tests/tst_bases_cell_hittest.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QFont>
#include <QFontMetrics>
#include "corbomite/bases/CellHitTest.h"
#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

class TestCellHitTest : public QObject
{
    Q_OBJECT
    QFont m_font;
private Q_SLOTS:
    void checkboxGlyphHitVsWhitespace() {
        const QRect cell(0, 0, 200, 24);
        QFontMetrics fm(m_font);
        auto v = std::make_shared<BooleanValue>(true);
        const QRect glyph = checkboxGlyphRect(cell);
        // a point inside the centered glyph -> Checkbox
        CellHit hit = hitTestCell(QStringLiteral("Boolean"), v, cell, glyph.center(), fm);
        QCOMPARE(int(hit.kind), int(CellHit::Checkbox));
        // a point far in the left margin (outside the centered glyph) -> Whitespace
        CellHit ws = hitTestCell(QStringLiteral("Boolean"), v, cell, QPoint(2, 12), fm);
        QCOMPARE(int(ws.kind), int(CellHit::Whitespace));
    }
    void linkTextHitVsTrailingWhitespace() {
        const QRect cell(0, 0, 400, 24);
        QFontMetrics fm(m_font);
        auto v = std::make_shared<LinkValue>(QStringLiteral("Ridley Scott"));
        // a point near the left (over the text) -> Link with the target as payload
        CellHit hit = hitTestCell(QStringLiteral("Link"), v, cell, QPoint(10, 12), fm);
        QCOMPARE(int(hit.kind), int(CellHit::Link));
        QCOMPARE(hit.payload, QStringLiteral("Ridley Scott"));
        // a point far to the right (past the text) -> Whitespace
        CellHit ws = hitTestCell(QStringLiteral("Link"), v, cell, QPoint(390, 12), fm);
        QCOMPARE(int(ws.kind), int(CellHit::Whitespace));
    }
    void urlHit() {
        const QRect cell(0, 0, 400, 24);
        QFontMetrics fm(m_font);
        auto v = std::make_shared<UrlValue>(QStringLiteral("https://example.com"));
        CellHit hit = hitTestCell(QStringLiteral("URL"), v, cell, QPoint(10, 12), fm);
        QCOMPARE(int(hit.kind), int(CellHit::Url));
        QCOMPARE(hit.payload, QStringLiteral("https://example.com"));
    }
    void tagChipIndexResolution() {
        const QRect cell(0, 0, 400, 24);
        QFontMetrics fm(m_font);
        QVector<ValuePtr> tags{ std::make_shared<TagValue>(QStringLiteral("sci-fi")),
                                std::make_shared<TagValue>(QStringLiteral("noir")),
                                std::make_shared<TagValue>(QStringLiteral("dystopia")) };
        auto list = std::make_shared<ListValue>(tags);
        const QVector<QRect> chips = tagChipRects(list, cell, fm);
        QCOMPARE(chips.size(), 3);
        // click inside chip 0 and chip 2 resolve to the right tag
        CellHit h0 = hitTestCell(QStringLiteral("List"), list, cell, chips[0].center(), fm);
        QCOMPARE(int(h0.kind), int(CellHit::Tag));
        QCOMPARE(h0.tagIndex, 0);
        QCOMPARE(h0.payload, QStringLiteral("sci-fi"));
        CellHit h2 = hitTestCell(QStringLiteral("List"), list, cell, chips[2].center(), fm);
        QCOMPARE(h2.tagIndex, 2);
        QCOMPARE(h2.payload, QStringLiteral("dystopia"));
    }
    void plainAndNullCellsAreWhitespace() {
        const QRect cell(0, 0, 200, 24);
        QFontMetrics fm(m_font);
        CellHit s = hitTestCell(QStringLiteral("String"),
            std::make_shared<StringValue>(QStringLiteral("hello")), cell, QPoint(5, 12), fm);
        QCOMPARE(int(s.kind), int(CellHit::Whitespace));
        CellHit n = hitTestCell(QStringLiteral("Number"), nullptr, cell, QPoint(5, 12), fm);
        QCOMPARE(int(n.kind), int(CellHit::Whitespace));
    }
};

QTEST_MAIN(TestCellHitTest)
#include "tst_bases_cell_hittest.moc"
```

- [ ] **Step 2: Register + confirm it fails to compile**

Append to `libs/bases/tests/CMakeLists.txt`:
```cmake
add_executable(tst_bases_cell_hittest tst_bases_cell_hittest.cpp)
add_test(NAME tst_bases_cell_hittest COMMAND tst_bases_cell_hittest)
target_link_libraries(tst_bases_cell_hittest PRIVATE Qt6::Test Qt6::Gui Corbomite::Bases)
```
Run: `cmake --preset dev >/dev/null && cmake --build --preset dev -j 10 --target tst_bases_cell_hittest`
Expected: FAILS (`CellHitTest.h` missing).

- [ ] **Step 3: Create the header**

Create `libs/bases/include/corbomite/bases/CellHitTest.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ValuePtr.h"

#include <QRect>
#include <QString>
#include <QVector>

class QFontMetrics;

namespace Corbomite::Bases {

/// What lies under a point inside a Bases cell. `Whitespace` means the caller
/// should fall back to default selection / double-click-to-edit behaviour.
struct CellHit {
    enum Kind { Whitespace, Checkbox, Link, Tag, Url } kind = Whitespace;
    int tagIndex = -1;     ///< index into the tag list when kind == Tag
    QString payload;       ///< Link: link target; Url: url; Tag: tag text
};

/// Centered, fixed-size square where the boolean ballot glyph is drawn.
QRect checkboxGlyphRect(const QRect &cellRect);
/// Left-aligned bounding rect of `text` (font-measured, clipped to the cell).
QRect linkTextRect(const QString &text, const QRect &cellRect, const QFontMetrics &fm);
QRect urlTextRect(const QString &text, const QRect &cellRect, const QFontMetrics &fm);
/// Per-tag chip rects, left-to-right, for a ListValue of TagValues (or a single
/// TagValue). Empty if `value` carries no tags.
QVector<QRect> tagChipRects(const ValuePtr &value, const QRect &cellRect, const QFontMetrics &fm);

/// Hit-test `point` (viewport coords, same space as `cellRect`) against the
/// interactive element the delegate paints for `valueType`/`value`.
CellHit hitTestCell(const QString &valueType, const ValuePtr &value,
                    const QRect &cellRect, const QPoint &point, const QFontMetrics &fm);

}  // namespace Corbomite::Bases
```

- [ ] **Step 4: Create the source**

Create `libs/bases/src/CellHitTest.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/CellHitTest.h"

#include "corbomite/bases/Values.h"

#include <QFontMetrics>

namespace Corbomite::Bases {

namespace {
constexpr int kGlyph = 18;      // checkbox glyph box
constexpr int kPad = 4;         // left text padding / chip inner padding
constexpr int kChipSpacing = 4; // gap between tag chips

// Pull the displayed text out of a StringValue subclass (Link/Url/Tag/String).
QString stringData(const ValuePtr &v) {
    if (auto *s = dynamic_cast<StringValue *>(v.get())) return s->data();
    return v ? v->toString() : QString{};
}
}  // namespace

QRect checkboxGlyphRect(const QRect &cellRect)
{
    QRect r(0, 0, kGlyph, kGlyph);
    r.moveCenter(cellRect.center());
    return r;
}

QRect linkTextRect(const QString &text, const QRect &cellRect, const QFontMetrics &fm)
{
    const int w = fm.horizontalAdvance(text);
    return QRect(cellRect.left() + kPad, cellRect.top(),
                 qMin(w, cellRect.width() - kPad), cellRect.height());
}

QRect urlTextRect(const QString &text, const QRect &cellRect, const QFontMetrics &fm)
{
    return linkTextRect(text, cellRect, fm);
}

QVector<QRect> tagChipRects(const ValuePtr &value, const QRect &cellRect, const QFontMetrics &fm)
{
    // Collect tag texts: a ListValue of TagValues, or a single TagValue.
    QStringList tags;
    if (auto *list = dynamic_cast<ListValue *>(value.get())) {
        for (const auto &e : list->data())
            if (auto *t = dynamic_cast<TagValue *>(e.get())) tags << t->data();
    } else if (auto *t = dynamic_cast<TagValue *>(value.get())) {
        tags << t->data();
    }

    QVector<QRect> rects;
    int x = cellRect.left() + kPad;
    const int h = qMin(cellRect.height() - 2, fm.height() + 2 * kPad);
    const int y = cellRect.top() + (cellRect.height() - h) / 2;
    for (const QString &tag : tags) {
        const int w = fm.horizontalAdvance(tag) + 2 * kPad;
        rects.append(QRect(x, y, w, h));
        x += w + kChipSpacing;
    }
    return rects;
}

CellHit hitTestCell(const QString &valueType, const ValuePtr &value,
                    const QRect &cellRect, const QPoint &point, const QFontMetrics &fm)
{
    if (!value) return {};

    if (valueType == QLatin1String("Boolean")) {
        if (checkboxGlyphRect(cellRect).contains(point))
            return { CellHit::Checkbox, -1, {} };
        return {};
    }
    if (valueType == QLatin1String("Link")) {
        const QString target = stringData(value);
        if (linkTextRect(target, cellRect, fm).contains(point))
            return { CellHit::Link, -1, target };
        return {};
    }
    if (valueType == QLatin1String("URL")) {
        const QString url = stringData(value);
        if (urlTextRect(url, cellRect, fm).contains(point))
            return { CellHit::Url, -1, url };
        return {};
    }
    if (valueType == QLatin1String("Tag") || valueType == QLatin1String("List")) {
        const QVector<QRect> chips = tagChipRects(value, cellRect, fm);
        QStringList tags;
        if (auto *list = dynamic_cast<ListValue *>(value.get())) {
            for (const auto &e : list->data())
                if (auto *t = dynamic_cast<TagValue *>(e.get())) tags << t->data();
        } else if (auto *t = dynamic_cast<TagValue *>(value.get())) {
            tags << t->data();
        }
        for (int i = 0; i < chips.size(); ++i)
            if (chips[i].contains(point))
                return { CellHit::Tag, i, tags.value(i) };
        return {};
    }
    return {};
}

}  // namespace Corbomite::Bases
```

Add `src/CellHitTest.cpp` to the `add_library(corbomite-bases ...)` list in `libs/bases/CMakeLists.txt` (after `src/SortCycle.cpp`).

- [ ] **Step 5: Build + run the test**

Run: `cmake --build --preset dev -j 10 --target tst_bases_cell_hittest && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_bases_cell_hittest --output-on-failure`
Expected: all 5 slots PASS.

- [ ] **Step 6: Commit**
```bash
git add libs/bases/include/corbomite/bases/CellHitTest.h libs/bases/src/CellHitTest.cpp libs/bases/CMakeLists.txt libs/bases/tests/tst_bases_cell_hittest.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): CellHitTest — pure hit-testing for interactive cell content"
```
End the commit body with:
Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>

---

### Task 2: `BasesCellDelegate` — editorEvent + signals + paint onto shared rects

**Files:**
- Modify: `libs/bases/include/corbomite/bases/BasesCellDelegate.h`, `libs/bases/src/BasesCellDelegate.cpp`

GUI behaviour; the hit-test logic is covered by Task 1. Verified by build (and launch in Task 4).

- [ ] **Step 1: Header — add signals + editorEvent**

In `libs/bases/include/corbomite/bases/BasesCellDelegate.h`, add inside the class (after `paint`):
```cpp
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override;

Q_SIGNALS:
    void linkClicked(const QString &target, Qt::KeyboardModifiers mods);
    void tagClicked(const QString &tag);
    void urlClicked(const QString &url);
```
Add `#include <QEvent>` is not needed in the header; keep includes minimal (`<QStyledItemDelegate>` already present; `Qt::KeyboardModifiers` comes via Qt namespace).

- [ ] **Step 2: Source — includes + editorEvent + paint refactor**

In `libs/bases/src/BasesCellDelegate.cpp`:

Add includes:
```cpp
#include "corbomite/bases/CellHitTest.h"
#include <QMouseEvent>
```

Add the `editorEvent` implementation (anywhere after `paint`):
```cpp
bool BasesCellDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                    const QStyleOptionViewItem &option,
                                    const QModelIndex &index)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        const QString type = index.data(BasesTreeModel::ValueTypeRole).toString();
        const ValuePtr value = index.data(BasesTreeModel::ValuePtrRole).value<ValuePtr>();
        const QFontMetrics fm(option.font);
        const CellHit hit = hitTestCell(type, value, option.rect, me->position().toPoint(), fm);

        if (hit.kind == CellHit::Checkbox && me->button() == Qt::LeftButton) {
            auto *b = dynamic_cast<BooleanValue *>(value.get());
            model->setData(index, !(b && b->data()), Qt::EditRole);
            return true;
        }
        if (hit.kind == CellHit::Link
            && (me->button() == Qt::LeftButton || me->button() == Qt::MiddleButton)) {
            // Middle-click behaves like Ctrl+click (open in new tab).
            Qt::KeyboardModifiers mods = me->modifiers();
            if (me->button() == Qt::MiddleButton) mods |= Qt::ControlModifier;
            Q_EMIT linkClicked(hit.payload, mods);
            return true;
        }
        if (hit.kind == CellHit::Tag && me->button() == Qt::LeftButton) {
            Q_EMIT tagClicked(hit.payload);
            return true;
        }
        if (hit.kind == CellHit::Url && me->button() == Qt::LeftButton) {
            Q_EMIT urlClicked(hit.payload);
            return true;
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
```

Refactor `paint()` so the boolean, link, and tag drawing use the shared rect helpers (so paint and hit-test stay in lock-step). Replace the existing `Boolean` paint branch's `drawText(option.rect, Qt::AlignCenter, glyph)` with a draw into `checkboxGlyphRect(option.rect)`:
```cpp
    if (type == QLatin1String("Boolean")) {
        const auto valueVar = index.data(BasesTreeModel::ValuePtrRole);
        ValuePtr v = valueVar.value<ValuePtr>();
        auto *b = dynamic_cast<BooleanValue *>(v.get());
        painter->save();
        const QString glyph = b && b->data()
            ? QStringLiteral("☑") : QStringLiteral("☐");
        painter->drawText(checkboxGlyphRect(option.rect), Qt::AlignCenter, glyph);
        painter->restore();
        return;
    }
```
Add a `Link`/`URL` paint branch (before the generic fallback) that draws the text into `linkTextRect`/`urlTextRect` with a link-styled colour so it reads as clickable:
```cpp
    if (type == QLatin1String("Link") || type == QLatin1String("URL")) {
        auto *s = dynamic_cast<StringValue *>(index.data(BasesTreeModel::ValuePtrRole)
                                                   .value<ValuePtr>().get());
        const QString text = s ? s->data() : index.data(Qt::DisplayRole).toString();
        const QFontMetrics fm(option.font);
        painter->save();
        painter->setPen(option.palette.link().color());
        painter->drawText(linkTextRect(text, option.rect, fm),
                          Qt::AlignVCenter | Qt::AlignLeft, text);
        painter->restore();
        return;
    }
```
Add a `List`/`Tag` chip paint branch (before the generic fallback) using `tagChipRects`:
```cpp
    {
        const ValuePtr v = index.data(BasesTreeModel::ValuePtrRole).value<ValuePtr>();
        const QFontMetrics fm(option.font);
        const QVector<QRect> chips = tagChipRects(v, option.rect, fm);
        if (!chips.isEmpty()) {
            QStringList tags;
            if (auto *list = dynamic_cast<ListValue *>(v.get()))
                for (const auto &e : list->data())
                    if (auto *t = dynamic_cast<TagValue *>(e.get())) tags << t->data();
            else if (auto *t = dynamic_cast<TagValue *>(v.get())) tags << t->data();
            painter->save();
            for (int i = 0; i < chips.size() && i < tags.size(); ++i) {
                painter->setBrush(option.palette.alternateBase());
                painter->setPen(Qt::NoPen);
                painter->drawRoundedRect(chips[i], 6, 6);
                painter->setPen(option.palette.text().color());
                painter->drawText(chips[i], Qt::AlignCenter, tags[i]);
            }
            painter->restore();
            return;
        }
    }
```
Place these branches in `paint()` after the existing `Icon`/`Image`/`HTML` branches and after the `Error` branch, **before** the final `QStyledItemDelegate::paint(painter, option, index);` fallback. (Keep the group-row branch at the very top untouched.)

Remove the boolean branch from `createEditor`, `setEditorData`, and `setModelData` (single-click toggle replaces the `QCheckBox` editor). Concretely: delete the `if (type == "Boolean") { ... QCheckBox ... }` block from `createEditor`; delete the `if (auto *cb = qobject_cast<QCheckBox *>(editor))` blocks from `setEditorData` and `setModelData`; remove the now-unused `#include <QCheckBox>`.

- [ ] **Step 3: Build**

Run: `cmake --build --preset dev -j 10 --target corbomite-bases`
Expected: compiles clean.

- [ ] **Step 4: Run the bases suite (no regressions)**

Run: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_bases --output-on-failure`
Expected: all green (existing delegate smoke test still passes; editorEvent untested here but compiled).

- [ ] **Step 5: Commit**
```bash
git add libs/bases/include/corbomite/bases/BasesCellDelegate.h libs/bases/src/BasesCellDelegate.cpp
git commit -m "feat(bases): delegate editorEvent routes clicks; link/tag chip rendering; drop bool editor"
```
End with the Co-Authored-By line.

---

### Task 3: `BasesTreeModel` — drag mime (`[[wikilink]]`) + flags

**Files:**
- Modify: `libs/bases/include/corbomite/bases/BasesTreeModel.h`, `libs/bases/src/BasesTreeModel.cpp`
- Test: `libs/bases/tests/tst_bases_tree_model.cpp`

- [ ] **Step 1: Add a failing test**

In `libs/bases/tests/tst_bases_tree_model.cpp`, add a slot to `TestBasesTreeModel` (reusing the existing `grp`/`note` helpers + `BasesQuery q`). It builds a real entry over a temp vault so `entryForIndex`→`file()` resolves:
```cpp
    void mimeDataYieldsWikilinkForEntries() {
        // One flat group with a single entry backed by a real TFile.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        { QFile f(dir.path() + QStringLiteral("/Alien.md"));
          QVERIFY(f.open(QIODevice::WriteOnly)); f.write("# Alien\n"); }
        Corbomite::FileSystemAdapter adapter;
        Corbomite::Vault vault(&adapter);
        vault.load(dir.path());
        Corbomite::TFile *tf = vault.getFileByPath(QStringLiteral("Alien.md"));
        QVERIFY(tf);

        BasesQuery q;
        BasesEntryGroup g;
        g.entries.push_back(std::make_shared<BasesEntry>(&vault, nullptr, tf, tf, q));
        BasesTreeModel m(nullptr, nullptr);
        m.populateForTesting({ g }, { note("title") });   // single keyless group -> flat

        const QModelIndex idx = m.index(0, 0, QModelIndex());
        QVERIFY(idx.isValid());
        QVERIFY(m.flags(idx) & Qt::ItemIsDragEnabled);
        std::unique_ptr<QMimeData> md(m.mimeData({ idx }));
        QVERIFY(md);
        QCOMPARE(md->text(), QStringLiteral("[[Alien]]"));
    }
```
Add includes at the top of the test file if missing: `#include <QTemporaryDir>`, `#include <QMimeData>`, `#include "corbomite/storage/FileSystemAdapter.h"`, `#include "corbomite/vault/Vault.h"`, `#include "corbomite/vault/TFile.h"`, `#include "corbomite/bases/BasesEntry.h"`. Ensure the test target links `Corbomite::Vault Corbomite::Storage` (see Step 2).

- [ ] **Step 2: Ensure the test target links Vault + Storage**

In `libs/bases/tests/CMakeLists.txt`, find the `tst_bases_tree_model` target and extend its link line to:
```cmake
target_link_libraries(tst_bases_tree_model PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases Corbomite::Vault Corbomite::Storage)
```
Run: `cmake --preset dev >/dev/null && cmake --build --preset dev -j 10 --target tst_bases_tree_model`
Expected: FAILS (`mimeData` returns base-class default / no `[[Alien]]`).

- [ ] **Step 3: Declare the overrides**

In `libs/bases/include/corbomite/bases/BasesTreeModel.h`, add to the public section (near `flags`):
```cpp
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
```
Add `#include <QStringList>` if not already present (it is via Qt headers; safe to add).

- [ ] **Step 4: Implement + extend flags**

In `libs/bases/src/BasesTreeModel.cpp`, add includes:
```cpp
#include "corbomite/vault/TFile.h"
#include <QMimeData>
#include <QFileInfo>
```
Add the implementations:
```cpp
QStringList BasesTreeModel::mimeTypes() const
{
    return { QStringLiteral("text/plain") };
}

QMimeData *BasesTreeModel::mimeData(const QModelIndexList &indexes) const
{
    QStringList links;
    QSet<BasesEntry *> seen;
    for (const QModelIndex &idx : indexes) {
        if (idx.column() != 0) continue;          // one entry per row
        BasesEntry *e = entryForIndex(idx);
        if (!e || !e->file() || seen.contains(e)) continue;
        seen.insert(e);
        const QString base = QFileInfo(e->file()->path).completeBaseName();
        if (!base.isEmpty()) links << QStringLiteral("[[%1]]").arg(base);
    }
    if (links.isEmpty()) return nullptr;
    auto *md = new QMimeData;
    md->setText(links.join(QLatin1Char('\n')));
    return md;
}
```
Add `#include <QSet>` near the top if not present. Extend `flags()` so entry (non-group) rows are draggable — find the existing `flags()` body and add `f |= Qt::ItemIsDragEnabled;` in the non-group branch:
```cpp
Qt::ItemFlags BasesTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (!isGroupRow(index)) {
        f |= Qt::ItemIsDragEnabled;
        if (propertyAt(index.column()).kind == PropertyKind::Note)
            f |= Qt::ItemIsEditable;
    }
    return f;
}
```
(Adjust to match the exact current `flags()` text — the key addition is `ItemIsDragEnabled` for non-group rows.)

- [ ] **Step 5: Build + run**

Run: `cmake --build --preset dev -j 10 --target tst_bases_tree_model && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_bases_tree_model --output-on-failure`
Expected: all slots PASS, including `mimeDataYieldsWikilinkForEntries`.

- [ ] **Step 6: Commit**
```bash
git add libs/bases/include/corbomite/bases/BasesTreeModel.h libs/bases/src/BasesTreeModel.cpp libs/bases/tests/tst_bases_tree_model.cpp libs/bases/tests/CMakeLists.txt
git commit -m "feat(bases): BasesTreeModel drag mime — entry rows export [[wikilink]]"
```
End with the Co-Authored-By line.

---

### Task 4: `BasesView` — wire interactions (navigate / search / open URL / context menu / drag)

**Files:**
- Modify: `libs/bases/include/corbomite/bases/BasesView.h`, `libs/bases/src/BasesView.cpp`

- [ ] **Step 1: Header — host callbacks + helper decls**

In `BasesView.h`, add to the public section (near `setServices`):
```cpp
    void setOpenInNewTabHandler(std::function<void(const QString &path)> cb) { m_openInNewTab = std::move(cb); }
    void setTagSearchHandler(std::function<void(const QString &tag)> cb) { m_searchTag = std::move(cb); }
    void setRenamePrompt(std::function<void(const QString &path)> cb) { m_promptRename = std::move(cb); }
    void setDeletePrompt(std::function<void(const QString &path)> cb) { m_promptDelete = std::move(cb); }
```
Add `#include <functional>` and `#include <QString>` if not present. Add private slots:
```cpp
    void onLinkClicked(const QString &target, Qt::KeyboardModifiers mods);
    void onContextMenu(const QPoint &pos);
```
Add private helper + members:
```cpp
    QString resolveLink(const QString &target) const;   // wikilink target -> vault path ("" if unresolved)

    std::function<void(const QString &)> m_openInNewTab;
    std::function<void(const QString &)> m_searchTag;
    std::function<void(const QString &)> m_promptRename;
    std::function<void(const QString &)> m_promptDelete;
```

- [ ] **Step 2: Source — connect delegate signals + drag + context menu policy**

In `BasesView.cpp` add includes:
```cpp
#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/BasesVaultResolver.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/vault/TFile.h"
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QMenu>
#include <QUrl>
```
In the constructor, after `m_delegate = new BasesCellDelegate(this);` and `m_table->setItemDelegate(m_delegate);`, connect the signals + enable drag + context menu:
```cpp
    connect(m_delegate, &BasesCellDelegate::linkClicked,
            this, &BasesView::onLinkClicked);
    connect(m_delegate, &BasesCellDelegate::tagClicked, this, [this](const QString &tag) {
        if (m_searchTag) m_searchTag(tag);
    });
    connect(m_delegate, &BasesCellDelegate::urlClicked, this, [](const QString &url) {
        QDesktopServices::openUrl(QUrl(url));
    });

    m_table->setDragEnabled(true);
    m_table->setDragDropMode(QAbstractItemView::DragOnly);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QWidget::customContextMenuRequested,
            this, &BasesView::onContextMenu);
```

- [ ] **Step 3: Source — implement onLinkClicked + resolveLink**

Add to `BasesView.cpp`:
```cpp
QString BasesView::resolveLink(const QString &target) const
{
    if (!m_vault) return {};
    BasesVaultResolver resolver(m_vault, m_cache);
    const QString src = m_query ? m_query->filePath : QString{};
    return resolver.resolveLinkTarget(target, src);   // "" if unresolved
}

void BasesView::onLinkClicked(const QString &target, Qt::KeyboardModifiers mods)
{
    const QString path = resolveLink(target);
    if (path.isEmpty()) return;                       // unresolved -> no-op
    const bool newTab = mods.testFlag(Qt::ControlModifier)
                     || mods.testFlag(Qt::MetaModifier);
    if (newTab) {
        if (m_openInNewTab) m_openInNewTab(path);
        return;
    }
    // Same-tab navigation: drive the base's own leaf (history-aware).
    if (auto *lf = leaf()) {
        QJsonObject state;
        state[QStringLiteral("type")] = QStringLiteral("markdown");
        state[QStringLiteral("state")] = QJsonObject{{QStringLiteral("file"), path}};
        lf->navigate(state);
    } else if (m_openInNewTab) {
        m_openInNewTab(path);                         // fallback
    }
}
```
Add `#include <QJsonObject>` to `BasesView.cpp` if not present.

- [ ] **Step 4: Source — implement onContextMenu**

Add to `BasesView.cpp`:
```cpp
void BasesView::onContextMenu(const QPoint &pos)
{
    if (!m_model) return;
    const QModelIndex idx = m_table->indexAt(pos);
    if (!idx.isValid() || m_model->isGroupRow(idx)) return;

    QMenu menu(this);
    // Resolve the row's note (entry's own file) for file actions.
    QString notePath;
    if (BasesEntry *e = m_model->entryForIndex(idx); e && e->file())
        notePath = e->file()->path;

    // If the clicked cell is a wikilink, prefer its target.
    const QString type = idx.data(BasesTreeModel::ValueTypeRole).toString();
    if (type == QLatin1String("Link")) {
        const auto v = idx.data(BasesTreeModel::ValuePtrRole).value<ValuePtr>();
        if (auto *s = dynamic_cast<StringValue *>(v.get())) {
            const QString resolved = resolveLink(s->data());
            if (!resolved.isEmpty()) notePath = resolved;
        }
    }

    if (!notePath.isEmpty()) {
        const QString path = notePath;
        menu.addAction(i18n("Open"), this, [this, path]() {
            if (auto *lf = leaf()) {
                QJsonObject st; st[QStringLiteral("type")] = QStringLiteral("markdown");
                st[QStringLiteral("state")] = QJsonObject{{QStringLiteral("file"), path}};
                lf->navigate(st);
            }
        });
        menu.addAction(i18n("Open in new tab"), this, [this, path]() {
            if (m_openInNewTab) m_openInNewTab(path);
        });
        menu.addAction(i18n("Copy as wikilink"), this, [path]() {
            const QString base = QFileInfo(path).completeBaseName();
            QApplication::clipboard()->setText(QStringLiteral("[[%1]]").arg(base));
        });
        menu.addSeparator();
        menu.addAction(i18n("Rename…"), this, [this, path]() {
            if (m_promptRename) m_promptRename(path);
        });
        menu.addAction(i18n("Delete"), this, [this, path]() {
            if (m_promptDelete) m_promptDelete(path);
        });
    } else {
        const QString display = idx.data(Qt::DisplayRole).toString();
        menu.addAction(i18n("Copy value"), this, [display]() {
            QApplication::clipboard()->setText(display);
        });
    }
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}
```
Add `#include <QFileInfo>` and `#include <QJsonObject>` to `BasesView.cpp`.

- [ ] **Step 5: Build + launch verify**

Run: `cmake --build --preset dev -j 10`
Then (with the films vault, which shows the `director` wikilink column):
`./build-dev/bin/Corbomite testvaults/films-vault`
- Click a `director` link's text → the base tab navigates to the director note; the Back button (header) returns to the base.
- Ctrl-click (or middle-click) a director link → opens in a new tab (requires Task 5 wiring; until then it no-ops gracefully).
- Right-click a director cell → Open / Open in new tab / Copy as wikilink / Rename… / Delete; right-click a plain cell → Copy value.
- Drag a row onto a markdown note → drops `[[FilmName]]`.
Expected: same-tab navigation + drag work now; new-tab/rename/delete/tag-search become live after Task 5.

- [ ] **Step 6: Run the bases suite**

Run: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_bases --output-on-failure`
Expected: all green.

- [ ] **Step 7: Commit**
```bash
git add libs/bases/include/corbomite/bases/BasesView.h libs/bases/src/BasesView.cpp
git commit -m "feat(bases): wire cell clicks — link navigate, tag search, url open, context menu, drag"
```
End with the Co-Authored-By line.

---

### Task 5: `MainWindow` — provide the host callbacks

**Files:**
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Set the callbacks in the BasesView branch**

In `src/app/MainWindow.cpp`, find the `if (auto *bv = qobject_cast<Corbomite::Bases::BasesView *>(view))` block in `propagateServicesToView` (where `setServices` + `setCurrentFile` are wired). After the existing wiring, add:
```cpp
        bv->setOpenInNewTabHandler([this](const QString &path) {
            openFileInWorkspace(path);
        });
        bv->setTagSearchHandler([this](const QString &tag) {
            // Reuse the existing search entry point; tag form "tag:#name".
            showSearchForQuery(QStringLiteral("tag:%1").arg(tag));
        });
        bv->setRenamePrompt([this](const QString &path) {
            if (!m_vaultObj || !m_fileManagerProxy) return;
            if (auto *f = m_vaultObj->getAbstractFileByPath(path))
                m_fileManagerProxy->promptForFileRename(f, this);
        });
        bv->setDeletePrompt([this](const QString &path) {
            if (!m_vaultObj || !m_fileManagerProxy) return;
            if (auto *f = m_vaultObj->getAbstractFileByPath(path))
                m_fileManagerProxy->promptForDeletion(f, this);
        });
```
> **Verify the exact symbols before writing:** confirm the search entry point name (search for an existing call that opens search with a query — e.g. `showSearchForQuery`, `showQuickSwitcher`, or the search plugin's API; use whatever the codebase already exposes, falling back to a no-op lambda with a `// TODO` *only if no search entry exists*). Confirm the FileManagerProxy member name (`m_fileManagerProxy` vs `m_fileManager`); `promptForFileRename`/`promptForDeletion` live on `FileManagerProxy`. If only a raw `FileManager` is available, use its rename/delete methods with a confirmation, or route through the existing file-explorer delete/rename path.

- [ ] **Step 2: Build + launch verify**

Run: `cmake --build --preset dev -j 10`
Then `./build-dev/bin/Corbomite testvaults/films-vault`:
- Ctrl/middle-click a director link → opens in a new tab.
- Right-click a director cell → Rename…/Delete actually drive the validating prompts.
- (Add `note.tags` to a view via the Properties menu, then click a tag → a tag search opens.)
Expected: all cell interactions fully live.

- [ ] **Step 3: Commit**
```bash
git add src/app/MainWindow.cpp
git commit -m "feat(app): wire BasesView open-in-new-tab / tag-search / rename / delete callbacks"
```
End with the Co-Authored-By line.

---

### Task 6: Full suite + close-out

- [ ] **Step 1: Full bases suite**

Run: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_bases --output-on-failure`
Expected: all green (`tst_bases_cell_hittest` + `tst_bases_tree_model` + the rest).

- [ ] **Step 2: Full tree build (no new regressions)**

Run: `cmake --build --preset dev -j 10`
Expected: clean. (Pre-existing foundation-port failures elsewhere are unrelated.)

- [ ] **Step 3: Launch smoke (no crash, no layout warning)**

Run: `timeout 12 ./build-dev/bin/Corbomite testvaults/films-vault >/tmp/d4a.log 2>&1; grep -ciE "segfault|already has a layout" /tmp/d4a.log`
Expected: `0`. Manually exercise: link navigate + Back, Ctrl-click new tab, checkbox toggle on a boolean column (add `note.watched` to a view via Properties menu — it's already in All Films), right-click menu, drag-out.

- [ ] **Step 4: Update tracking docs**

Per CONTRIBUTING-OPS Ritual 3:
- `docs/PROJECT-STATE.md` — D row: note D.4a (cell interactivity) done; D.4b (export/+New), D.4c (undo), formula editor + filter builder remain. Add a one-line Recent-decisions entry.
- `docs/superpowers/plans/INDEX.md` — D row status reflects D.4a done.
- `docs/decisions-archive.md` — append a dated D.4a closeout paragraph (note hover-preview still deferred on the Markoff dependency).

- [ ] **Step 5: Commit the close-out**
```bash
git add docs/PROJECT-STATE.md docs/superpowers/plans/INDEX.md docs/decisions-archive.md
git commit -m "docs(tracking): close out Cluster D.4a (Bases cell interactivity)"
```
End with the Co-Authored-By line.

---

## Definition of done

- `CellHitTest` exists; `tst_bases_cell_hittest` passes (checkbox/link/url/tag/whitespace).
- Delegate `editorEvent` routes clicks by hit-test; link/tag/url emit signals; checkbox toggles via `setData`; whitespace falls through to select/edit.
- Clicking a wikilink navigates the base's leaf (Back returns); Ctrl/Cmd/middle-click opens a new tab; tag click searches; URL click opens the browser.
- Right-click yields the file context menu (link/row) or Copy value (plain cell).
- Dragging an entry row produces `[[wikilink]]`; `tst_bases_tree_model` covers `mimeData` + `ItemIsDragEnabled`.
- Full `libs/bases` suite green; clean build; no crash / no layout warning on launch; tracking docs updated.
- Hover-preview explicitly deferred + documented.

## Notes / risks

- **paint/hit-test parity:** all element geometry lives in `CellHitTest`; `paint()` must call the same helpers. Never compute element rects independently in `paint()`.
- **Host symbol verification (Task 5):** confirm `showSearchForQuery`/search entry point + `m_fileManagerProxy` names against the real `MainWindow` before writing; fall back to existing equivalents.
- **Middle-click reliability:** if `editorEvent` doesn't receive middle-button releases on the platform, new-tab still works via Ctrl/Cmd-click; document if so.
- **Link resolution:** reuse `BasesVaultResolver`; unresolved links are no-ops (no note creation this slice).
- **Tag visibility:** the default `Films.base` shows `note.director` (link) but not `note.tags`; tag-click is verified by adding `note.tags` to a view via the D.3 Properties menu.
