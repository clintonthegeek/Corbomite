// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorViewSpace.h"
#include "HoverPopover.h"
#include "NoteEditorWidget.h"
#include "graph/GraphViewTab.h"
#include "canvas/CanvasViewTab.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/FileView.h"
#include "corbomite/core/View.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include <QVBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QMenu>
#include <QFileInfo>
#include <QJsonObject>
#include <KLocalizedString>

namespace Corbomite {

EditorViewSpace::EditorViewSpace(QWidget *parent)
    : QWidget(parent)
    , m_tabBar(new QTabBar(this))
    , m_stack(new QStackedWidget(this))
{
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setDocumentMode(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabBar);
    layout->addWidget(m_stack);

    m_tabBar->installEventFilter(this);
    m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_tabBar, &QTabBar::currentChanged, this, &EditorViewSpace::onTabChanged);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &EditorViewSpace::onTabCloseRequested);
    connect(m_tabBar, &QTabBar::customContextMenuRequested, this, &EditorViewSpace::showTabContextMenu);
}

void EditorViewSpace::setCanvasEngine(MarkdownRenderEngine *engine)
{
    m_canvasEngine = engine;
}

void EditorViewSpace::setHoverPopover(HoverPopover *popover)
{
    m_hoverPopover = popover;
    for (auto *editor : std::as_const(m_editors)) {
        editor->setHoverPopover(popover);
    }
}

void EditorViewSpace::setEditorSuggestManager(EditorSuggestManager *manager)
{
    m_suggestManager = manager;
    for (auto *editor : std::as_const(m_editors)) {
        editor->setEditorSuggestManager(manager);
    }
}

void EditorViewSpace::setViewRegistry(ViewRegistry *registry)
{
    m_viewRegistry = registry;
}

WorkspaceLeaf *EditorViewSpace::openFile(const QString &relativePath)
{
    // Check for existing leaf with this file
    for (auto *leaf : std::as_const(m_leaves)) {
        if (auto *fv = qobject_cast<FileView *>(leaf->view())) {
            if (fv->file() && fv->file()->relativePath() == relativePath) {
                // Find tab index for this leaf and switch to it
                int leafIdx = m_leaves.indexOf(leaf);
                // Leaves are appended to tabs after any legacy tabs, so the
                // tab index is offset by the number of non-leaf tabs.
                int offset = m_tabBar->count() - m_leaves.size();
                int tabIdx = leafIdx + offset;
                if (tabIdx >= 0 && tabIdx < m_tabBar->count())
                    m_tabBar->setCurrentIndex(tabIdx);
                return leaf;
            }
        }
    }

    // Determine type from extension
    if (!m_viewRegistry) return nullptr;
    QString ext = QFileInfo(relativePath).suffix().toLower();
    QString type = m_viewRegistry->getTypeByExtension(ext);
    if (type.isEmpty()) return nullptr;

    QJsonObject state;
    state[QStringLiteral("file")] = relativePath;
    return openView(type, state);
}

WorkspaceLeaf *EditorViewSpace::openView(const QString &type, const QJsonObject &state)
{
    if (!m_viewRegistry) return nullptr;
    auto factory = m_viewRegistry->getViewCreatorByType(type);
    if (!factory) return nullptr;

    auto *leaf = new WorkspaceLeaf(m_viewRegistry, m_stack);
    auto *view = factory(leaf);
    leaf->open(view);

    if (!state.isEmpty())
        view->setState(state);

    m_leaves.append(leaf);
    m_stack->addWidget(leaf);

    QString title = view->getDisplayText();
    int tabIdx = m_tabBar->addTab(title);
    m_tabBar->setCurrentIndex(tabIdx);

    return leaf;
}

WorkspaceLeaf *EditorViewSpace::activeLeaf() const
{
    auto *current = m_stack->currentWidget();
    for (auto *leaf : m_leaves) {
        if (leaf == current)
            return leaf;
    }
    return nullptr;
}

WorkspaceLeaf *EditorViewSpace::leafForPath(const QString &relativePath) const
{
    for (auto *leaf : m_leaves) {
        if (auto *fv = qobject_cast<FileView *>(leaf->view())) {
            if (fv->file() && fv->file()->relativePath() == relativePath)
                return leaf;
        }
    }
    return nullptr;
}

QVector<WorkspaceLeaf *> EditorViewSpace::leaves() const
{
    return m_leaves;
}

void EditorViewSpace::openNote(NoteDocument *doc)
{
    if (!doc) return;

    const QString &path = doc->relativePath();

    if (m_editors.contains(path)) {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            if (m_tabBar->tabData(i).toString() == path) {
                m_tabBar->setCurrentIndex(i);
                return;
            }
        }
    }

    auto *editor = new NoteEditorWidget(m_stack);
    editor->setNoteDocument(doc);
    editor->setHoverPopover(m_hoverPopover);
    editor->setEditorSuggestManager(m_suggestManager);
    m_editors.insert(path, editor);

    int stackIdx = m_stack->addWidget(editor);
    Q_UNUSED(stackIdx);
    int tabIdx = m_tabBar->addTab(doc->name());
    m_tabBar->setTabData(tabIdx, path);
    m_tabBar->setCurrentIndex(tabIdx);
    m_tabModel.openTab(path);

    connect(doc, &NoteDocument::modificationChanged, this, [this, path](bool modified) {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            if (m_tabBar->tabData(i).toString() == path) {
                QString title = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
                int dot = title.lastIndexOf(QLatin1Char('.'));
                if (dot > 0) title = title.left(dot);
                if (modified) title += QStringLiteral(" \u2022");
                m_tabBar->setTabText(i, title);
                break;
            }
        }
    });

    connect(editor, &NoteEditorWidget::cursorInfoChanged,
            this, &EditorViewSpace::cursorInfoChanged);
}

