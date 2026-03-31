# Quick Switcher & Command Palette — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Obsidian-style Quick Switcher (Ctrl+O) and Command Palette (Ctrl+P) for keyboard-driven note navigation and command discovery.

**Architecture:** Command Palette uses KDE's built-in KCommandBar widget (zero custom UI). Quick Switcher is a custom QFrame popup with QLineEdit + QTreeView backed by a fuzzy-matching proxy model using KFuzzyMatcher from KCoreAddons.

**Tech Stack:** C++20, Qt6 Widgets, KFuzzyMatcher (KCoreAddons), KCommandBar (KConfigWidgets)

**Spec:** `docs/superpowers/specs/2026-03-31-quick-switcher-command-palette-design.md`

**Current state:** Phase 1 complete, 12 tests passing. App opens vaults, browses files, edits markdown in tabs.

---

### Task 1: QuickSwitcherModel with Fuzzy Matching

**Files:**
- Create: `src/dialogs/QuickSwitcherModel.h`
- Create: `src/dialogs/QuickSwitcherModel.cpp`
- Create: `tests/dialogs/tst_quickswitchermodel.cpp`
- Create: `tests/dialogs/CMakeLists.txt`
- Modify: `CMakeLists.txt` (add test subdirectory)

- [ ] **Step 1: Write QuickSwitcherModel tests**

`tests/dialogs/tst_quickswitchermodel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "dialogs/QuickSwitcherModel.h"
#include "corbomite/core/NoteMeta.h"

class TestQuickSwitcherModel : public QObject {
    Q_OBJECT

    QVector<Corbomite::NoteMeta> makeNotes(const QStringList &paths)
    {
        QVector<Corbomite::NoteMeta> notes;
        for (const auto &p : paths) {
            notes.append(Corbomite::NoteMeta::fromRelativePath(p));
        }
        return notes;
    }

private Q_SLOTS:
    void testPopulatesFromNotes()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({
            QStringLiteral("alpha.md"),
            QStringLiteral("beta.md")
        }));
        QCOMPARE(model.rowCount(), 2);
    }

    void testNoteNameRole()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({QStringLiteral("folder/My Note.md")}));

        auto idx = model.index(0, 0);
        QCOMPARE(idx.data(Corbomite::QuickSwitcherModel::NoteNameRole).toString(),
                 QStringLiteral("My Note"));
    }

    void testFolderPathRole()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({QStringLiteral("Projects/Work/task.md")}));

        auto idx = model.index(0, 0);
        QCOMPARE(idx.data(Corbomite::QuickSwitcherModel::FolderPathRole).toString(),
                 QStringLiteral("Projects/Work"));
    }

    void testFolderPathEmptyForRoot()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({QStringLiteral("root-note.md")}));

        auto idx = model.index(0, 0);
        QCOMPARE(idx.data(Corbomite::QuickSwitcherModel::FolderPathRole).toString(),
                 QString());
    }

    void testNotePathRole()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({QStringLiteral("folder/note.md")}));

        auto idx = model.index(0, 0);
        QCOMPARE(idx.data(Corbomite::QuickSwitcherModel::NotePathRole).toString(),
                 QStringLiteral("folder/note.md"));
    }

    void testDisplayRoleIsNoteName()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({QStringLiteral("My Note.md")}));

        auto idx = model.index(0, 0);
        QCOMPARE(idx.data(Qt::DisplayRole).toString(), QStringLiteral("My Note"));
    }

    void testEmptyVault()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes({});
        QCOMPARE(model.rowCount(), 0);
    }

    void testCanvasFilesIncluded()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({
            QStringLiteral("note.md"),
            QStringLiteral("board.canvas")
        }));
        QCOMPARE(model.rowCount(), 2);

        // Canvas name strips .canvas extension
        auto idx = model.index(1, 0);
        QCOMPARE(idx.data(Corbomite::QuickSwitcherModel::NoteNameRole).toString(),
                 QStringLiteral("board"));
    }

    void testRecentPathsOrderedFirst()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({
            QStringLiteral("alpha.md"),
            QStringLiteral("beta.md"),
            QStringLiteral("gamma.md"),
        }));
        model.setRecentPaths({
            QStringLiteral("gamma.md"),
            QStringLiteral("alpha.md"),
        });

        // When no filter applied, recent paths should appear first
        // Check that gamma and alpha have IsRecentRole = true
        bool foundGammaRecent = false;
        bool foundBetaNotRecent = false;
        for (int i = 0; i < model.rowCount(); ++i) {
            auto idx = model.index(i, 0);
            QString path = idx.data(Corbomite::QuickSwitcherModel::NotePathRole).toString();
            bool recent = idx.data(Corbomite::QuickSwitcherModel::IsRecentRole).toBool();
            if (path == QStringLiteral("gamma.md")) foundGammaRecent = recent;
            if (path == QStringLiteral("beta.md")) foundBetaNotRecent = !recent;
        }
        QVERIFY(foundGammaRecent);
        QVERIFY(foundBetaNotRecent);
    }
};

QTEST_MAIN(TestQuickSwitcherModel)
#include "tst_quickswitchermodel.moc"
```

