# Vault Session Management Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the vault-switching crash, add per-vault session persistence (tabs, splits, cursor positions, sidebar state), and replace the empty startup window with a welcome screen.

**Architecture:** EditorViewManager gains closeAllDocuments()/queryClose() for clean vault shutdown. SessionManager extended with split layout tree, per-tab cursor/mode state. New WelcomeScreen widget replaces editor area when no vault is open. MainWindow restructured with QStackedWidget to swap between welcome and editor views. Close Vault action added.

**Tech Stack:** C++20, Qt6 (QStackedWidget, QPainter, QSplitter), KDE Frameworks 6 (KXmlGui, KStandardAction, KMessageBox, KRecentFilesAction)

**Spec:** `docs/superpowers/specs/2026-04-01-vault-session-management-design.md`

---

### Task 1: EditorViewManager::closeAllDocuments() and queryClose()

**Files:**
- Modify: `src/editor/EditorViewSpace.h`
- Modify: `src/editor/EditorViewSpace.cpp`
- Modify: `src/editor/EditorViewManager.h`
- Modify: `src/editor/EditorViewManager.cpp`

Add methods to cleanly shut down all editor state. EditorViewSpace gets `closeAllTabs()` and `hasModifiedDocuments()`. EditorViewManager gets `queryClose()` (prompt for unsaved) and `closeAllDocuments()` (close everything, reset to single pane).

- [ ] **Step 1: Add closeAllTabs() and hasModifiedDocuments() to EditorViewSpace.h**

Add two new public methods after `hasGraphView()`:

```cpp
    void closeAllTabs();
    bool hasModifiedDocuments() const;
    QStringList modifiedDocumentPaths() const;
```

- [ ] **Step 2: Implement closeAllTabs() in EditorViewSpace.cpp**

Add at the end, before the closing namespace brace:

```cpp
void EditorViewSpace::closeAllTabs()
{
    // Close from the end to avoid index shifting issues
    while (m_tabBar->count() > 0) {
        closeTab(0);
    }
}

bool EditorViewSpace::hasModifiedDocuments() const
{
    for (auto it = m_editors.constBegin(); it != m_editors.constEnd(); ++it) {
        auto *editor = it.value();
        if (editor && editor->noteDocument() && editor->noteDocument()->isModified()) {
            return true;
        }
    }
    return false;
}

QStringList EditorViewSpace::modifiedDocumentPaths() const
{
    QStringList paths;
    for (auto it = m_editors.constBegin(); it != m_editors.constEnd(); ++it) {
        auto *editor = it.value();
        if (editor && editor->noteDocument() && editor->noteDocument()->isModified()) {
            paths.append(it.key());
        }
    }
    return paths;
}
```

- [ ] **Step 3: Add queryClose() and closeAllDocuments() to EditorViewManager.h**

Add two new public methods after `int viewSpaceCount() const;`:

```cpp
    bool queryClose();
    void closeAllDocuments();
```

Add a new private helper after `void cleanupEmptySplitters(QSplitter *splitter);`:

```cpp
    void resetToSingleViewSpace();
```

- [ ] **Step 4: Implement queryClose() in EditorViewManager.cpp**

Add include at the top of the file:

```cpp
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardGuiItem>
```

Add the implementation before the closing namespace brace:

```cpp
bool EditorViewManager::queryClose()
{
    // Collect all modified documents across all panes
    QStringList modifiedPaths;
    for (auto *space : std::as_const(m_viewSpaces)) {
        modifiedPaths.append(space->modifiedDocumentPaths());
    }
    modifiedPaths.removeDuplicates();

    if (modifiedPaths.isEmpty()) {
        return true;
    }

    // Build a human-readable list of unsaved document names
    QStringList names;
    for (const QString &path : std::as_const(modifiedPaths)) {
        QString name = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
        names.append(name);
    }

    QString message;
    if (names.size() == 1) {
        message = i18n("The document \"%1\" has unsaved changes.\n\nDo you want to save before closing?", names.first());
    } else {
        message = i18n("The following documents have unsaved changes:\n\n%1\n\nDo you want to save before closing?",
                        names.join(QStringLiteral("\n")));
    }

    auto result = KMessageBox::warningTwoActionsCancel(
        this,
        message,
        i18n("Unsaved Changes"),
        KStandardGuiItem::save(),
        KStandardGuiItem::discard()
    );

    if (result == KMessageBox::Cancel) {
        return false;
    }

    if (result == KMessageBox::PrimaryAction) {
        // Save all modified documents
        for (auto *space : std::as_const(m_viewSpaces)) {
            for (auto it = space->m_editors.constBegin(); it != space->m_editors.constEnd(); ++it) {
                auto *editor = it.value();
                if (editor && editor->noteDocument() && editor->noteDocument()->isModified()) {
                    editor->noteDocument()->save();
                }
            }
        }
    }

    return true;
}
```

**Note:** The `space->m_editors` access requires making `EditorViewManager` a friend of `EditorViewSpace`. However, a cleaner approach is to add a public `saveAllModified()` method to EditorViewSpace. Let's do that instead.

- [ ] **Step 5: Add saveAllModified() to EditorViewSpace**

In `EditorViewSpace.h`, add after `modifiedDocumentPaths()`:

```cpp
    void saveAllModified();
```

In `EditorViewSpace.cpp`, add at the end:

```cpp
void EditorViewSpace::saveAllModified()
{
    for (auto it = m_editors.constBegin(); it != m_editors.constEnd(); ++it) {
        auto *editor = it.value();
        if (editor && editor->noteDocument() && editor->noteDocument()->isModified()) {
            editor->noteDocument()->save();
        }
    }
}
```

Now revise the save branch in `queryClose()` to use the public API:

```cpp
    if (result == KMessageBox::PrimaryAction) {
        // Save all modified documents
        for (auto *space : std::as_const(m_viewSpaces)) {
            space->saveAllModified();
        }
    }

    return true;
```

- [ ] **Step 6: Implement closeAllDocuments() and resetToSingleViewSpace()**

Add in EditorViewManager.cpp before the closing namespace brace:

```cpp
void EditorViewManager::closeAllDocuments()
{
    // Close all tabs in all panes
    for (auto *space : std::as_const(m_viewSpaces)) {
        space->closeAllTabs();
    }

    // Reset to a single view space
    resetToSingleViewSpace();
}

void EditorViewManager::resetToSingleViewSpace()
{
    if (m_viewSpaces.size() <= 1) return;

    // Keep the first view space, remove all others
    auto *keepSpace = m_viewSpaces.first();

    // Remove all other view spaces
    for (int i = m_viewSpaces.size() - 1; i >= 1; --i) {
        auto *space = m_viewSpaces.at(i);
        m_viewSpaces.removeAt(i);
        space->deleteLater();
    }

    // Remove all intermediate splitters — reparent keepSpace directly under root
    // First, collect all child splitters of root
    QList<QSplitter *> childSplitters;
    for (int i = m_rootSplitter->count() - 1; i >= 0; --i) {
        auto *child = qobject_cast<QSplitter *>(m_rootSplitter->widget(i));
        if (child) {
            childSplitters.append(child);
        }
    }

    // Reparent keep space to root splitter
    m_rootSplitter->addWidget(keepSpace);

    // Delete intermediate splitters (they're now empty since keepSpace was reparented)
    for (auto *s : std::as_const(childSplitters)) {
        s->deleteLater();
    }

    setActiveViewSpace(keepSpace);
    Q_EMIT activeEditorChanged(keepSpace->activeEditor());
}
```

- [ ] **Step 7: Build and verify**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -20
```

Commit: `feat: add EditorViewManager::queryClose() and closeAllDocuments() for clean vault shutdown`

---

### Task 2: Extend SessionManager for Split Layout and Per-Tab State

**Files:**
- Modify: `src/app/SessionManager.h`
- Modify: `src/app/SessionManager.cpp`

Extend the session schema to store the split layout tree, per-tab cursor/mode/scroll, and tab types. The new schema nests pane data inside an `"editor"` object with a `"splitLayout"` tree and `"panes"` array.

- [ ] **Step 1: Add new methods to SessionManager.h**

Replace the current save/load methods section with:

```cpp
    // Save
    void saveWindowGeometry(const QByteArray &geometry, const QByteArray &state);
    void saveSidebarState(bool leftVisible, int leftWidth, bool rightVisible, int rightWidth,
                          const QString &activePanel = QString());
    void saveExpandedFolders(const QStringList &folders);
    void saveEditorState(const QJsonObject &editorState);
    void scheduleSave();
    void saveNow();

    // Load
    QJsonObject load() const;

    // Convenience accessors for loaded data
    QJsonObject editorState() const;
    QJsonObject sidebarState() const;
    QStringList expandedFolders() const;
    QByteArray windowGeometry() const;
    QByteArray windowState() const;

    // Block saves during vault transitions
    void blockSaving();
    void unblockSaving();
```

Add new private member:

```cpp
    int m_saveBlockCount = 0;
```

- [ ] **Step 2: Remove the old saveOpenTabs method**

In `SessionManager.h`, remove this line:

```cpp
    void saveOpenTabs(const QJsonArray &tabs, int activeIndex);
```

- [ ] **Step 3: Implement the new methods in SessionManager.cpp**

Replace `saveOpenTabs` with the new methods. After `saveExpandedFolders`:

```cpp
void SessionManager::saveEditorState(const QJsonObject &editorState)
{
    m_data[QStringLiteral("editor")] = editorState;
    scheduleSave();
}
```

Add `saveSidebarState` with the new `activePanel` parameter — replace the existing implementation:

```cpp
void SessionManager::saveSidebarState(bool leftVisible, int leftWidth, bool rightVisible, int rightWidth,
                                       const QString &activePanel)
{
    QJsonObject sidebar;
    sidebar[QStringLiteral("leftVisible")] = leftVisible;
    sidebar[QStringLiteral("leftWidth")] = leftWidth;
    sidebar[QStringLiteral("rightVisible")] = rightVisible;
    sidebar[QStringLiteral("rightWidth")] = rightWidth;
    if (!activePanel.isEmpty()) {
        sidebar[QStringLiteral("activePanel")] = activePanel;
    }
    m_data[QStringLiteral("sidebar")] = sidebar;
    scheduleSave();
}
```

Add convenience accessors:

```cpp
QJsonObject SessionManager::editorState() const
{
    return m_data[QStringLiteral("editor")].toObject();
}

QJsonObject SessionManager::sidebarState() const
{
    return m_data[QStringLiteral("sidebar")].toObject();
}

QStringList SessionManager::expandedFolders() const
{
    QStringList result;
    auto arr = m_data[QStringLiteral("expandedFolders")].toArray();
    for (const auto &v : arr) {
        result.append(v.toString());
    }
    return result;
}

QByteArray SessionManager::windowGeometry() const
{
    return QByteArray::fromBase64(
        m_data[QStringLiteral("windowGeometry")].toString().toLatin1());
}

QByteArray SessionManager::windowState() const
{
    return QByteArray::fromBase64(
        m_data[QStringLiteral("windowState")].toString().toLatin1());
}

void SessionManager::blockSaving()
{
    ++m_saveBlockCount;
    m_saveTimer.stop();
}

void SessionManager::unblockSaving()
{
    if (m_saveBlockCount > 0) {
        --m_saveBlockCount;
    }
}
```

Modify `scheduleSave` to respect the block:

```cpp
void SessionManager::scheduleSave()
{
    if (m_saveBlockCount > 0) return;
    m_saveTimer.start();
}
```

- [ ] **Step 4: Add static helpers for building the editor state JSON**

Add a new utility section at the bottom of `SessionManager.cpp`. These are free functions in the `Corbomite` namespace, not member functions — they live close to SessionManager but operate on EditorViewManager/EditorViewSpace data passed in as JSON.

In `SessionManager.h`, add before the class closing brace:

```cpp
    // Static helpers for building editor state JSON
    static QJsonObject buildSplitLayoutJson(QSplitter *splitter, const QVector<EditorViewSpace *> &viewSpaces);

private:
    static QJsonValue encodeSplitterNode(QSplitter *splitter, const QVector<EditorViewSpace *> &viewSpaces);
```

Add the forward declarations at the top of SessionManager.h:

```cpp
class QSplitter;
```

And in the Corbomite namespace forward declarations:

```cpp
class EditorViewSpace;
```

Implement in SessionManager.cpp:

```cpp
QJsonObject SessionManager::buildSplitLayoutJson(QSplitter *splitter, const QVector<EditorViewSpace *> &viewSpaces)
{
    QJsonObject result;
    result[QStringLiteral("splitLayout")] = encodeSplitterNode(splitter, viewSpaces);
    return result;
}

