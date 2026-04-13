// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorViewManager.h"
#include "EditorViewSpace.h"
#include "NoteEditorWidget.h"
#include <markoff/Editor.h>
#include "SessionManager.h"
#include "corbomite/core/RegexRenderEngine.h"
#include "corbomite/core/RenderProfile.h"
#include "corbomite/core/NoteDocument.h"
#include <QVBoxLayout>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollBar>
#include <QTabBar>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardGuiItem>

namespace Corbomite {

EditorViewManager::EditorViewManager(QWidget *parent)
    : QWidget(parent)
    , m_rootSplitter(new QSplitter(Qt::Horizontal, this))
    , m_canvasEngine(std::make_unique<RegexRenderEngine>())
{
    m_canvasEngine->setProfile(RenderProfile::canvasCard());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_rootSplitter);

    // Create the initial view space
    auto *initialSpace = createViewSpace();
    m_rootSplitter->addWidget(initialSpace);
    setActiveViewSpace(initialSpace);
}

EditorViewManager::~EditorViewManager() = default;

EditorViewSpace *EditorViewManager::createViewSpace()
{
    auto *space = new EditorViewSpace(this);
    space->setCanvasEngine(m_canvasEngine.get());
    connectViewSpace(space);
    m_viewSpaces.append(space);
    return space;
}

void EditorViewManager::connectViewSpace(EditorViewSpace *space)
{
    // When this space is clicked/focused, make it active
    connect(space, &EditorViewSpace::activeEditorChanged,
            this, [this, space](NoteEditorWidget *editor) {
        setActiveViewSpace(space);
        Q_EMIT activeEditorChanged(editor);
    });
    connect(space, &EditorViewSpace::cursorInfoChanged,
            this, [this, space](int line, int column, int wordCount) {
        if (space == m_activeViewSpace) {
            Q_EMIT cursorInfoChanged(line, column, wordCount);
        }
    });
    connect(space, &EditorViewSpace::graphNoteActivated,
            this, &EditorViewManager::graphNoteActivated);
    connect(space, &EditorViewSpace::splitRightRequested,
            this, &EditorViewManager::splitActiveHorizontal);
    connect(space, &EditorViewSpace::splitDownRequested,
            this, &EditorViewManager::splitActiveVertical);
}

void EditorViewManager::setActiveViewSpace(EditorViewSpace *space)
{
    if (m_activeViewSpace == space) return;
    m_activeViewSpace = space;
}

void EditorViewManager::openNote(NoteDocument *doc)
{
    if (m_activeViewSpace) m_activeViewSpace->openNote(doc);
}

void EditorViewManager::openCanvas(const QString &filePath)
{
    if (m_activeViewSpace) m_activeViewSpace->openCanvas(filePath);
}

NoteEditorWidget *EditorViewManager::activeEditor() const
{
    return m_activeViewSpace ? m_activeViewSpace->activeEditor() : nullptr;
}

EditorViewSpace *EditorViewManager::activeViewSpace() const
{
    return m_activeViewSpace;
}

void EditorViewManager::setViewMode(NoteEditorWidget::ViewMode mode)
{
    if (m_activeViewSpace) m_activeViewSpace->setViewMode(mode);
}

NoteEditorWidget::ViewMode EditorViewManager::viewMode() const
{
    return m_activeViewSpace ? m_activeViewSpace->viewMode() : NoteEditorWidget::ViewMode::Editing;
}

void EditorViewManager::openGraphView(SQLiteIndex *index, VaultModel *vault)
{
    if (m_activeViewSpace) m_activeViewSpace->openGraphView(index, vault);
}

bool EditorViewManager::hasGraphView() const
{
    return m_activeViewSpace ? m_activeViewSpace->hasGraphView() : false;
}

int EditorViewManager::viewSpaceCount() const
{
    return m_viewSpaces.size();
}

void EditorViewManager::splitActiveHorizontal()
{
    splitActiveView(Qt::Horizontal);
}

void EditorViewManager::splitActiveVertical()
{
    splitActiveView(Qt::Vertical);
}