void EditorViewSpace::setViewMode(NoteEditorWidget::ViewMode mode)
{
    if (auto *editor = activeEditor())
        editor->setViewMode(mode);
}

NoteEditorWidget::ViewMode EditorViewSpace::viewMode() const
{
    if (auto *editor = activeEditor())
        return editor->viewMode();
    return NoteEditorWidget::ViewMode::LivePreview;
}

void EditorViewSpace::closeTab(int index)
{
    if (index < 0 || index >= m_tabBar->count()) return;

    QString path = m_tabBar->tabData(index).toString();

    // Check if the widget at this stack index is a WorkspaceLeaf.
    // Leaf-based tabs don't set tabData, so path will be empty for them.
    // We also check by matching the stack widget directly.
    // Leaf tabs are appended after legacy tabs, so the leaf index is
    // offset from the tab index.
    int legacyCount = m_tabBar->count() - m_leaves.size();
    if (index >= legacyCount && !m_leaves.isEmpty()) {
        int leafIdx = index - legacyCount;
        if (leafIdx >= 0 && leafIdx < m_leaves.size()) {
            auto *leaf = m_leaves.takeAt(leafIdx);
            m_tabBar->removeTab(index);
            m_stack->removeWidget(leaf);
            leaf->deleteLater();
            return;
        }
    }

    m_tabBar->removeTab(index);

    if (path == QStringLiteral("__graph__")) {
        for (int i = 0; i < m_stack->count(); ++i) {
            if (auto *graph = qobject_cast<GraphViewTab *>(m_stack->widget(i))) {
                m_stack->removeWidget(graph);
                graph->deleteLater();
                break;
            }
        }
        return;
    }

    if (path.endsWith(QStringLiteral(".canvas"))) {
        for (int i = 0; i < m_stack->count(); ++i) {
            if (auto *canvas = qobject_cast<CanvasViewTab *>(m_stack->widget(i))) {
                if (canvas->filePath() == path) {
                    m_stack->removeWidget(canvas);
                    canvas->deleteLater();
                    break;
                }
            }
        }
        return;
    }

    if (auto *editor = m_editors.take(path)) {
        m_stack->removeWidget(editor);
        editor->deleteLater();
    }

    for (int i = 0; i < m_tabModel.rowCount(); ++i) {
        if (m_tabModel.tabPath(i) == path) {
            m_tabModel.closeTab(i);
            break;
        }
    }
}

NoteEditorWidget *EditorViewSpace::activeEditor() const
{
    return qobject_cast<NoteEditorWidget *>(m_stack->currentWidget());
}

TabModel *EditorViewSpace::tabModel()
{
    return &m_tabModel;
}

void EditorViewSpace::onTabChanged(int index)
{
    if (index < 0 || index >= m_tabBar->count()) {
        Q_EMIT activeEditorChanged(nullptr);
        return;
    }

    // Check if this is a leaf-based tab
    int legacyCount = m_tabBar->count() - m_leaves.size();
    if (index >= legacyCount && !m_leaves.isEmpty()) {
        int leafIdx = index - legacyCount;
        if (leafIdx >= 0 && leafIdx < m_leaves.size()) {
            m_stack->setCurrentWidget(m_leaves[leafIdx]);
            Q_EMIT activeEditorChanged(nullptr);
            return;
        }
    }

    QString path = m_tabBar->tabData(index).toString();

    if (path == QStringLiteral("__graph__")) {
        for (int i = 0; i < m_stack->count(); ++i) {
            if (auto *graph = qobject_cast<GraphViewTab *>(m_stack->widget(i))) {
                m_stack->setCurrentWidget(graph);
                break;
            }
        }
        Q_EMIT activeEditorChanged(nullptr);
        return;
    }

    if (path.endsWith(QStringLiteral(".canvas"))) {
        for (int i = 0; i < m_stack->count(); ++i) {
            if (auto *canvas = qobject_cast<CanvasViewTab *>(m_stack->widget(i))) {
                if (canvas->filePath() == path) {
                    m_stack->setCurrentWidget(canvas);
                    break;
                }
            }
        }
        Q_EMIT activeEditorChanged(nullptr);
        return;
    }

    if (auto *editor = m_editors.value(path)) {
        m_stack->setCurrentWidget(editor);
        m_tabModel.setActiveTab(index);
        Q_EMIT activeEditorChanged(editor);
    }
}

void EditorViewSpace::onTabCloseRequested(int index)
{
    closeTab(index);
}

