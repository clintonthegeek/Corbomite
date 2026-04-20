# Sidebar Panels (Backlinks, Outlinks, Outline) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Backlinks, Outlinks, and Outline panels to the right sidebar that update when the active note changes.

**Architecture:** Three QWidget-based panels registered as CorbomiteMDI ToolViews in the right sidebar. BacklinksPanel and OutlinksPanel query SQLiteIndex for link data. OutlinePanel extracts headings from note content via regex. All panels update via `activeEditorChanged` signal.

**Tech Stack:** C++20, Qt6 Widgets (QListWidget, QTreeWidget), SQLiteIndex link queries

**Spec:** `docs/superpowers/specs/2026-03-31-sidebar-panels-design.md`

**Current state:** 17 tests passing. Link index with backlinksFor/outlinksFor implemented.

---

### Task 1: BacklinksPanel

**Files:**
- Create: `src/sidebar/BacklinksPanel.h`
- Create: `src/sidebar/BacklinksPanel.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Implement BacklinksPanel**

`src/sidebar/BacklinksPanel.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLabel>

namespace Corbomite {

class NoteDocument;
class SQLiteIndex;

class BacklinksPanel : public QWidget {
    Q_OBJECT

public:
    explicit BacklinksPanel(QWidget *parent = nullptr);

    void setIndex(SQLiteIndex *index);
    void setCurrentNote(NoteDocument *doc);

Q_SIGNALS:
    void noteActivated(const QString &relativePath);

private:
    void refresh();
    void onItemClicked(QListWidgetItem *item);

    QLabel *m_headerLabel;
    QListWidget *m_list;
    QLabel *m_emptyLabel;

    SQLiteIndex *m_index = nullptr;
    NoteDocument *m_currentDoc = nullptr;
};

} // namespace Corbomite
```

`src/sidebar/BacklinksPanel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "BacklinksPanel.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/SQLiteIndex.h"

#include <KLocalizedString>
#include <QVBoxLayout>
#include <QFont>

namespace Corbomite {

BacklinksPanel::BacklinksPanel(QWidget *parent)
    : QWidget(parent)
    , m_headerLabel(new QLabel(i18n("Backlinks"), this))
    , m_list(new QListWidget(this))
    , m_emptyLabel(new QLabel(i18n("No backlinks"), this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    QFont headerFont = m_headerLabel->font();
    headerFont.setBold(true);
    m_headerLabel->setFont(headerFont);
    layout->addWidget(m_headerLabel);

    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_list);

    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(m_emptyLabel);
    m_emptyLabel->setVisible(true);
    m_list->setVisible(false);

    connect(m_list, &QListWidget::itemClicked, this, &BacklinksPanel::onItemClicked);
}

void BacklinksPanel::setIndex(SQLiteIndex *index)
{
    m_index = index;
    refresh();
}

void BacklinksPanel::setCurrentNote(NoteDocument *doc)
{
    m_currentDoc = doc;
    refresh();
}

void BacklinksPanel::refresh()
{
    m_list->clear();

    if (!m_index || !m_currentDoc) {
        m_headerLabel->setText(i18n("Backlinks"));
        m_emptyLabel->setVisible(true);
        m_list->setVisible(false);
        return;
    }

    auto backlinks = m_index->backlinksFor(m_currentDoc->relativePath());

    m_headerLabel->setText(i18n("Backlinks (%1)", backlinks.size()));

    if (backlinks.isEmpty()) {
        m_emptyLabel->setVisible(true);
        m_list->setVisible(false);
        return;
    }

    m_emptyLabel->setVisible(false);
    m_list->setVisible(true);

    for (const auto &link : backlinks) {
        // Extract note name from path
        QString name = link.sourcePath;
        name = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);

        // Extract folder for disambiguation
        QString folder;
        int lastSlash = link.sourcePath.lastIndexOf(QLatin1Char('/'));
        if (lastSlash > 0) {
            folder = link.sourcePath.left(lastSlash);
        }

        auto *item = new QListWidgetItem(m_list);
        if (folder.isEmpty()) {
            item->setText(name);
        } else {
            item->setText(name + QStringLiteral("  — ") + folder);
        }
        item->setData(Qt::UserRole, link.sourcePath);
        item->setToolTip(link.sourcePath);

        // Future: Add context snippets showing the paragraph containing the link.
        // Options to explore for caching:
        // - Cache context at index time (adds ~100 chars per link to DB, fast retrieval)
        // - Lazy-load context on panel expand (slower but no storage cost)
        // - Store line numbers in links table, read only the relevant line from disk
    }
}