QJsonValue SessionManager::encodeSplitterNode(QSplitter *splitter, const QVector<EditorViewSpace *> &viewSpaces)
{
    if (!splitter) return QJsonValue();

    // Check if any direct child is an EditorViewSpace (leaf)
    // or a QSplitter (branch)
    if (splitter->count() == 1) {
        // Could be a single pane or a single nested splitter
        if (auto *space = qobject_cast<EditorViewSpace *>(splitter->widget(0))) {
            int idx = viewSpaces.indexOf(space);
            return QStringLiteral("pane:%1").arg(idx);
        }
        if (auto *childSplitter = qobject_cast<QSplitter *>(splitter->widget(0))) {
            return encodeSplitterNode(childSplitter, viewSpaces);
        }
    }

    QJsonObject node;
    node[QStringLiteral("orientation")] =
        splitter->orientation() == Qt::Horizontal
            ? QStringLiteral("horizontal")
            : QStringLiteral("vertical");

    QJsonArray sizes;
    for (int s : splitter->sizes()) {
        sizes.append(s);
    }
    node[QStringLiteral("sizes")] = sizes;

    QJsonArray children;
    for (int i = 0; i < splitter->count(); ++i) {
        QWidget *child = splitter->widget(i);
        if (auto *space = qobject_cast<EditorViewSpace *>(child)) {
            int idx = viewSpaces.indexOf(space);
            children.append(QStringLiteral("pane:%1").arg(idx));
        } else if (auto *childSplitter = qobject_cast<QSplitter *>(child)) {
            children.append(encodeSplitterNode(childSplitter, viewSpaces));
        }
    }
    node[QStringLiteral("children")] = children;

    return node;
}
```

- [ ] **Step 5: Build and verify**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -20
```

Commit: `feat: extend SessionManager with split layout tree, per-tab state, and save blocking`

---

### Task 3: WelcomeScreen Widget

**Files:**
- Create: `src/app/WelcomeScreen.h`
- Create: `src/app/WelcomeScreen.cpp`
- Modify: `src/CMakeLists.txt`

A full-screen widget with generative artwork, vault list, and open/create buttons. Fills the central area when no vault is open.

- [ ] **Step 1: Create WelcomeScreen.h**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QPixmap>

class QListWidget;
class QPushButton;

namespace Corbomite {

class VaultService;

class WelcomeScreen : public QWidget {
    Q_OBJECT

public:
    explicit WelcomeScreen(VaultService *vaultService, QWidget *parent = nullptr);

    void refreshRecentVaults();

Q_SIGNALS:
    void vaultRequested(const QString &path);
    void openFolderRequested();
    void createVaultRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void generateArtwork();

    VaultService *m_vaultService;
    QListWidget *m_recentList;
    QPushButton *m_openButton;
    QPushButton *m_createButton;
    QPixmap m_artwork;
    int m_artworkSeed;
};

} // namespace Corbomite
```

- [ ] **Step 2: Create WelcomeScreen.cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "WelcomeScreen.h"
#include "VaultService.h"

#include <KLocalizedString>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QPainter>
#include <QPalette>
#include <QRandomGenerator>
#include <QFont>
#include <QFileInfo>
#include <QDir>

namespace Corbomite {

WelcomeScreen::WelcomeScreen(VaultService *vaultService, QWidget *parent)
    : QWidget(parent)
    , m_vaultService(vaultService)
    , m_artworkSeed(QRandomGenerator::global()->bounded(100000))
{
    // Outer layout centers content vertically
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setAlignment(Qt::AlignCenter);

    // Content container with max width
    auto *content = new QWidget(this);
    content->setMaximumWidth(500);
    content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setSpacing(16);

    // Artwork placeholder — painted in paintEvent
    auto *artworkWidget = new QWidget(content);
    artworkWidget->setFixedHeight(200);
    artworkWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(artworkWidget);

    // Title
    auto *titleLabel = new QLabel(content);
#ifdef CORBOMITE_DEV_BUILD
    titleLabel->setText(QStringLiteral("Corbomite [Dev]"));
#else
    titleLabel->setText(QStringLiteral("Corbomite"));
#endif
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(24);
    titleFont.setWeight(QFont::Light);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(titleLabel);

    contentLayout->addSpacing(8);

    // Recent vaults label
    auto *recentLabel = new QLabel(i18n("Recent Vaults"), content);
    QFont recentFont = recentLabel->font();
    recentFont.setWeight(QFont::DemiBold);
    recentLabel->setFont(recentFont);
    contentLayout->addWidget(recentLabel);

    // Recent vaults list
    m_recentList = new QListWidget(content);
    m_recentList->setMaximumHeight(240);
    m_recentList->setAlternatingRowColors(true);
    m_recentList->setSelectionMode(QAbstractItemView::SingleSelection);
    contentLayout->addWidget(m_recentList);

    connect(m_recentList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            Q_EMIT vaultRequested(path);
        }
    });

    // Also allow single-click + Enter
    connect(m_recentList, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            Q_EMIT vaultRequested(path);
        }
    });

    contentLayout->addSpacing(8);

    // Buttons row
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);

    m_openButton = new QPushButton(i18n("Open Folder..."), content);
    m_openButton->setIcon(QIcon::fromTheme(QStringLiteral("folder-open")));
    connect(m_openButton, &QPushButton::clicked, this, &WelcomeScreen::openFolderRequested);
    buttonLayout->addWidget(m_openButton);

    m_createButton = new QPushButton(i18n("Create New Vault..."), content);
    m_createButton->setIcon(QIcon::fromTheme(QStringLiteral("folder-new")));
    connect(m_createButton, &QPushButton::clicked, this, &WelcomeScreen::createVaultRequested);
    buttonLayout->addWidget(m_createButton);

    contentLayout->addLayout(buttonLayout);

    outerLayout->addWidget(content);

    refreshRecentVaults();
}

void WelcomeScreen::refreshRecentVaults()
{
    m_recentList->clear();

    QStringList recent = m_vaultService->recentVaults();
    int count = qMin(recent.size(), 8);

    for (int i = 0; i < count; ++i) {
        const QString &path = recent.at(i);
        QFileInfo info(path);
        QString name = info.fileName();
        QString dir = info.absolutePath();

        auto *item = new QListWidgetItem(m_recentList);
        item->setText(name + QStringLiteral("    ") + dir);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
    }

    // Show/hide the list based on whether there are recent vaults
    m_recentList->setVisible(count > 0);
}

void WelcomeScreen::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    if (m_artwork.isNull() || m_artwork.size() != size()) {
        generateArtwork();
    }

    QPainter painter(this);
    painter.drawPixmap(0, 0, m_artwork);
}

void WelcomeScreen::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_artwork = QPixmap(); // invalidate, will regenerate in paintEvent
}

void WelcomeScreen::generateArtwork()
{
    if (width() <= 0 || height() <= 0) return;

    m_artwork = QPixmap(size());
    m_artwork.fill(Qt::transparent);

    QPainter painter(&m_artwork);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRandomGenerator rng(m_artworkSeed);
    const QPalette &pal = palette();

    // Use palette colors for theme awareness
    QColor highlight = pal.color(QPalette::Highlight);
    QColor accent = pal.color(QPalette::Link);
    QColor base = pal.color(QPalette::Base);

    QList<QColor> colors = {
        highlight,
        accent,
        QColor(highlight.red(), accent.green(), base.blue()),
        QColor(accent.red(), highlight.green(), accent.blue()),
    };

    // Draw overlapping translucent ellipses in the top portion
    int artHeight = qMin(200, height() / 3);
    int centerX = width() / 2;
    int centerY = artHeight / 2;

    for (int i = 0; i < 7; ++i) {
        QColor c = colors.at(rng.bounded(colors.size()));
        c.setAlpha(30 + rng.bounded(40));

        int rx = 60 + rng.bounded(140);
        int ry = 40 + rng.bounded(80);
        int ox = centerX + rng.bounded(201) - 100;
        int oy = centerY + rng.bounded(101) - 50;

        painter.setPen(Qt::NoPen);
        painter.setBrush(c);
        painter.drawEllipse(QPoint(ox, oy), rx, ry);
    }
}

} // namespace Corbomite
```