- [ ] **Step 2: Create test CMakeLists**

`tests/dialogs/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(Corbomite_DialogTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_quickswitchermodel tst_quickswitchermodel.cpp)
add_test(NAME tst_quickswitchermodel COMMAND tst_quickswitchermodel)
target_link_libraries(tst_quickswitchermodel PRIVATE Qt6::Test CorbomiteApp)
set_tests_properties(tst_quickswitchermodel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

Add to root `CMakeLists.txt` after the other test subdirectories:
```cmake
add_subdirectory(tests/dialogs)
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -5`

Expected: Build fails — QuickSwitcherModel doesn't exist yet.

- [ ] **Step 4: Implement QuickSwitcherModel**

`src/dialogs/QuickSwitcherModel.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QSet>
#include "corbomite/core/NoteMeta.h"

namespace Corbomite {

class QuickSwitcherModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        NotePathRole = Qt::UserRole + 1,
        NoteNameRole,
        FolderPathRole,
        IsRecentRole
    };

    explicit QuickSwitcherModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setNotes(const QVector<NoteMeta> &notes);
    void setRecentPaths(const QStringList &recentPaths);

    // Future: add setAliases() for frontmatter alias matching

private:
    struct Entry {
        QString relativePath;  // e.g. "folder/note.md"
        QString name;          // e.g. "note"
        QString folder;        // e.g. "folder" or ""
        bool isRecent = false;
    };

    QVector<Entry> m_entries;
    QSet<QString> m_recentSet;
};

} // namespace Corbomite
```

`src/dialogs/QuickSwitcherModel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "QuickSwitcherModel.h"

namespace Corbomite {

QuickSwitcherModel::QuickSwitcherModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int QuickSwitcherModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_entries.size());
}

QVariant QuickSwitcherModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) return {};

    const auto &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NoteNameRole:
        return entry.name;
    case NotePathRole:
        return entry.relativePath;
    case FolderPathRole:
        return entry.folder;
    case IsRecentRole:
        return entry.isRecent;
    }
    return {};
}

void QuickSwitcherModel::setNotes(const QVector<NoteMeta> &notes)
{
    beginResetModel();
    m_entries.clear();
    m_entries.reserve(notes.size());

    for (const auto &meta : notes) {
        Entry entry;
        entry.relativePath = meta.relativePath;
        entry.name = meta.nameFromPath();

        int lastSlash = meta.relativePath.lastIndexOf(QLatin1Char('/'));
        entry.folder = lastSlash > 0 ? meta.relativePath.left(lastSlash) : QString();
        entry.isRecent = m_recentSet.contains(meta.relativePath);

        m_entries.append(entry);
    }
    endResetModel();
}