void EditorViewManager::splitActiveView(Qt::Orientation orientation)
{
    if (!m_activeViewSpace) return;

    auto *newSpace = createViewSpace();

    // Clone the current note into the new pane
    auto *editor = m_activeViewSpace->activeEditor();
    if (editor && editor->noteDocument()) {
        newSpace->openNote(editor->noteDocument());
    }

    // Find the parent splitter of the active view space
    auto *parentSplitter = qobject_cast<QSplitter *>(m_activeViewSpace->parentWidget());
    if (!parentSplitter) parentSplitter = m_rootSplitter;

    if (parentSplitter->orientation() == orientation) {
        // Same orientation — just insert after active
        int idx = parentSplitter->indexOf(m_activeViewSpace);
        parentSplitter->insertWidget(idx + 1, newSpace);
    } else {
        // Different orientation — wrap in a new splitter
        int idx = parentSplitter->indexOf(m_activeViewSpace);

        auto *newSplitter = new QSplitter(orientation, this);
        parentSplitter->insertWidget(idx, newSplitter);
        newSplitter->addWidget(m_activeViewSpace);
        newSplitter->addWidget(newSpace);

        // Equal sizes
        QList<int> sizes;
        int total = (orientation == Qt::Horizontal)
            ? newSplitter->width() : newSplitter->height();
        sizes << total / 2 << total / 2;
        newSplitter->setSizes(sizes);
    }

    setActiveViewSpace(newSpace);
}

void EditorViewManager::closeActiveViewSpace()
{
    if (m_viewSpaces.size() <= 1) return;
    if (!m_activeViewSpace) return;

    auto *toRemove = m_activeViewSpace;

    // Find a sibling to activate
    int idx = m_viewSpaces.indexOf(toRemove);
    int newIdx = (idx > 0) ? idx - 1 : 1;
    setActiveViewSpace(m_viewSpaces.at(newIdx));
    Q_EMIT activeEditorChanged(m_activeViewSpace->activeEditor());

    removeViewSpace(toRemove);
}

void EditorViewManager::removeViewSpace(EditorViewSpace *space)
{
    auto *parentSplitter = qobject_cast<QSplitter *>(space->parentWidget());

    m_viewSpaces.removeOne(space);
    space->deleteLater();

    // Clean up empty or single-child splitters
    if (parentSplitter && parentSplitter != m_rootSplitter) {
        cleanupEmptySplitters(parentSplitter);
    }
}

void EditorViewManager::cleanupEmptySplitters(QSplitter *splitter)
{
    if (!splitter || splitter == m_rootSplitter) return;

    if (splitter->count() <= 1) {
        auto *parent = qobject_cast<QSplitter *>(splitter->parentWidget());
        if (!parent) return;

        // If one child left, unwrap it into the parent
        if (splitter->count() == 1) {
            QWidget *child = splitter->widget(0);
            int idx = parent->indexOf(splitter);
            parent->insertWidget(idx, child);
        }

        splitter->deleteLater();

        // Recurse in case parent is now single-child too
        if (parent != m_rootSplitter) {
            cleanupEmptySplitters(parent);
        }
    }
}

bool EditorViewManager::queryClose()
{
    QStringList modifiedPaths;
    for (auto *space : std::as_const(m_viewSpaces)) {
        modifiedPaths.append(space->modifiedDocumentPaths());
    }
    modifiedPaths.removeDuplicates();

    if (modifiedPaths.isEmpty()) {
        return true;
    }

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
        for (auto *space : std::as_const(m_viewSpaces)) {
            space->saveAllModified();
        }
    }

    return true;
}

void EditorViewManager::closeAllDocuments()
{
    for (auto *space : std::as_const(m_viewSpaces)) {
        space->closeAllTabs();
    }

    resetToSingleViewSpace();
}