- [ ] **Step 3: Add WelcomeScreen.cpp to CMakeLists.txt**

In `src/CMakeLists.txt`, add the new source file after `app/SessionManager.cpp`:

```cpp
    app/WelcomeScreen.cpp
```

- [ ] **Step 4: Build and verify**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -20
```

Commit: `feat: add WelcomeScreen widget with generative artwork and recent vault list`

---

### Task 4: MainWindow QStackedWidget Restructuring

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

Wrap the editor area in a QStackedWidget. Index 0 = WelcomeScreen, Index 1 = editor area (m_editorManager). The welcome screen shows on startup, switches to editor on vault open.

- [ ] **Step 1: Add new members to MainWindow.h**

Add the forward declaration after `class DailyNoteService;`:

```cpp
class WelcomeScreen;
```

Add new members after `DailyNoteService *m_dailyNoteService = nullptr;`:

```cpp
    QStackedWidget *m_centralStack = nullptr;
    WelcomeScreen *m_welcomeScreen = nullptr;
```

Add the include at the top of MainWindow.h:

```cpp
#include <QStackedWidget>
```

- [ ] **Step 2: Add WelcomeScreen include to MainWindow.cpp**

Add at the top of MainWindow.cpp with the other includes:

```cpp
#include "WelcomeScreen.h"
```

Also add `QStackedWidget` include if not already present (check — it's included by CorbomiteMDI.h indirectly but be explicit):

```cpp
#include <QStackedWidget>
```

- [ ] **Step 3: Rewrite setupEditor() to use QStackedWidget**

Replace the entire `setupEditor()` method:

```cpp
void MainWindow::setupEditor()
{
    // Create the stacked widget inside the MDI central area
    m_centralStack = new QStackedWidget(centralWidget());
    centralWidget()->layout()->addWidget(m_centralStack);

    // Index 0: Welcome screen
    m_welcomeScreen = new WelcomeScreen(m_vaultService, m_centralStack);
    m_centralStack->addWidget(m_welcomeScreen);

    connect(m_welcomeScreen, &WelcomeScreen::vaultRequested, this, [this](const QString &path) {
        m_vaultService->openVault(path);
    });
    connect(m_welcomeScreen, &WelcomeScreen::openFolderRequested, this, &MainWindow::openVaultDialog);
    connect(m_welcomeScreen, &WelcomeScreen::createVaultRequested, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, i18n("Create New Vault"), QDir::homePath());
        if (!dir.isEmpty()) {
            // Create .corbomite config directory
            QDir(dir).mkpath(QStringLiteral(".corbomite"));
            m_vaultService->openVault(dir);
        }
    });

    // Index 1: Editor view manager
    m_editorManager = new EditorViewManager(m_centralStack);
    m_centralStack->addWidget(m_editorManager);

    connect(m_editorManager, &EditorViewManager::cursorInfoChanged,
            this, &MainWindow::onCursorInfoChanged);

    // Start on welcome screen
    m_centralStack->setCurrentIndex(0);
    setSidebarsVisible(false);
}
```

- [ ] **Step 4: Fix split pane action connections in setupActions()**

The split pane shortcuts at lines 325-333 connect directly to `m_editorManager`, which is now created inside `setupEditor()` — but `setupActions()` is called first in the constructor. We need to move those connections or use lambdas. Replace the split pane connections at the end of `setupActions()`:

```cpp
    // Split pane shortcuts
    auto *splitRight = ac->addAction(QStringLiteral("split_right"));
    splitRight->setText(i18n("Split Right"));
    ac->setDefaultShortcut(splitRight, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Right));
    connect(splitRight, &QAction::triggered, this, [this]() {
        if (m_editorManager) m_editorManager->splitActiveHorizontal();
    });

    auto *splitDown = ac->addAction(QStringLiteral("split_down"));
    splitDown->setText(i18n("Split Down"));
    ac->setDefaultShortcut(splitDown, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down));
    connect(splitDown, &QAction::triggered, this, [this]() {
        if (m_editorManager) m_editorManager->splitActiveVertical();
    });
```

- [ ] **Step 5: Build and verify**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -20
```

Run the app and verify:
- App starts showing welcome screen
- Sidebars are hidden
- Menu bar is visible
- "Open Vault" action still works

Commit: `feat: restructure MainWindow with QStackedWidget for welcome screen / editor swap`

---

### Task 5: Close Vault Action and Vault Flow Changes

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

Add a Close Vault action. Modify open vault / open recent flows to call queryClose first. Wire the welcome screen visibility to vault state.

- [ ] **Step 1: Add closeVault() method to MainWindow.h**

Add in the private section after `void openVaultDialog();`:

```cpp
    void closeVault();
```

- [ ] **Step 2: Add the Close Vault action in setupActions()**

Add after the `m_recentVaults` setup (after line 161 — `m_recentVaults->loadEntries(recentGroup);`):

```cpp
    auto *closeVaultAction = ac->addAction(QStringLiteral("file_close_vault"));
    closeVaultAction->setText(i18n("Close Vault"));
    closeVaultAction->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
    connect(closeVaultAction, &QAction::triggered, this, &MainWindow::closeVault);
```

- [ ] **Step 3: Modify the Open Recent handler to queryClose first**

Replace the `m_recentVaults` connection (currently at line ~154-156):

```cpp
    m_recentVaults = KStandardAction::openRecent(this, [this](const QUrl &url) {
        if (m_vaultService->isOpen()) {
            if (!m_editorManager->queryClose()) return;
        }
        m_vaultService->openVault(url.toLocalFile());
    }, ac);
```