void QuickSwitcherModel::setRecentPaths(const QStringList &recentPaths)
{
    m_recentSet = QSet<QString>(recentPaths.begin(), recentPaths.end());

    // Update isRecent flags on existing entries
    for (auto &entry : m_entries) {
        entry.isRecent = m_recentSet.contains(entry.relativePath);
    }

    if (!m_entries.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(m_entries.size() - 1), {IsRecentRole});
    }
}

} // namespace Corbomite
```

- [ ] **Step 5: Add QuickSwitcherModel.cpp to src/CMakeLists.txt**

Add `dialogs/QuickSwitcherModel.cpp` to the CorbomiteApp source list (after `dialogs/SettingsDialog.cpp`).

- [ ] **Step 6: Build and run tests**

Run:
```bash
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R tst_quickswitchermodel --output-on-failure
```

Expected: All tests PASS.

- [ ] **Step 7: Verify all existing tests still pass**

Run: `cd build && ctest --output-on-failure`

Expected: All 13 tests pass (12 existing + 1 new).

- [ ] **Step 8: Commit**

```bash
git add src/dialogs/QuickSwitcherModel.h src/dialogs/QuickSwitcherModel.cpp \
        tests/dialogs/ src/CMakeLists.txt CMakeLists.txt
git commit -m "feat: add QuickSwitcherModel with note name/folder/recent tracking"
```

---

### Task 2: QuickSwitcherDelegate with Fuzzy Highlight

**Files:**
- Create: `src/dialogs/QuickSwitcherDelegate.h`
- Create: `src/dialogs/QuickSwitcherDelegate.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Implement QuickSwitcherDelegate**

`src/dialogs/QuickSwitcherDelegate.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStyledItemDelegate>

namespace Corbomite {

class QuickSwitcherDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit QuickSwitcherDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    void setFilterPattern(const QString &pattern);

private:
    QString m_pattern;
};

} // namespace Corbomite
```

`src/dialogs/QuickSwitcherDelegate.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "QuickSwitcherDelegate.h"
#include "QuickSwitcherModel.h"

#include <KFuzzyMatcher>
#include <QPainter>
#include <QApplication>

namespace Corbomite {

QuickSwitcherDelegate::QuickSwitcherDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void QuickSwitcherDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    painter->save();

    // Draw background (selection highlight)
    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    const QString name = index.data(QuickSwitcherModel::NoteNameRole).toString();
    const QString folder = index.data(QuickSwitcherModel::FolderPathRole).toString();

    const int padding = 6;
    QRect textRect = option.rect.adjusted(padding, 0, -padding, 0);

    // Draw note name with fuzzy match highlighting
    QFont nameFont = option.font;
    nameFont.setPointSize(nameFont.pointSize() + 1);
    QFontMetrics nameFm(nameFont);

    if (!m_pattern.isEmpty()) {
        // Get matched character ranges for highlighting
        auto ranges = KFuzzyMatcher::matchedRanges(m_pattern, name);

        int x = textRect.left();
        int y = textRect.center().y() + nameFm.ascent() / 2 - 1;

        for (int i = 0; i < name.length(); ++i) {
            bool highlighted = false;
            for (const auto &range : ranges) {
                if (i >= range.start && i < range.start + range.length) {
                    highlighted = true;
                    break;
                }
            }

            QFont charFont = nameFont;
            if (highlighted) {
                charFont.setBold(true);
                painter->setPen(option.palette.color(QPalette::Link));
            } else {
                painter->setPen(option.palette.color(QPalette::Text));
            }
            painter->setFont(charFont);
            QString ch = name.mid(i, 1);
            painter->drawText(x, y, ch);
            x += QFontMetrics(charFont).horizontalAdvance(ch);
        }
    } else {
        // No filter — draw name normally
        painter->setFont(nameFont);
        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, name);
    }

    // Draw folder path in muted color on the right
    if (!folder.isEmpty()) {
        QFont folderFont = option.font;
        folderFont.setPointSize(folderFont.pointSize() - 1);
        painter->setFont(folderFont);
        painter->setPen(option.palette.color(QPalette::PlaceholderText));
        painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, folder);
    }

    painter->restore();
}

QSize QuickSwitcherDelegate::sizeHint(const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const
{
    Q_UNUSED(index)
    return QSize(option.rect.width(), option.fontMetrics.height() + 16);
}

void QuickSwitcherDelegate::setFilterPattern(const QString &pattern)
{
    m_pattern = pattern;
}

} // namespace Corbomite
```

