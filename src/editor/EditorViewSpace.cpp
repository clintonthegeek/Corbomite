// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorViewSpace.h"
#include "NoteEditorWidget.h"
#include "NotePreviewWidget.h"
#include "graph/GraphViewTab.h"
#include "corbomite/core/NoteDocument.h"
#include <QVBoxLayout>
#include <QIcon>

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

    connect(m_tabBar, &QTabBar::currentChanged, this, &EditorViewSpace::onTabChanged);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &EditorViewSpace::onTabCloseRequested);
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

} // namespace Corbomite