- [ ] **Step 4: Modify openVaultDialog() to queryClose first**

Replace the `openVaultDialog()` method:

```cpp
void MainWindow::openVaultDialog()
{
    if (m_vaultService->isOpen()) {
        if (!m_editorManager->queryClose()) return;
    }

    QString dir = QFileDialog::getExistingDirectory(
        this, i18n("Open Vault"), QDir::homePath());
    if (!dir.isEmpty()) {
        if (!m_vaultService->openVault(dir)) {
            KMessageBox::error(this,
                i18n("Could not open vault at:\n%1\n\nThe directory may not exist or is not readable.", dir),
                i18n("Open Vault Failed"));
        }
    }
}
```

- [ ] **Step 5: Modify the WelcomeScreen vaultRequested handler to queryClose**

In `setupEditor()`, update the `vaultRequested` connection:

```cpp
    connect(m_welcomeScreen, &WelcomeScreen::vaultRequested, this, [this](const QString &path) {
        if (m_vaultService->isOpen()) {
            if (!m_editorManager->queryClose()) return;
        }
        m_vaultService->openVault(path);
    });
```

- [ ] **Step 6: Implement closeVault()**

Add in MainWindow.cpp:

```cpp
void MainWindow::closeVault()
{
    if (!m_vaultService->isOpen()) return;

    if (!m_editorManager->queryClose()) return;

    m_vaultService->closeVault();
}
```

- [ ] **Step 7: Add Close Vault to updateVaultActions()**

In `updateVaultActions()`, add after the existing `setEnabled` calls:

```cpp
    setEnabled(QStringLiteral("file_close_vault"), open);
```

- [ ] **Step 8: Modify closeEvent to queryClose**

Replace the `closeEvent()` method:

```cpp
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_vaultService->isOpen()) {
        if (!m_editorManager->queryClose()) {
            event->ignore();
            return;
        }

        // Save session before quitting (full save happens in onVaultClosed
        // but we need to save while state is still alive)
        if (m_sessionManager) {
            m_sessionManager->saveWindowGeometry(saveGeometry(), saveState());
            m_sessionManager->saveNow();
        }
    }

    CorbomiteMDI::MainWindow::closeEvent(event);
}
```

- [ ] **Step 9: Build and verify**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -20
```

Commit: `feat: add Close Vault action with unsaved-changes prompt on all vault transitions`

---

### Task 6: Full Session Save/Restore and Signal Cleanup

**Files:**
- Modify: `src/app/MainWindow.cpp`
- Modify: `src/editor/EditorViewManager.h`
- Modify: `src/editor/EditorViewManager.cpp`

This is the task that fixes the crash. Rewrite `onVaultOpened()` and `onVaultClosed()` to properly save/restore the full session state (split layout, per-tab cursor/mode) and clean up EditorViewManager on vault close. Add helpers for building and restoring split layout.

- [ ] **Step 1: Add restoreSplitLayout and buildEditorStateJson to EditorViewManager**

In `EditorViewManager.h`, add new public methods after `void closeAllDocuments();`:

```cpp
    void restoreFromSession(const QJsonObject &editorState,
                            std::function<void(const QString &path, EditorViewSpace *space)> openTabCallback);
    QJsonObject buildSessionState() const;
    QVector<EditorViewSpace *> viewSpaces() const;
    QSplitter *rootSplitter() const;
```

In `EditorViewManager.cpp`, add at the top:

```cpp
#include <QJsonObject>
#include <QJsonArray>
#include "SessionManager.h"
```

Implement the new methods:

```cpp
QVector<EditorViewSpace *> EditorViewManager::viewSpaces() const
{
    return m_viewSpaces;
}

QSplitter *EditorViewManager::rootSplitter() const
{
    return m_rootSplitter;
}

QJsonObject EditorViewManager::buildSessionState() const
{
    QJsonObject editor;

    // Build panes array
    QJsonArray panes;
    for (auto *space : std::as_const(m_viewSpaces)) {
        QJsonObject pane;
        QJsonArray tabs;

        auto *tabBar = space->findChild<QTabBar *>();
        if (tabBar) {
            for (int i = 0; i < tabBar->count(); ++i) {
                QString path = tabBar->tabData(i).toString();
                if (path == QStringLiteral("__graph__")) continue; // skip graph tabs

                QJsonObject tab;
                tab[QStringLiteral("path")] = path;

                if (path.endsWith(QStringLiteral(".canvas"))) {
                    tab[QStringLiteral("type")] = QStringLiteral("canvas");
                } else {
                    tab[QStringLiteral("type")] = QStringLiteral("note");

                    // Check if in reading mode
                    if (space->isPreviewMode() && tabBar->currentIndex() == i) {
                        tab[QStringLiteral("mode")] = QStringLiteral("reading");
                    } else {
                        tab[QStringLiteral("mode")] = QStringLiteral("source");
                    }

                    // Get cursor position from editor
                    auto *editor = space->activeEditor();
                    if (editor && editor->noteDocument()
                        && editor->noteDocument()->relativePath() == path) {
                        tab[QStringLiteral("cursorLine")] = editor->currentLine();
                        tab[QStringLiteral("cursorColumn")] = editor->currentColumn();
                        // scrollY from the editor's vertical scrollbar
                        tab[QStringLiteral("scrollY")] = editor->verticalScrollBar()->value();
                    }
                }

                tabs.append(tab);
            }
        }

        pane[QStringLiteral("tabs")] = tabs;
        pane[QStringLiteral("activeTabIndex")] = tabBar ? tabBar->currentIndex() : 0;
        panes.append(pane);
    }
    editor[QStringLiteral("panes")] = panes;

    // Build split layout tree
    editor[QStringLiteral("splitLayout")] =
        SessionManager::encodeSplitterNode(m_rootSplitter, m_viewSpaces);

    return editor;
}
```

Add the includes needed for scrollbar access:

```cpp
#include <QScrollBar>
#include <QTabBar>
```

- [ ] **Step 2: Implement restoreFromSession()**

In `EditorViewManager.cpp`:

```cpp
void EditorViewManager::restoreFromSession(
    const QJsonObject &editorState,
    std::function<void(const QString &path, EditorViewSpace *space)> openTabCallback)
{
    auto panesArray = editorState[QStringLiteral("panes")].toArray();
    auto splitLayout = editorState[QStringLiteral("splitLayout")];

    if (panesArray.isEmpty()) return;

    // If we have more than one pane in the session, we need to build the split tree
    int paneCount = panesArray.size();

    if (paneCount == 1) {
        // Single pane — use the existing one (m_viewSpaces[0])
        // Just open the tabs
        auto pane = panesArray[0].toObject();
        auto tabs = pane[QStringLiteral("tabs")].toArray();
        auto *space = m_viewSpaces.first();

        for (const auto &tabVal : tabs) {
            auto tab = tabVal.toObject();
            QString path = tab[QStringLiteral("path")].toString();
            if (path.isEmpty()) continue;
            openTabCallback(path, space);
        }

        // Set active tab
        int activeIdx = pane[QStringLiteral("activeTabIndex")].toInt(0);
        auto *tabBar = space->findChild<QTabBar *>();
        if (tabBar && activeIdx >= 0 && activeIdx < tabBar->count()) {
            tabBar->setCurrentIndex(activeIdx);
        }

        // Restore cursor/scroll for active tab
        restoreTabState(panesArray[0].toObject(), space);
        return;
    }

    // Multiple panes — create additional view spaces
    for (int i = 1; i < paneCount; ++i) {
        auto *space = createViewSpace();
        // Don't add to splitter yet — we'll rebuild the layout
        space->setParent(this);
        space->hide();
    }

    // Rebuild the split layout from JSON
    rebuildSplitLayout(splitLayout, m_rootSplitter);

    // Open tabs in each pane
    for (int i = 0; i < paneCount && i < m_viewSpaces.size(); ++i) {
        auto pane = panesArray[i].toObject();
        auto tabs = pane[QStringLiteral("tabs")].toArray();
        auto *space = m_viewSpaces.at(i);

        for (const auto &tabVal : tabs) {
            auto tab = tabVal.toObject();
            QString path = tab[QStringLiteral("path")].toString();
            if (path.isEmpty()) continue;
            openTabCallback(path, space);
        }

        // Set active tab
        int activeIdx = pane[QStringLiteral("activeTabIndex")].toInt(0);
        auto *tabBar = space->findChild<QTabBar *>();
        if (tabBar && activeIdx >= 0 && activeIdx < tabBar->count()) {
            tabBar->setCurrentIndex(activeIdx);
        }

        space->show();
        restoreTabState(pane, space);
    }

    // Activate the first pane
    if (!m_viewSpaces.isEmpty()) {
        setActiveViewSpace(m_viewSpaces.first());
        Q_EMIT activeEditorChanged(m_viewSpaces.first()->activeEditor());
    }
}
```

- [ ] **Step 3: Add private helper methods to EditorViewManager**

In `EditorViewManager.h`, add private methods:

```cpp
    void rebuildSplitLayout(const QJsonValue &node, QSplitter *parent);
    void restoreTabState(const QJsonObject &paneJson, EditorViewSpace *space);