void BacklinksPanel::onItemClicked(QListWidgetItem *item)
{
    QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        Q_EMIT noteActivated(path);
    }
}

} // namespace Corbomite
```

- [ ] **Step 2: Add to src/CMakeLists.txt**

Add `sidebar/BacklinksPanel.cpp` to CorbomiteApp sources.

- [ ] **Step 3: Build**

Run: `cmake --build build`

- [ ] **Step 4: Commit**

```bash
git add src/sidebar/BacklinksPanel.h src/sidebar/BacklinksPanel.cpp src/CMakeLists.txt
git commit -m "feat: add BacklinksPanel showing notes linking to current note"
```

---

### Task 2: OutlinksPanel

**Files:**
- Create: `src/sidebar/OutlinksPanel.h`
- Create: `src/sidebar/OutlinksPanel.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Implement OutlinksPanel**

`src/sidebar/OutlinksPanel.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLabel>

namespace Corbomite {

class NoteDocument;
class SQLiteIndex;
class VaultModel;

class OutlinksPanel : public QWidget {
    Q_OBJECT

public:
    explicit OutlinksPanel(QWidget *parent = nullptr);

    void setIndex(SQLiteIndex *index);
    void setVaultModel(VaultModel *vault);
    void setCurrentNote(NoteDocument *doc);

Q_SIGNALS:
    void noteActivated(const QString &relativePath);
    void createNoteRequested(const QString &name);

private:
    void refresh();
    void onItemClicked(QListWidgetItem *item);

    QLabel *m_headerLabel;
    QListWidget *m_list;
    QLabel *m_emptyLabel;

    SQLiteIndex *m_index = nullptr;
    VaultModel *m_vault = nullptr;
    NoteDocument *m_currentDoc = nullptr;
};

} // namespace Corbomite
```

`src/sidebar/OutlinksPanel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OutlinksPanel.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/models/VaultModel.h"

#include <KLocalizedString>
#include <QVBoxLayout>
#include <QFont>

namespace Corbomite {

OutlinksPanel::OutlinksPanel(QWidget *parent)
    : QWidget(parent)
    , m_headerLabel(new QLabel(i18n("Outgoing Links"), this))
    , m_list(new QListWidget(this))
    , m_emptyLabel(new QLabel(i18n("No outgoing links"), this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    QFont headerFont = m_headerLabel->font();
    headerFont.setBold(true);
    m_headerLabel->setFont(headerFont);
    layout->addWidget(m_headerLabel);

    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_list);

    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(m_emptyLabel);
    m_emptyLabel->setVisible(true);
    m_list->setVisible(false);

    connect(m_list, &QListWidget::itemClicked, this, &OutlinksPanel::onItemClicked);
}

void OutlinksPanel::setIndex(SQLiteIndex *index)
{
    m_index = index;
    refresh();
}

void OutlinksPanel::setVaultModel(VaultModel *vault)
{
    m_vault = vault;
}

void OutlinksPanel::setCurrentNote(NoteDocument *doc)
{
    m_currentDoc = doc;
    refresh();
}

void OutlinksPanel::refresh()
{
    m_list->clear();

    if (!m_index || !m_currentDoc) {
        m_headerLabel->setText(i18n("Outgoing Links"));
        m_emptyLabel->setVisible(true);
        m_list->setVisible(false);
        return;
    }

    auto outlinks = m_index->outlinksFor(m_currentDoc->relativePath());

    m_headerLabel->setText(i18n("Outgoing Links (%1)", outlinks.size()));

    if (outlinks.isEmpty()) {
        m_emptyLabel->setVisible(true);
        m_list->setVisible(false);
        return;
    }

    m_emptyLabel->setVisible(false);
    m_list->setVisible(true);

    for (const auto &link : outlinks) {
        // Extract note name from target path
        QString name = link.targetPath;
        name = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);

        auto *item = new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, link.targetPath);
        item->setToolTip(link.targetPath);

        // Check if target exists
        bool exists = m_vault && m_vault->noteExists(link.targetPath);

        if (exists) {
            item->setText(name);
        } else {
            // Orphan link — show in muted italic
            item->setText(name + i18n(" (create)"));
            QFont font = item->font();
            font.setItalic(true);
            item->setFont(font);
            item->setForeground(QColor(128, 128, 128));
        }

        // Show link type as icon
        if (link.linkType == QStringLiteral("embed")) {
            item->setIcon(QIcon::fromTheme(QStringLiteral("insert-image")));
        } else if (link.linkType == QStringLiteral("markdown")) {
            item->setIcon(QIcon::fromTheme(QStringLiteral("text-html")));
        }
        // wiki links get no special icon (default)
    }
}

void OutlinksPanel::onItemClicked(QListWidgetItem *item)
{
    QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;

    bool exists = m_vault && m_vault->noteExists(path);
    if (exists) {
        Q_EMIT noteActivated(path);
    } else {
        // Extract name for creation (strip .md)
        QString name = path;
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
        Q_EMIT createNoteRequested(name);
    }
}

} // namespace Corbomite
```

