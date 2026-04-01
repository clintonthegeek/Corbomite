// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorViewSpace.h"
#include "NoteEditorWidget.h"
#include "NotePreviewWidget.h"
#include "graph/GraphViewTab.h"
#include "canvas/CanvasViewTab.h"
#include "corbomite/core/NoteDocument.h"
#include <QVBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QMenu>
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

void EditorViewSpace::setRenderEngine(MarkdownRenderEngine *engine)
{
    m_engine = engine;

    // Update any existing preview widgets
    for (auto *preview : std::as_const(m_previews)) {
        preview->setRenderEngine(engine);
    }
}

void EditorViewSpace::openNote(NoteDocument *doc)
{
    if (!doc) return;

    const QString &path = doc->relativePath();

    // If already open, activate that tab
    if (m_editors.contains(path)) {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            if (m_tabBar->tabData(i).toString() == path) {
                m_tabBar->setCurrentIndex(i);
                return;
            }
        }
    }

    // Create editor for this note
    auto *editor = new NoteEditorWidget(m_stack);
    editor->setNoteDocument(doc);
    m_editors.insert(path, editor);

    int stackIdx = m_stack->addWidget(editor);
    Q_UNUSED(stackIdx);
    int tabIdx = m_tabBar->addTab(doc->name());
    m_tabBar->setTabData(tabIdx, path);
    m_tabBar->setCurrentIndex(tabIdx);
    m_tabModel.openTab(path);

    // Connect dirty state to tab visual
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

void EditorViewSpace::openCanvas(const QString &filePath)
{
    // Check if already open
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toString() == filePath) {
            m_tabBar->setCurrentIndex(i);
            return;
        }
    }

    auto *canvasTab = new CanvasViewTab(filePath, m_stack);
    m_stack->addWidget(canvasTab);

    // Extract filename for tab title
    QString name = filePath.mid(filePath.lastIndexOf(QLatin1Char('/')) + 1);
    if (name.endsWith(QStringLiteral(".canvas"))) name.chop(7);

    int tabIdx = m_tabBar->addTab(QIcon::fromTheme(QStringLiteral("draw-rectangle")), name);
    m_tabBar->setTabData(tabIdx, filePath);
    m_tabBar->setCurrentIndex(tabIdx);

    // Track modification state
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

void EditorViewSpace::closeTab(int index)
{
    if (index < 0 || index >= m_tabBar->count()) return;

    QString path = m_tabBar->tabData(index).toString();
    m_tabBar->removeTab(index);

    // Check if this is a graph tab
    if (path == QStringLiteral("__graph__")) {
        // Find and remove the GraphViewTab widget from the stack
        for (int i = 0; i < m_stack->count(); ++i) {
            if (auto *graph = qobject_cast<GraphViewTab *>(m_stack->widget(i))) {
                m_stack->removeWidget(graph);
                graph->deleteLater();
                break;
            }
        }
        return;
    }

    // Check if this is a canvas tab
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

    // Clean up preview widget if any
    if (auto *preview = m_previews.take(path)) {
        m_stack->removeWidget(preview);
        preview->deleteLater();
    }
    m_previewModePaths.remove(path);

    // Find matching index in TabModel using the public tabPath() API
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

    QString path = m_tabBar->tabData(index).toString();

    // Handle graph tab
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

    // Handle canvas tab
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

    if (m_previewModePaths.contains(path)) {
        // Tab is in preview mode — show preview widget
        if (auto *preview = m_previews.value(path)) {
            m_stack->setCurrentWidget(preview);
            m_tabModel.setActiveTab(index);
            Q_EMIT activeEditorChanged(nullptr);
            return;
        }
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

void EditorViewSpace::toggleEditorMode()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0) return;
    QString path = m_tabBar->tabData(idx).toString();

    if (m_previewModePaths.contains(path)) {
        // Switch from preview to editor
        m_previewModePaths.remove(path);
        if (auto *editor = m_editors.value(path)) {
            m_stack->setCurrentWidget(editor);
        }
        Q_EMIT activeEditorChanged(m_editors.value(path));
    } else {
        // Switch from editor to preview
        m_previewModePaths.insert(path);
        auto *editor = m_editors.value(path);
        if (!editor || !editor->noteDocument()) return;

        auto *preview = m_previews.value(path);
        if (!preview) {
            preview = new NotePreviewWidget(m_stack);
            preview->setRenderEngine(m_engine);
            m_previews.insert(path, preview);
            m_stack->addWidget(preview);

            // Forward internal link clicks
            connect(preview, &NotePreviewWidget::internalLinkClicked,
                    this, &EditorViewSpace::internalLinkClicked);
        }

        preview->renderDocument(editor->noteDocument());
        m_stack->setCurrentWidget(preview);
        Q_EMIT activeEditorChanged(nullptr); // no active editor in preview mode
    }
}

bool EditorViewSpace::isPreviewMode() const
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0) return false;
    QString path = m_tabBar->tabData(idx).toString();
    return m_previewModePaths.contains(path);
}

void EditorViewSpace::openGraphView(SQLiteIndex *index, VaultModel *vault)
{
    // Only one graph tab allowed
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

} // namespace Corbomite