```

In `EditorViewManager.cpp`:

```cpp
void EditorViewManager::rebuildSplitLayout(const QJsonValue &node, QSplitter *parent)
{
    if (node.isString()) {
        // Leaf: "pane:N"
        QString ref = node.toString();
        if (ref.startsWith(QStringLiteral("pane:"))) {
            int idx = ref.mid(5).toInt();
            if (idx >= 0 && idx < m_viewSpaces.size()) {
                parent->addWidget(m_viewSpaces.at(idx));
            }
        }
        return;
    }

    if (!node.isObject()) return;

    auto obj = node.toObject();
    auto children = obj[QStringLiteral("children")].toArray();
    auto orientation = obj[QStringLiteral("orientation")].toString();
    auto sizesArray = obj[QStringLiteral("sizes")].toArray();

    Qt::Orientation orient = (orientation == QStringLiteral("vertical"))
        ? Qt::Vertical : Qt::Horizontal;

    if (parent->count() == 0 && parent == m_rootSplitter) {
        // Reuse the root splitter
        parent->setOrientation(orient);
        for (const auto &child : children) {
            if (child.isString()) {
                rebuildSplitLayout(child, parent);
            } else {
                auto *nested = new QSplitter(this);
                parent->addWidget(nested);
                rebuildSplitLayout(child, nested);
            }
        }
    } else {
        parent->setOrientation(orient);
        for (const auto &child : children) {
            if (child.isString()) {
                rebuildSplitLayout(child, parent);
            } else {
                auto *nested = new QSplitter(this);
                parent->addWidget(nested);
                rebuildSplitLayout(child, nested);
            }
        }
    }

    // Restore sizes
    QList<int> sizes;
    for (const auto &s : sizesArray) {
        sizes.append(s.toInt());
    }
    if (!sizes.isEmpty()) {
        parent->setSizes(sizes);
    }
}

void EditorViewManager::restoreTabState(const QJsonObject &paneJson, EditorViewSpace *space)
{
    auto tabs = paneJson[QStringLiteral("tabs")].toArray();
    int activeIdx = paneJson[QStringLiteral("activeTabIndex")].toInt(0);

    auto *tabBar = space->findChild<QTabBar *>();
    if (!tabBar) return;

    // Restore cursor/scroll for the active tab's note
    if (activeIdx >= 0 && activeIdx < tabs.size()) {
        auto tab = tabs[activeIdx].toObject();
        QString type = tab[QStringLiteral("type")].toString();
        QString mode = tab[QStringLiteral("mode")].toString();

        if (type == QStringLiteral("note")) {
            // Restore reading mode
            if (mode == QStringLiteral("reading") && !space->isPreviewMode()) {
                space->toggleEditorMode();
            }

            // Restore cursor position
            auto *editor = space->activeEditor();
            if (editor) {
                int line = tab[QStringLiteral("cursorLine")].toInt(0);
                int col = tab[QStringLiteral("cursorColumn")].toInt(0);
                int scrollY = tab[QStringLiteral("scrollY")].toInt(0);

                QTextCursor cursor(editor->document()->findBlockByLineNumber(line - 1));
                if (col > 0) {
                    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                                        qMin(col - 1, cursor.block().length() - 1));
                }
                editor->setTextCursor(cursor);
                editor->verticalScrollBar()->setValue(scrollY);
            }
        }
    }
}
```

- [ ] **Step 4: Make SessionManager::encodeSplitterNode public**

In `SessionManager.h`, move `encodeSplitterNode` from private to public:

```cpp
    static QJsonObject buildSplitLayoutJson(QSplitter *splitter, const QVector<EditorViewSpace *> &viewSpaces);
    static QJsonValue encodeSplitterNode(QSplitter *splitter, const QVector<EditorViewSpace *> &viewSpaces);