- [ ] **Step 2: Add to src/CMakeLists.txt**

Add `dialogs/QuickSwitcherDelegate.cpp` to CorbomiteApp source list.

- [ ] **Step 3: Build**

Run: `cmake --build build 2>&1 | tail -3`

Expected: Builds cleanly.

- [ ] **Step 4: Commit**

```bash
git add src/dialogs/QuickSwitcherDelegate.h src/dialogs/QuickSwitcherDelegate.cpp src/CMakeLists.txt
git commit -m "feat: add QuickSwitcherDelegate with fuzzy match highlighting

Renders note name with matched characters bold + accent color.
Shows folder path in muted text on the right side.
Uses KFuzzyMatcher::matchedRanges() for highlight positions."
```

---

### Task 3: QuickSwitcher Widget

**Files:**
- Create: `src/dialogs/QuickSwitcher.h`
- Create: `src/dialogs/QuickSwitcher.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Implement QuickSwitcher**

`src/dialogs/QuickSwitcher.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QLineEdit>
#include <QTreeView>
#include "QuickSwitcherModel.h"

class QSortFilterProxyModel;

namespace Corbomite {

class QuickSwitcherDelegate;
class VaultModel;

class QuickSwitcher : public QFrame {
    Q_OBJECT

public:
    explicit QuickSwitcher(VaultModel *vault, const QStringList &recentPaths,
                           QWidget *parent = nullptr);

Q_SIGNALS:
    void noteSelected(const QString &relativePath);
    void createNoteRequested(const QString &name);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void updateFilter(const QString &text);
    void activateSelected();
    void selectNext();
    void selectPrevious();

    QLineEdit *m_input;
    QTreeView *m_resultList;
    QuickSwitcherModel *m_sourceModel;
    QSortFilterProxyModel *m_proxyModel;
    QuickSwitcherDelegate *m_delegate;
    QString m_currentFilter;
    // Future: accept alias data for frontmatter alias matching
};

} // namespace Corbomite
```

`src/dialogs/QuickSwitcher.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "QuickSwitcher.h"
#include "QuickSwitcherDelegate.h"
#include "corbomite/models/VaultModel.h"

#include <KFuzzyMatcher>
#include <KLocalizedString>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QSortFilterProxyModel>
#include <QGraphicsDropShadowEffect>
#include <QApplication>

namespace Corbomite {

// Custom proxy that uses KFuzzyMatcher for filtering and scoring
class FuzzyFilterProxyModel : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setFilterPattern(const QString &pattern)
    {
        m_pattern = pattern;
        invalidateFilter();
        sort(0); // Re-sort by score
    }

    QString filterPattern() const { return m_pattern; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        Q_UNUSED(sourceParent)
        if (m_pattern.isEmpty()) return true;

        auto idx = sourceModel()->index(sourceRow, 0);
        QString name = idx.data(QuickSwitcherModel::NoteNameRole).toString();
        return KFuzzyMatcher::matchSimple(m_pattern, name);
    }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        if (m_pattern.isEmpty()) {
            // No filter: recent files first, then alphabetical
            bool leftRecent = left.data(QuickSwitcherModel::IsRecentRole).toBool();
            bool rightRecent = right.data(QuickSwitcherModel::IsRecentRole).toBool();
            if (leftRecent != rightRecent) return leftRecent;
            return left.data(QuickSwitcherModel::NoteNameRole).toString()
                       .compare(right.data(QuickSwitcherModel::NoteNameRole).toString(),
                                Qt::CaseInsensitive) < 0;
        }

        // With filter: sort by fuzzy score (higher = better = first)
        QString leftName = left.data(QuickSwitcherModel::NoteNameRole).toString();
        QString rightName = right.data(QuickSwitcherModel::NoteNameRole).toString();
        auto leftResult = KFuzzyMatcher::match(m_pattern, leftName);
        auto rightResult = KFuzzyMatcher::match(m_pattern, rightName);
        return leftResult.score > rightResult.score;
    }