void EditorViewManager::resetToSingleViewSpace()
{
    if (m_viewSpaces.size() <= 1) return;

    auto *keepSpace = m_viewSpaces.first();

    // Remove all other view spaces
    for (int i = m_viewSpaces.size() - 1; i >= 1; --i) {
        auto *space = m_viewSpaces.at(i);
        m_viewSpaces.removeAt(i);
        space->deleteLater();
    }

    // Remove all intermediate splitters — reparent keepSpace directly under root
    QList<QSplitter *> childSplitters;
    for (int i = m_rootSplitter->count() - 1; i >= 0; --i) {
        auto *child = qobject_cast<QSplitter *>(m_rootSplitter->widget(i));
        if (child) {
            childSplitters.append(child);
        }
    }

    m_rootSplitter->addWidget(keepSpace);

    for (auto *s : std::as_const(childSplitters)) {
        s->deleteLater();
    }

    setActiveViewSpace(keepSpace);
    Q_EMIT activeEditorChanged(keepSpace->activeEditor());
}

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
    QJsonObject state;

    // Build per-pane tab data
    QJsonArray panes;
    for (auto *space : std::as_const(m_viewSpaces)) {
        QJsonObject paneJson;
        QJsonArray tabs;

        auto *tabBar = space->findChild<QTabBar *>();
        if (tabBar) {
            for (int i = 0; i < tabBar->count(); ++i) {
                QJsonObject tabObj;
                QString path = tabBar->tabData(i).toString();
                tabObj[QStringLiteral("path")] = path;

                // Determine tab type
                if (path == QStringLiteral("__graph__")) {
                    tabObj[QStringLiteral("type")] = QStringLiteral("graph");
                } else if (path.endsWith(QStringLiteral(".canvas"))) {
                    tabObj[QStringLiteral("type")] = QStringLiteral("canvas");
                } else {
                    tabObj[QStringLiteral("type")] = QStringLiteral("note");

                    // Save cursor position for note tabs
                    auto *editor = space->activeEditor();
                    if (editor && editor->noteDocument()
                        && editor->noteDocument()->relativePath() == path) {
                        tabObj[QStringLiteral("cursorLine")] = editor->currentLine();
                        tabObj[QStringLiteral("cursorColumn")] = editor->currentColumn();
                        tabObj[QStringLiteral("scrollPosition")] = editor->editor()->verticalScrollBar()
                            ? editor->editor()->verticalScrollBar()->value() : 0;
                    }

                    // Save view mode for the active tab
                    if (tabBar->currentIndex() == i) {
                        auto *ed = space->activeEditor();
                        if (ed) {
                            auto vm = ed->viewMode();
                            if (vm == NoteEditorWidget::ViewMode::Reading)
                                tabObj[QStringLiteral("viewMode")] = QStringLiteral("reading");
                        }
                    }
                }

                tabs.append(tabObj);
            }
            paneJson[QStringLiteral("activeTab")] = tabBar->currentIndex();
        }

        paneJson[QStringLiteral("tabs")] = tabs;
        paneJson[QStringLiteral("isActive")] = (space == m_activeViewSpace);
        panes.append(paneJson);
    }
    state[QStringLiteral("panes")] = panes;

    // Build split layout tree
    QJsonValue layoutTree = SessionManager::encodeSplitterNode(m_rootSplitter, m_viewSpaces);
    state[QStringLiteral("splitLayout")] = layoutTree;

    return state;
}

void EditorViewManager::restoreFromSession(
    const QJsonObject &editorState,
    std::function<void(const QString &path, EditorViewSpace *space)> openTabCallback)
{
    auto panesArray = editorState[QStringLiteral("panes")].toArray();
    if (panesArray.isEmpty()) return;

    // Close all existing state first
    closeAllDocuments();

    // If there is a split layout, rebuild it; otherwise just use the single pane
    auto splitLayout = editorState[QStringLiteral("splitLayout")];

    if (panesArray.size() > 1 && !splitLayout.isUndefined()) {
        // Remove the default single view space from the root splitter
        auto *defaultSpace = m_viewSpaces.first();
        m_viewSpaces.clear();

        // Rebuild the split layout tree, which creates new view spaces
        rebuildSplitLayout(splitLayout, m_rootSplitter);

        // Delete the old default space now that new ones are in place
        defaultSpace->deleteLater();
    }

    // Open tabs in each pane
    int activePaneIdx = -1;
    for (int paneIdx = 0; paneIdx < panesArray.size() && paneIdx < m_viewSpaces.size(); ++paneIdx) {
        auto paneJson = panesArray[paneIdx].toObject();
        auto *space = m_viewSpaces[paneIdx];
        auto tabsArray = paneJson[QStringLiteral("tabs")].toArray();

        for (const auto &tabVal : tabsArray) {
            auto tabObj = tabVal.toObject();
            QString path = tabObj[QStringLiteral("path")].toString();
            QString type = tabObj[QStringLiteral("type")].toString();
            if (path.isEmpty()) continue;

            // Skip graph tabs — they require live index/vault references
            if (type == QStringLiteral("graph")) continue;

            openTabCallback(path, space);
        }

        // Restore active tab index
        int activeTab = paneJson[QStringLiteral("activeTab")].toInt(0);
        auto *tabBar = space->findChild<QTabBar *>();
        if (tabBar && activeTab >= 0 && activeTab < tabBar->count()) {
            tabBar->setCurrentIndex(activeTab);
        }

        // Restore reading mode and cursor for the active tab
        restoreTabState(paneJson, space);

        if (paneJson[QStringLiteral("isActive")].toBool()) {
            activePaneIdx = paneIdx;
        }
    }

    // Set the active view space
    if (activePaneIdx >= 0 && activePaneIdx < m_viewSpaces.size()) {
        setActiveViewSpace(m_viewSpaces[activePaneIdx]);
        Q_EMIT activeEditorChanged(m_viewSpaces[activePaneIdx]->activeEditor());
    } else if (!m_viewSpaces.isEmpty()) {
        setActiveViewSpace(m_viewSpaces.first());
        Q_EMIT activeEditorChanged(m_viewSpaces.first()->activeEditor());
    }
}