```

- [ ] **Step 5: Rewrite onVaultOpened() with full session restore**

Replace `onVaultOpened()` entirely:

```cpp
void MainWindow::onVaultOpened()
{
    auto *vault = m_vaultService->vault();
    updateWindowTitle();

    // Switch from welcome screen to editor
    m_centralStack->setCurrentIndex(1);
    setSidebarsVisible(true);

    // Track in recent vaults
    m_recentVaults->addUrl(QUrl::fromLocalFile(vault->path()));
    auto config = KSharedConfig::openConfig();
    KConfigGroup recentGroup = config->group(QStringLiteral("RecentVaults"));
    m_recentVaults->saveEntries(recentGroup);
    config->sync();

    delete m_treeModel;
    m_treeModel = new NotesTreeModel(vault, this);
    m_fileExplorer->setModel(m_treeModel);

    // Create autosave reactor
    delete m_autosave;
    m_autosave = new AutosaveReactor(m_vaultService->noteService(), this);

    // Create file watch reactor
    delete m_fileWatch;
    m_fileWatch = new FileWatchReactor(vault, this);
    m_fileWatch->startWatching(vault->path());

    // Connect autosave's noteSaved to file watcher suppression
    connect(m_autosave, &AutosaveReactor::noteSaved, this, [this](const QString &relativePath) {
        if (m_fileWatch && m_vaultService->vault()) {
            QString absPath = m_vaultService->vault()->path() + QLatin1Char('/') + relativePath;
            m_fileWatch->suppressPath(absPath);
        }
    });

    // Watch documents as they are opened in the editor
    connect(m_editorManager, &EditorViewManager::activeEditorChanged,
            this, [this](NoteEditorWidget *editor) {
        if (editor && editor->noteDocument() && m_autosave) {
            m_autosave->watchDocument(editor->noteDocument());
        }
    });

    // Set vault on editors and connect link navigation
    connect(m_editorManager, &EditorViewManager::activeEditorChanged,
            this, [this](NoteEditorWidget *editor) {
        if (!editor) return;
        if (m_vaultService->vault()) {
            editor->setVaultModel(m_vaultService->vault());
        }
        connect(editor, &NoteEditorWidget::linkActivated,
                this, &MainWindow::onNoteActivated, Qt::UniqueConnection);
    });

    // Create search index
    delete m_searchIndex;
    m_searchIndex = new SQLiteIndex(this);
    m_searchIndex->open(vault->configPath() + QStringLiteral("/index.sqlite"));

    statusBar()->showMessage(i18n("Indexing vault..."));
    connect(m_searchIndex, &SQLiteIndex::indexReady, this, [this]() {
        statusBar()->showMessage(i18n("Indexing complete"), 3000);
    });
    m_searchIndex->rebuildIndexAsync(vault->path());

    m_searchPanel->setIndex(m_searchIndex);
    m_vaultService->noteService()->setSearchIndex(m_searchIndex);
    m_vaultService->vault()->setSearchIndex(m_searchIndex);

    // Set index on sidebar panels
    m_backlinksPanel->setIndex(m_searchIndex);
    m_outlinksPanel->setIndex(m_searchIndex);
    m_outlinksPanel->setVaultModel(vault);
    m_localGraphPanel->setIndex(m_searchIndex);
    m_localGraphPanel->setVaultModel(vault);

    // Update sidebar panels when active note changes
    connect(m_editorManager, &EditorViewManager::activeEditorChanged,
            this, [this](NoteEditorWidget *editor) {
        if (editor && editor->noteDocument()) {
            m_backlinksPanel->setCurrentNote(editor->noteDocument());
            m_outlinksPanel->setCurrentNote(editor->noteDocument());
            m_outlinePanel->setCurrentNote(editor->noteDocument());
            m_localGraphPanel->setCurrentNote(editor->noteDocument());
        } else {
            m_backlinksPanel->setCurrentNote(nullptr);
            m_outlinksPanel->setCurrentNote(nullptr);
            m_outlinePanel->setCurrentNote(nullptr);
            m_localGraphPanel->setCurrentNote(nullptr);
        }
    });

    // Update window title when active note changes
    connect(m_editorManager, &EditorViewManager::activeEditorChanged,
            this, [this](NoteEditorWidget *editor) {
        updateWindowTitle(editor);
        if (editor && editor->noteDocument()) {
            disconnect(editor->noteDocument(), &NoteDocument::modificationChanged,
                       this, nullptr);
            connect(editor->noteDocument(), &NoteDocument::modificationChanged,
                    this, [this]() {
                updateWindowTitle(m_editorManager->activeEditor());
            });
        }
    });

    // Update index on note saves
    connect(m_autosave, &AutosaveReactor::noteSaved, this, [this](const QString &relPath) {
        if (!m_searchIndex || !m_vaultService->vault()) return;
        auto *doc = m_vaultService->vault()->cachedDocument(relPath);
        if (doc) {
            m_searchIndex->indexNote(relPath, doc->name(), doc->markdown());
        }
    });

    // Connect internal link navigation from all view spaces
    for (auto *space : m_editorManager->viewSpaces()) {
        connect(space, &EditorViewSpace::internalLinkClicked,
                this, &MainWindow::onNoteActivated, Qt::UniqueConnection);
    }

    // Connect graph view note navigation
    connect(m_editorManager, &EditorViewManager::graphNoteActivated,
            this, &MainWindow::onNoteActivated, Qt::UniqueConnection);

    // Create session manager and restore session
    delete m_sessionManager;
    m_sessionManager = new SessionManager(this);
    m_sessionManager->setSessionPath(vault->configPath() + QStringLiteral("/session.json"));

    auto session = m_sessionManager->load();

    // Restore window geometry
    if (session.contains(QStringLiteral("windowGeometry"))) {
        restoreGeometry(m_sessionManager->windowGeometry());
        restoreState(m_sessionManager->windowState());
    }

    // Restore editor state (tabs, splits, cursor positions)
    auto editorState = session[QStringLiteral("editor")].toObject();
    if (!editorState.isEmpty()) {
        m_sessionManager->blockSaving(); // prevent saves during restore
        m_editorManager->restoreFromSession(editorState,
            [this](const QString &path, EditorViewSpace *space) {
                // Save/restore active space so we open in the right pane
                auto *prevActive = m_editorManager->activeViewSpace();
                // Temporarily make this space active for openNote/openCanvas routing
                if (space != prevActive) {
                    // We need a way to open in a specific space — use direct call
                    if (path.endsWith(QStringLiteral(".canvas"))) {
                        QString absPath = m_vaultService->vault()->path() + QLatin1Char('/') + path;
                        space->openCanvas(absPath);
                    } else {
                        auto *doc = m_vaultService->noteService()->openNote(path);
                        if (doc) {
                            space->openNote(doc);
                        }
                    }
                } else {
                    onNoteActivated(path);
                }
            });
        m_sessionManager->unblockSaving();
    } else if (session.contains(QStringLiteral("tabs"))) {
        // Backward compat: restore old flat tab format
        auto tabs = session[QStringLiteral("tabs")].toArray();
        for (const auto &tabVal : tabs) {
            auto tab = tabVal.toObject();
            QString path = tab[QStringLiteral("path")].toString();
            if (!path.isEmpty()) {
                onNoteActivated(path);
            }
        }
    }

    // Restore expanded folders
    QStringList expandedFolders;
    if (session.contains(QStringLiteral("expandedFolders"))) {
        auto arr = session[QStringLiteral("expandedFolders")].toArray();
        for (const auto &v : arr) {
            expandedFolders.append(v.toString());
        }
        m_fileExplorer->restoreExpandedFolders(expandedFolders);
    }

    // Template and Daily Note services
    auto *settings = CorbomiteSettings::self();

    delete m_templateService;
    m_templateService = new TemplateService(vault, this);
    m_templateService->setTemplateFolder(settings->templateFolder());
    m_templateService->setDefaultDateFormat(settings->defaultDateFormat());
    m_templateService->setDefaultTimeFormat(settings->defaultTimeFormat());

    delete m_dailyNoteService;
    m_dailyNoteService = new DailyNoteService(vault, m_vaultService->noteService(),
                                                m_templateService, this);
    m_dailyNoteService->setDateFormat(settings->dailyNoteDateFormat());
    m_dailyNoteService->setFolder(settings->dailyNoteFolder());
    m_dailyNoteService->setTemplateName(settings->dailyNoteTemplate());

    updateVaultActions();
}
```

- [ ] **Step 6: Rewrite onVaultClosed() with full cleanup**

Replace `onVaultClosed()` entirely:

```cpp
void MainWindow::onVaultClosed()
{
    // Save session before cleanup
    if (m_sessionManager) {
        m_sessionManager->saveWindowGeometry(saveGeometry(), saveState());

        // Save sidebar state
        m_sessionManager->saveSidebarState(
            sidebarsVisible(), 200, false, 200);

        // Save editor state (tabs, splits, cursor positions)
        m_sessionManager->saveEditorState(m_editorManager->buildSessionState());

        // Save expanded folders
        if (m_fileExplorer) {
            m_sessionManager->saveExpandedFolders(m_fileExplorer->expandedFolders());
        }

        m_sessionManager->saveNow();
    }

    // Disconnect all vault-specific signals from editor manager
    disconnect(m_editorManager, &EditorViewManager::activeEditorChanged, this, nullptr);
    disconnect(m_editorManager, &EditorViewManager::graphNoteActivated, this, nullptr);

    // Reconnect the non-vault-specific ones
    connect(m_editorManager, &EditorViewManager::cursorInfoChanged,
            this, &MainWindow::onCursorInfoChanged);

    // Clean up all editor state — this is the crash fix
    m_editorManager->closeAllDocuments();

    // Delete vault-scoped services
    delete m_autosave;
    m_autosave = nullptr;

    if (m_fileWatch) {
        m_fileWatch->stopWatching();
    }
    delete m_fileWatch;
    m_fileWatch = nullptr;

    delete m_sessionManager;
    m_sessionManager = nullptr;

    delete m_templateService;
    m_templateService = nullptr;
    delete m_dailyNoteService;
    m_dailyNoteService = nullptr;

    m_vaultService->noteService()->setSearchIndex(nullptr);

    // Clear sidebar panels
    m_backlinksPanel->setIndex(nullptr);
    m_backlinksPanel->setCurrentNote(nullptr);
    m_outlinksPanel->setIndex(nullptr);
    m_outlinksPanel->setVaultModel(nullptr);
    m_outlinksPanel->setCurrentNote(nullptr);
    m_outlinePanel->setCurrentNote(nullptr);
    m_localGraphPanel->setIndex(nullptr);
    m_localGraphPanel->setVaultModel(nullptr);
    m_localGraphPanel->setCurrentNote(nullptr);

    delete m_searchIndex;
    m_searchIndex = nullptr;
    m_searchPanel->setIndex(nullptr);

    delete m_treeModel;
    m_treeModel = nullptr;
    m_fileExplorer->setModel(nullptr);

    // Switch to welcome screen
    m_centralStack->setCurrentIndex(0);
    m_welcomeScreen->refreshRecentVaults();
    setSidebarsVisible(false);

    updateWindowTitle();
    updateVaultActions();
}
```

- [ ] **Step 7: Rewrite closeEvent() to save full session**

Replace `closeEvent()`:

```cpp
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_vaultService->isOpen()) {
        if (!m_editorManager->queryClose()) {
            event->ignore();
            return;
        }

        // Save full session state before quitting
        if (m_sessionManager) {
            m_sessionManager->saveWindowGeometry(saveGeometry(), saveState());
            m_sessionManager->saveSidebarState(
                sidebarsVisible(), 200, false, 200);
            m_sessionManager->saveEditorState(m_editorManager->buildSessionState());
            if (m_fileExplorer) {
                m_sessionManager->saveExpandedFolders(m_fileExplorer->expandedFolders());
            }
            m_sessionManager->saveNow();
        }
    }

    CorbomiteMDI::MainWindow::closeEvent(event);
}
```

- [ ] **Step 8: Check FileExplorerPanel has expandedFolders() and restoreExpandedFolders()**

If `FileExplorerPanel` doesn't have these methods, add stubs. Check with:

```bash
grep -n "expandedFolders\|restoreExpandedFolders" src/sidebar/FileExplorerPanel.h
```

If missing, add to `FileExplorerPanel.h`:

```cpp
    QStringList expandedFolders() const;
    void restoreExpandedFolders(const QStringList &folders);
```

And stub implementations in `FileExplorerPanel.cpp`:

```cpp
QStringList FileExplorerPanel::expandedFolders() const
{
    // TODO: implement tree view expanded state tracking
    return {};
}

void FileExplorerPanel::restoreExpandedFolders(const QStringList &folders)
{
    Q_UNUSED(folders);
    // TODO: implement tree view expanded state restore
}
```

- [ ] **Step 9: Build and test the full flow**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -30
```

Manual test sequence:
1. Launch app — welcome screen shows
2. Open a vault — editor appears with sidebars
3. Open several tabs, split panes
4. Close vault — welcome screen shows, no crash
5. Reopen same vault — tabs and splits restored
6. Open vault A, switch to vault B — no crash
7. Close app with vault open — session saved
8. Relaunch, open vault — previous state restored

```bash
cd build && ctest --output-on-failure
```

Commit: `feat: full session save/restore with split layout, cursor state, and vault-switch crash fix`