- [ ] **Step 2: Add to src/CMakeLists.txt**

Add `sidebar/OutlinksPanel.cpp` to CorbomiteApp sources.

- [ ] **Step 3: Build**

Run: `cmake --build build`

- [ ] **Step 4: Commit**

```bash
git add src/sidebar/OutlinksPanel.h src/sidebar/OutlinksPanel.cpp src/CMakeLists.txt
git commit -m "feat: add OutlinksPanel showing links from current note

Existing targets shown normally, orphan links in muted italic with
(create) suffix. Click orphan to create the note."
```

---

### Task 3: OutlinePanel

**Files:**
- Create: `src/sidebar/OutlinePanel.h`
- Create: `src/sidebar/OutlinePanel.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Implement OutlinePanel**

`src/sidebar/OutlinePanel.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QTimer>

namespace Corbomite {

class NoteDocument;

class OutlinePanel : public QWidget {
    Q_OBJECT

public:
    explicit OutlinePanel(QWidget *parent = nullptr);

    void setCurrentNote(NoteDocument *doc);

Q_SIGNALS:
    void scrollToLine(int lineNumber);

private:
    void refresh();
    void onItemClicked(QTreeWidgetItem *item, int column);

    QLabel *m_headerLabel;
    QTreeWidget *m_tree;
    QLabel *m_emptyLabel;
    QTimer m_debounceTimer;

    NoteDocument *m_currentDoc = nullptr;
};

} // namespace Corbomite
```

`src/sidebar/OutlinePanel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OutlinePanel.h"
#include "corbomite/core/NoteDocument.h"

#include <KLocalizedString>
#include <QVBoxLayout>
#include <QFont>
#include <QRegularExpression>

namespace Corbomite {

OutlinePanel::OutlinePanel(QWidget *parent)
    : QWidget(parent)
    , m_headerLabel(new QLabel(i18n("Outline"), this))
    , m_tree(new QTreeWidget(this))
    , m_emptyLabel(new QLabel(i18n("No headings"), this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    QFont headerFont = m_headerLabel->font();
    headerFont.setBold(true);
    m_headerLabel->setFont(headerFont);
    layout->addWidget(m_headerLabel);

    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_tree);

    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(m_emptyLabel);
    m_emptyLabel->setVisible(true);
    m_tree->setVisible(false);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(500);
    connect(&m_debounceTimer, &QTimer::timeout, this, &OutlinePanel::refresh);

    connect(m_tree, &QTreeWidget::itemClicked, this, &OutlinePanel::onItemClicked);
}

void OutlinePanel::setCurrentNote(NoteDocument *doc)
{
    // Disconnect from previous document
    if (m_currentDoc) {
        disconnect(m_currentDoc, &NoteDocument::textChanged, &m_debounceTimer, nullptr);
    }

    m_currentDoc = doc;

    if (m_currentDoc) {
        // Connect for live updates
        connect(m_currentDoc, &NoteDocument::textChanged,
                &m_debounceTimer, qOverload<>(&QTimer::start));
    }

    refresh();
}

void OutlinePanel::refresh()
{
    m_tree->clear();

    if (!m_currentDoc || m_currentDoc->markdown().isEmpty()) {
        m_headerLabel->setText(i18n("Outline"));
        m_emptyLabel->setVisible(true);
        m_tree->setVisible(false);
        return;
    }

    static const QRegularExpression headingPattern(QStringLiteral(R"(^(#{1,6})\s+(.+)$)"));

    const auto lines = m_currentDoc->markdown().split(QLatin1Char('\n'));

    // Stack to track parent items at each heading level
    // Index 0 = H1 parent, Index 1 = H2 parent, etc.
    QTreeWidgetItem *parents[6] = {nullptr};
    int headingCount = 0;

    for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
        auto match = headingPattern.match(lines[lineNum]);
        if (!match.hasMatch()) continue;

        int level = match.captured(1).length(); // 1-6
        QString text = match.captured(2).trimmed();

        auto *item = new QTreeWidgetItem();
        item->setText(0, text);
        item->setData(0, Qt::UserRole, lineNum + 1); // 1-based line number

        // Find parent: the most recent heading with a lower level
        QTreeWidgetItem *parent = nullptr;
        for (int i = level - 2; i >= 0; --i) {
            if (parents[i]) {
                parent = parents[i];
                break;
            }
        }

        if (parent) {
            parent->addChild(item);
        } else {
            m_tree->addTopLevelItem(item);
        }

        // Register this item as the parent for deeper levels
        parents[level - 1] = item;
        // Clear deeper level parents (a new H2 resets H3-H6 parents)
        for (int i = level; i < 6; ++i) {
            parents[i] = nullptr;
        }

        ++headingCount;
    }