private:
    QString m_pattern;
};

QuickSwitcher::QuickSwitcher(VaultModel *vault, const QStringList &recentPaths,
                               QWidget *parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedWidth(600);
    setMaximumHeight(400);
    setFrameShape(QFrame::StyledPanel);

    // Drop shadow
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 80));
    setGraphicsEffect(shadow);

    // Layout
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    // Search input
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(i18n("Open note..."));
    m_input->setClearButtonEnabled(true);
    QFont inputFont = m_input->font();
    inputFont.setPointSize(inputFont.pointSize() + 2);
    m_input->setFont(inputFont);
    layout->addWidget(m_input);

    // Results list
    m_resultList = new QTreeView(this);
    m_resultList->setHeaderHidden(true);
    m_resultList->setRootIsDecorated(false);
    m_resultList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultList->setFrameShape(QFrame::NoFrame);
    m_resultList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_resultList);

    // Source model
    m_sourceModel = new QuickSwitcherModel(this);
    m_sourceModel->setNotes(vault->allNotes());
    m_sourceModel->setRecentPaths(recentPaths);

    // Proxy model with fuzzy filtering
    m_proxyModel = new FuzzyFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_sourceModel);
    m_proxyModel->sort(0);

    m_resultList->setModel(m_proxyModel);

    // Delegate for match highlighting
    m_delegate = new QuickSwitcherDelegate(this);
    m_resultList->setItemDelegate(m_delegate);

    // Select first item
    if (m_proxyModel->rowCount() > 0) {
        m_resultList->setCurrentIndex(m_proxyModel->index(0, 0));
    }

    // Connect filter
    connect(m_input, &QLineEdit::textChanged, this, &QuickSwitcher::updateFilter);

    // Install event filter for keyboard navigation
    m_input->installEventFilter(this);
    m_resultList->installEventFilter(this);

    // Double-click to open
    connect(m_resultList, &QTreeView::doubleClicked, this, [this](const QModelIndex &) {
        activateSelected();
    });

    m_input->setFocus();
}

void QuickSwitcher::updateFilter(const QString &text)
{
    m_currentFilter = text;
    static_cast<FuzzyFilterProxyModel *>(m_proxyModel)->setFilterPattern(text);
    m_delegate->setFilterPattern(text);

    // Select first result
    if (m_proxyModel->rowCount() > 0) {
        m_resultList->setCurrentIndex(m_proxyModel->index(0, 0));
    }

    m_resultList->viewport()->update(); // Repaint with new highlights
}

void QuickSwitcher::activateSelected()
{
    auto current = m_resultList->currentIndex();
    if (current.isValid()) {
        QString path = current.data(QuickSwitcherModel::NotePathRole).toString();
        Q_EMIT noteSelected(path);
    } else if (!m_currentFilter.isEmpty()) {
        // No match — create new note with the typed name
        Q_EMIT createNoteRequested(m_currentFilter);
    }
    close();
}

void QuickSwitcher::selectNext()
{
    auto current = m_resultList->currentIndex();
    int nextRow = current.isValid() ? current.row() + 1 : 0;
    if (nextRow < m_proxyModel->rowCount()) {
        m_resultList->setCurrentIndex(m_proxyModel->index(nextRow, 0));
    }
}

void QuickSwitcher::selectPrevious()
{
    auto current = m_resultList->currentIndex();
    int prevRow = current.isValid() ? current.row() - 1 : 0;
    if (prevRow >= 0) {
        m_resultList->setCurrentIndex(m_proxyModel->index(prevRow, 0));
    }
}

