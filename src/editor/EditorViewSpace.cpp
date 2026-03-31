// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorViewSpace.h"
#include "NoteEditorWidget.h"
#include "corbomite/core/NoteDocument.h"
#include <QVBoxLayout>

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

    if (auto *editor = m_editors.take(path)) {
        m_stack->removeWidget(editor);
        editor->deleteLater();
    }

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

} // namespace Corbomite