    m_headerLabel->setText(i18n("Outline (%1)", headingCount));

    if (headingCount == 0) {
        m_emptyLabel->setVisible(true);
        m_tree->setVisible(false);
    } else {
        m_emptyLabel->setVisible(false);
        m_tree->setVisible(true);
        m_tree->expandAll();
    }
}

void OutlinePanel::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    int lineNumber = item->data(0, Qt::UserRole).toInt();
    if (lineNumber > 0) {
        Q_EMIT scrollToLine(lineNumber);
    }
}

} // namespace Corbomite
```

- [ ] **Step 2: Add to src/CMakeLists.txt**

Add `sidebar/OutlinePanel.cpp` to CorbomiteApp sources.

- [ ] **Step 3: Build**

Run: `cmake --build build`

- [ ] **Step 4: Commit**

```bash
git add src/sidebar/OutlinePanel.h src/sidebar/OutlinePanel.cpp src/CMakeLists.txt
git commit -m "feat: add OutlinePanel showing heading structure of current note

H1-H6 heading tree with proper nesting. Click heading to scroll
editor. Updates on text change with 500ms debounce."
```

---

### Task 4: MainWindow Wiring

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Add panel members to MainWindow.h**

Add forward declarations after existing ones:
```cpp
class BacklinksPanel;
class OutlinksPanel;
class OutlinePanel;
```

Add private members after `m_searchIndex`:
```cpp
    BacklinksPanel *m_backlinksPanel = nullptr;
    OutlinksPanel *m_outlinksPanel = nullptr;
    OutlinePanel *m_outlinePanel = nullptr;
```

- [ ] **Step 2: Add includes to MainWindow.cpp**

```cpp
#include "sidebar/BacklinksPanel.h"
#include "sidebar/OutlinksPanel.h"
#include "sidebar/OutlinePanel.h"
```

- [ ] **Step 3: Create panels in setupSidebars()**

Add after the existing search panel setup (at the end of `setupSidebars()`):

```cpp
    // Right sidebar: Backlinks
    auto *backlinksView = createToolView(
        nullptr,
        QStringLiteral("backlinks_panel"),
        KMultiTabBar::Right,
        QIcon::fromTheme(QStringLiteral("link")),
        i18n("Backlinks")
    );
    m_backlinksPanel = new BacklinksPanel(backlinksView);
    backlinksView->layout()->addWidget(m_backlinksPanel);
    connect(m_backlinksPanel, &BacklinksPanel::noteActivated,
            this, &MainWindow::onNoteActivated);

    // Right sidebar: Outlinks
    auto *outlinksView = createToolView(
        nullptr,
        QStringLiteral("outlinks_panel"),
        KMultiTabBar::Right,
        QIcon::fromTheme(QStringLiteral("go-jump")),
        i18n("Outlinks")
    );
    m_outlinksPanel = new OutlinksPanel(outlinksView);
    outlinksView->layout()->addWidget(m_outlinksPanel);
    connect(m_outlinksPanel, &OutlinksPanel::noteActivated,
            this, &MainWindow::onNoteActivated);
    connect(m_outlinksPanel, &OutlinksPanel::createNoteRequested,
            this, [this](const QString &name) {
        auto *doc = m_vaultService->noteService()->createNote(name, QString());
        if (doc) m_editorManager->openNote(doc);
    });

    // Right sidebar: Outline
    auto *outlineView = createToolView(
        nullptr,
        QStringLiteral("outline_panel"),
        KMultiTabBar::Right,
        QIcon::fromTheme(QStringLiteral("view-list-tree")),
        i18n("Outline")
    );
    m_outlinePanel = new OutlinePanel(outlineView);
    outlineView->layout()->addWidget(m_outlinePanel);
    connect(m_outlinePanel, &OutlinePanel::scrollToLine,
            this, [this](int lineNumber) {
        auto *editor = m_editorManager->activeEditor();
        if (!editor) return;
        QTextCursor cursor(editor->document()->findBlockByLineNumber(lineNumber - 1));
        editor->setTextCursor(cursor);
        editor->centerCursor();
    });