bool QuickSwitcher::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Escape:
            close();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            activateSelected();
            return true;
        case Qt::Key_Down:
            selectNext();
            return true;
        case Qt::Key_Up:
            selectPrevious();
            return true;
        default:
            // If typing in the result list, redirect to input
            if (obj == m_resultList && keyEvent->text().length() > 0
                && !keyEvent->text().at(0).isSpace()) {
                m_input->setFocus();
                QApplication::sendEvent(m_input, event);
                return true;
            }
            break;
        }
    }
    return QFrame::eventFilter(obj, event);
}

} // namespace Corbomite
```

- [ ] **Step 2: Add to src/CMakeLists.txt**

Add `dialogs/QuickSwitcher.cpp` to CorbomiteApp source list.

- [ ] **Step 3: Build**

Run: `cmake --build build 2>&1 | tail -3`

Expected: Builds cleanly.

- [ ] **Step 4: Commit**

```bash
git add src/dialogs/QuickSwitcher.h src/dialogs/QuickSwitcher.cpp src/CMakeLists.txt
git commit -m "feat: add QuickSwitcher popup with fuzzy matching and keyboard navigation

Obsidian-style note finder: frameless popup, fuzzy filter via
KFuzzyMatcher, recent notes first when empty, create-on-no-match,
arrow key navigation, Escape to dismiss."
```

---

### Task 4: Command Palette + Quick Switcher MainWindow Integration

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `src/app/corbomiteui.rc.in`

- [ ] **Step 1: Add new methods and actions to MainWindow**

Update `src/app/MainWindow.h` — add two new private methods:

```cpp
    // In private section, after saveCurrentNote():
    void showQuickSwitcher();
    void showCommandPalette();