void EditorViewManager::rebuildSplitLayout(const QJsonValue &node, QSplitter *parent)
{
    if (node.isString()) {
        // Leaf: "pane:N" — create a new view space
        auto *space = createViewSpace();
        parent->addWidget(space);
        return;
    }

    if (!node.isObject()) return;

    auto obj = node.toObject();
    QString orientation = obj[QStringLiteral("orientation")].toString();
    auto children = obj[QStringLiteral("children")].toArray();
    auto sizesArray = obj[QStringLiteral("sizes")].toArray();

    // Create a sub-splitter for this node
    Qt::Orientation orient = (orientation == QStringLiteral("vertical"))
        ? Qt::Vertical : Qt::Horizontal;

    // If parent has the same orientation and is empty, reuse it
    QSplitter *splitter;
    if (parent->count() == 0 && parent->orientation() == orient) {
        splitter = parent;
    } else {
        splitter = new QSplitter(orient, this);
        parent->addWidget(splitter);
    }

    for (const auto &child : children) {
        rebuildSplitLayout(child, splitter);
    }

    // Restore sizes
    if (!sizesArray.isEmpty()) {
        QList<int> sizes;
        for (const auto &s : sizesArray) {
            sizes.append(s.toInt());
        }
        if (sizes.size() == splitter->count()) {
            splitter->setSizes(sizes);
        }
    }
}

void EditorViewManager::restoreTabState(const QJsonObject &paneJson, EditorViewSpace *space)
{
    auto tabsArray = paneJson[QStringLiteral("tabs")].toArray();
    int activeTab = paneJson[QStringLiteral("activeTab")].toInt(0);

    if (activeTab < 0 || activeTab >= tabsArray.size()) return;

    auto tabObj = tabsArray[activeTab].toObject();
    QString type = tabObj[QStringLiteral("type")].toString();

    if (type != QStringLiteral("note")) return;

    // Restore view mode (backwards compatible with old readingMode key)
    QString savedMode = tabObj[QStringLiteral("viewMode")].toString();
    if (savedMode == QStringLiteral("livePreview")) {
        space->setViewMode(NoteEditorWidget::ViewMode::Editing);
    } else if (savedMode == QStringLiteral("reading")
               || tabObj[QStringLiteral("readingMode")].toBool()) {
        space->setViewMode(NoteEditorWidget::ViewMode::Reading);
    }

    // Restore cursor position
    auto *editor = space->activeEditor();
    if (editor) {
        int line = tabObj[QStringLiteral("cursorLine")].toInt(1);
        int column = tabObj[QStringLiteral("cursorColumn")].toInt(1);
        int scroll = tabObj[QStringLiteral("scrollPosition")].toInt(0);

        editor->editor()->goToLine(line);

        if (editor->editor()->verticalScrollBar() && scroll > 0) {
            editor->editor()->verticalScrollBar()->setValue(scroll);
        }
    }
}

} // namespace Corbomite