```

- [ ] **Step 4: Wire active note updates in onVaultOpened()**

In `onVaultOpened()`, after the existing `activeEditorChanged` connection, add the panel index setup and update connection:

```cpp
    // Set index on sidebar panels
    m_backlinksPanel->setIndex(m_searchIndex);
    m_outlinksPanel->setIndex(m_searchIndex);
    m_outlinksPanel->setVaultModel(vault);

    // Update sidebar panels when active note changes
    connect(m_editorManager, &EditorViewManager::activeEditorChanged,
            this, [this](NoteEditorWidget *editor) {
        if (editor && editor->noteDocument()) {
            m_backlinksPanel->setCurrentNote(editor->noteDocument());
            m_outlinksPanel->setCurrentNote(editor->noteDocument());
            m_outlinePanel->setCurrentNote(editor->noteDocument());
        } else {
            m_backlinksPanel->setCurrentNote(nullptr);
            m_outlinksPanel->setCurrentNote(nullptr);
            m_outlinePanel->setCurrentNote(nullptr);
        }
    });
```

- [ ] **Step 5: Clear panels in onVaultClosed()**

Add before the existing search index cleanup:

```cpp
    m_backlinksPanel->setIndex(nullptr);
    m_backlinksPanel->setCurrentNote(nullptr);
    m_outlinksPanel->setIndex(nullptr);
    m_outlinksPanel->setVaultModel(nullptr);
    m_outlinksPanel->setCurrentNote(nullptr);
    m_outlinePanel->setCurrentNote(nullptr);
```

- [ ] **Step 6: Add QTextCursor include**

Add to MainWindow.cpp includes:
```cpp
#include <QTextCursor>
```

- [ ] **Step 7: Build and verify**

Run:
```bash
cmake --build build && cd /home/clinton/dev/Corbomite/build && ctest --output-on-failure
```

Expected: All 17 tests pass, builds clean.

- [ ] **Step 8: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat: wire Backlinks, Outlinks, and Outline panels into right sidebar

All three panels update when active note changes. Backlinks/Outlinks
query SQLiteIndex. Outline shows heading tree from note content.
Click backlink/outlink navigates to that note. Click outline heading
scrolls editor to that line."
```

---

Self-review:

1. **Spec coverage:** BacklinksPanel with count + clickable items ✓. OutlinksPanel with orphan detection + create-on-click ✓. OutlinePanel with H1-H6 tree + click-to-scroll ✓. Right sidebar ToolViews ✓. activeEditorChanged updates ✓. Empty states ✓. Breadcrumb comments for context snippets ✓. Link type indicators ✓.

2. **Placeholder scan:** All code complete. No TBDs. Future items in breadcrumb comments only.

3. **Type consistency:** `BacklinksPanel::setCurrentNote(NoteDocument*)` matches `OutlinksPanel::setCurrentNote(NoteDocument*)` matches `OutlinePanel::setCurrentNote(NoteDocument*)`. `noteActivated(QString)` signal matches `MainWindow::onNoteActivated(QString)`. `SQLiteIndex::backlinksFor/outlinksFor` return `QVector<LinkInfo>` with `sourcePath`/`targetPath` fields used correctly.