void EditorViewSpace::openCanvas(const QString &filePath)
{
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toString() == filePath) {
            m_tabBar->setCurrentIndex(i);
            return;
        }
    }

    auto *canvasTab = new CanvasViewTab(filePath, m_stack);
    if (m_canvasEngine) {
        canvasTab->setRenderEngine(m_canvasEngine);
    }
    m_stack->addWidget(canvasTab);

    QString name = filePath.mid(filePath.lastIndexOf(QLatin1Char('/')) + 1);
    if (name.endsWith(QStringLiteral(".canvas"))) name.chop(7);

    int tabIdx = m_tabBar->addTab(QIcon::fromTheme(QStringLiteral("draw-rectangle")), name);
    m_tabBar->setTabData(tabIdx, filePath);
    m_tabBar->setCurrentIndex(tabIdx);

    connect(canvasTab, &CanvasViewTab::modificationChanged, this, [this, filePath](bool modified) {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            if (m_tabBar->tabData(i).toString() == filePath) {
                QString title = filePath.mid(filePath.lastIndexOf(QLatin1Char('/')) + 1);
                if (title.endsWith(QStringLiteral(".canvas"))) title.chop(7);
                if (modified) title += QStringLiteral(" \u2022");
                m_tabBar->setTabText(i, title);
                break;
            }
        }
    });
}

void EditorViewSpace::openGraphView(SQLiteIndex *index, VaultModel *vault)
{
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toString() == QStringLiteral("__graph__")) {
            m_tabBar->setCurrentIndex(i);
            return;
        }
    }

    auto *graphTab = new GraphViewTab(index, vault, m_stack);
    m_stack->addWidget(graphTab);
    int tabIdx = m_tabBar->addTab(QIcon::fromTheme(QStringLiteral("preferences-system-network")),
                                   QStringLiteral("Graph View"));
    m_tabBar->setTabData(tabIdx, QStringLiteral("__graph__"));
    m_tabBar->setCurrentIndex(tabIdx);

    connect(graphTab, &GraphViewTab::noteActivated,
            this, &EditorViewSpace::graphNoteActivated);
    connect(graphTab, &GraphViewTab::openNoteInNewTabRequested,
            this, [this](const QString &path) {
        Q_EMIT graphNoteActivated(path);
    });
    connect(graphTab, &GraphViewTab::revealInNavigationRequested,
            this, [this](const QString &path) {
        Q_EMIT graphNoteActivated(path);
    });
    connect(graphTab, &GraphViewTab::deleteNoteRequested,
            this, [this](const QString &path) {
        Q_UNUSED(path);
    });
}

bool EditorViewSpace::hasGraphView() const
{
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toString() == QStringLiteral("__graph__"))
            return true;
    }
    return false;
}

bool EditorViewSpace::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_tabBar && event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::MiddleButton) {
            int tabIndex = m_tabBar->tabAt(mouseEvent->pos());
            if (tabIndex >= 0) {
                closeTab(tabIndex);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void EditorViewSpace::showTabContextMenu(const QPoint &pos)
{
    int tabIndex = m_tabBar->tabAt(pos);
    if (tabIndex < 0) return;

    QMenu menu(this);

    auto *closeAction = menu.addAction(i18n("Close"));
    connect(closeAction, &QAction::triggered, this, [this, tabIndex]() {
        closeTab(tabIndex);
    });

    auto *closeOthers = menu.addAction(i18n("Close Others"));
    connect(closeOthers, &QAction::triggered, this, [this, tabIndex]() {
        for (int i = m_tabBar->count() - 1; i >= 0; --i) {
            if (i != tabIndex) closeTab(i);
        }
    });

    auto *closeAll = menu.addAction(i18n("Close All"));
    connect(closeAll, &QAction::triggered, this, [this]() {
        while (m_tabBar->count() > 0) closeTab(0);
    });

    menu.addSeparator();

    auto *splitRight = menu.addAction(i18n("Split Right"));
    connect(splitRight, &QAction::triggered, this, [this]() {
        Q_EMIT splitRightRequested();
    });

    auto *splitDown = menu.addAction(i18n("Split Down"));
    connect(splitDown, &QAction::triggered, this, [this]() {
        Q_EMIT splitDownRequested();
    });

    menu.exec(m_tabBar->mapToGlobal(pos));
}

void EditorViewSpace::closeAllTabs()
{
    // Clean up leaf-based tabs
    for (auto *leaf : std::as_const(m_leaves)) {
        m_stack->removeWidget(leaf);
        leaf->deleteLater();
    }
    m_leaves.clear();

    // Clean up legacy tabs
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

void EditorViewSpace::saveAllModified()
{
    FileSystemAdapter fs;
    for (auto it = m_editors.constBegin(); it != m_editors.constEnd(); ++it) {
        auto *editor = it.value();
        if (editor && editor->noteDocument() && editor->noteDocument()->isModified()) {
            auto *doc = editor->noteDocument();
            if (fs.writeFile(doc->filePath(), doc->markdown())) {
                doc->setModified(false);
                Q_EMIT doc->saved();
            }
        }
    }
}

} // namespace Corbomite