```

- [ ] **Step 2: Implement showCommandPalette and showQuickSwitcher in MainWindow.cpp**

Add includes at top of `MainWindow.cpp`:
```cpp
#include <KCommandBar>
#include "dialogs/QuickSwitcher.h"
#include "editor/EditorViewSpace.h"
```

Add `showCommandPalette()` implementation:
```cpp
void MainWindow::showCommandPalette()
{
    auto *bar = new KCommandBar(this);

    QList<KCommandBar::ActionGroup> groups;

    // Collect actions from our KActionCollection, grouped by prefix
    KActionCollection *ac = actionCollection();
    QList<QAction *> fileActions, viewActions, editActions;

    for (QAction *action : ac->actions()) {
        QString name = action->objectName();
        if (name.startsWith(QStringLiteral("file_"))) {
            fileActions.append(action);
        } else if (name.startsWith(QStringLiteral("view_"))) {
            viewActions.append(action);
        } else {
            editActions.append(action);
        }
    }

    if (!fileActions.isEmpty())
        groups.append({i18n("File"), fileActions});
    if (!viewActions.isEmpty())
        groups.append({i18n("View"), viewActions});
    if (!editActions.isEmpty())
        groups.append({i18n("Other"), editActions});

    bar->setActions(groups);
    bar->show();
}
```

Add `showQuickSwitcher()` implementation:
```cpp
void MainWindow::showQuickSwitcher()
{
    if (!m_vaultService->isOpen()) return;

    QStringList recent;
    auto *viewSpace = m_editorManager->activeViewSpace();
    if (viewSpace) {
        recent = viewSpace->tabModel()->lruSortedPaths();
    }

    auto *switcher = new QuickSwitcher(m_vaultService->vault(), recent, this);

    // Position at top-center of window
    QPoint topCenter = mapToGlobal(QPoint(width() / 2 - 300, 80));
    switcher->move(topCenter);

    connect(switcher, &QuickSwitcher::noteSelected,
            this, &MainWindow::onNoteActivated);
    connect(switcher, &QuickSwitcher::createNoteRequested,
            this, [this](const QString &name) {
        auto *doc = m_vaultService->noteService()->createNote(name, QString());
        if (doc) m_editorManager->openNote(doc);
    });

    switcher->show();
}
```

- [ ] **Step 3: Register the new actions in setupActions()**

Add to `MainWindow::setupActions()`, after the existing view actions:

```cpp
    // Navigation actions
    auto *quickSwitcher = ac->addAction(QStringLiteral("quick_switcher"));
    quickSwitcher->setText(i18n("Quick Switcher"));
    quickSwitcher->setIcon(QIcon::fromTheme(QStringLiteral("quickopen")));
    ac->setDefaultShortcut(quickSwitcher, QKeySequence(Qt::CTRL | Qt::Key_O));
    connect(quickSwitcher, &QAction::triggered, this, &MainWindow::showQuickSwitcher);

    auto *commandPalette = ac->addAction(QStringLiteral("command_palette"));
    commandPalette->setText(i18n("Command Palette"));
    commandPalette->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
    ac->setDefaultShortcut(commandPalette, QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(commandPalette, &QAction::triggered, this, &MainWindow::showCommandPalette);
```

- [ ] **Step 4: Update XMLGUI**

Replace `src/app/corbomiteui.rc.in` with (increment version to 2 to force refresh):

```xml
<!DOCTYPE gui SYSTEM "kpartgui.dtd">
<gui name="@CORBOMITE_COMPONENT_NAME@" version="2">
  <MenuBar>
    <Menu name="file">
      <text>&amp;File</text>
      <Action name="file_open_vault"/>
      <Separator/>
      <Action name="file_new_note"/>
      <Separator/>
      <Action name="file_save"/>
      <Separator/>
    </Menu>
    <Menu name="go">
      <text>&amp;Go</text>
      <Action name="quick_switcher"/>
      <Action name="command_palette"/>
    </Menu>
    <Menu name="view">
      <text>&amp;View</text>
      <Action name="view_toggle_left_sidebar"/>
      <Separator/>
      <Action name="view_zoom_in"/>
      <Action name="view_zoom_out"/>
      <Action name="view_zoom_reset"/>
    </Menu>
  </MenuBar>
  <ToolBar name="mainToolBar" noMerge="1">
    <text>Main Toolbar</text>
    <Action name="file_open_vault"/>
    <Action name="file_new_note"/>
    <Action name="file_save"/>
    <Separator/>
    <Action name="quick_switcher"/>
    <Action name="command_palette"/>
  </ToolBar>
</gui>
```

- [ ] **Step 5: Build and verify**

Run:
```bash
cmake --build build && cd build && ctest --output-on-failure
```

Expected: All tests pass (13 including new quickswitchermodel test). Build clean.

- [ ] **Step 6: Manual testing**

Run `./build/bin/Corbomite`:
1. Open a vault (File → Open Vault, pick `testvaults/starter-vault/PKM LM/`)
2. Press **Ctrl+O** — Quick Switcher should appear centered at top with "Open note..." placeholder
3. Type part of a note name — results should fuzzy-filter with highlighted matches
4. Arrow keys navigate, Enter opens the note in a tab
5. Press **Ctrl+P** — Command Palette should appear showing all registered actions with shortcuts
6. Type to fuzzy-filter commands, Enter executes
7. Verify the Go menu appears in the menu bar with both entries
8. Verify toolbar has the new buttons

- [ ] **Step 7: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp src/app/corbomiteui.rc.in
git commit -m "feat: integrate Quick Switcher (Ctrl+O) and Command Palette (Ctrl+P)

Quick Switcher: fuzzy note finder with recent files, create-on-no-match.
Command Palette: KCommandBar with grouped actions and shortcut display.
Added Go menu with both actions. Added toolbar buttons.
XMLGUI version bumped to 2."
```

---

Self-review:

1. **Spec coverage:** Command Palette via KCommandBar ✓. Quick Switcher with fuzzy matching ✓. Recent files priority ✓. Create on no match ✓. Keyboard navigation ✓. Match highlighting ✓. XMLGUI Go menu ✓. Shortcuts Ctrl+O and Ctrl+P ✓. Breadcrumb comments for future alias matching ✓.

2. **Placeholder scan:** No TBDs. All code is complete. Future expansion points marked with `// Future:` comments.

3. **Type consistency:** `QuickSwitcherModel::Roles` (NotePathRole, NoteNameRole, FolderPathRole, IsRecentRole) used consistently in model, delegate, proxy, and widget. `KFuzzyMatcher::matchSimple` for filtering, `KFuzzyMatcher::match` for scoring, `KFuzzyMatcher::matchedRanges` for highlighting — all from `<KFuzzyMatcher>` header.
