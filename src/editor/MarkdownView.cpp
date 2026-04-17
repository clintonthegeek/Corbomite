// src/editor/MarkdownView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "MarkdownView.h"
#include "NoteEditorWidget.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/WorkspaceLeaf.h"

#include <QVBoxLayout>

namespace Corbomite {

MarkdownView::MarkdownView(WorkspaceLeaf *leaf, QWidget *parent)
    : TextFileView(leaf, parent)
    , m_editorWidget(new NoteEditorWidget(contentWidget()))
{
    auto *layout = new QVBoxLayout(contentWidget());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_editorWidget);
}

View *MarkdownView::factory(WorkspaceLeaf *leaf)
{
    return new MarkdownView(leaf);
}

QString MarkdownView::getViewType() const
{
    return QStringLiteral("markdown");
}

QString MarkdownView::getDisplayText() const
{
    if (m_file)
        return m_file->name();
    return QStringLiteral("Markdown");
}

QString MarkdownView::getIcon() const
{
    return QStringLiteral("text-markdown");
}

QString MarkdownView::getViewData() const
{
    if (!m_editorWidget || !m_editorWidget->noteDocument())
        return {};
    return m_editorWidget->noteDocument()->markdown();
}

void MarkdownView::setViewData(const QString &data, bool clear)
{
    if (!m_editorWidget || !m_editorWidget->noteDocument())
        return;
    m_editorWidget->noteDocument()->setMarkdown(data);
    Q_UNUSED(clear)
}

void MarkdownView::clear()
{
    if (m_editorWidget && m_editorWidget->noteDocument())
        m_editorWidget->noteDocument()->setMarkdown(QString());
}

bool MarkdownView::canAcceptExtension(const QString &ext) const
{
    return ext.compare(QStringLiteral("md"), Qt::CaseInsensitive) == 0;
}

bool MarkdownView::setCursorLine(int line)
{
    if (!m_editorWidget) return false;
    return m_editorWidget->goToLine(line);
}

QJsonObject MarkdownView::getState() const
{
    QJsonObject state = FileView::getState();
    auto mode = m_editorWidget->viewMode();
    if (mode == NoteEditorWidget::ViewMode::Reading) {
        state[QStringLiteral("mode")] = QStringLiteral("preview");
    } else {
        state[QStringLiteral("mode")] = QStringLiteral("source");
        state[QStringLiteral("source")] = (mode == NoteEditorWidget::ViewMode::Source);
    }
    return state;
}

void MarkdownView::setState(const QJsonObject &state)
{
    FileView::setState(state);
    QString mode = state[QStringLiteral("mode")].toString();
    if (mode == QStringLiteral("preview")) {
        m_editorWidget->setViewMode(NoteEditorWidget::ViewMode::Reading);
    } else if (mode == QStringLiteral("source")) {
        bool source = state[QStringLiteral("source")].toBool(false);
        m_editorWidget->setViewMode(source ? NoteEditorWidget::ViewMode::Source
                                           : NoteEditorWidget::ViewMode::LivePreview);
    }
}

QJsonObject MarkdownView::getEphemeralState() const
{
    // Delegate to EphemeralState serialization established in Cluster E
    return {};
}

void MarkdownView::setEphemeralState(const QJsonObject &state)
{
    Q_UNUSED(state)
}

NoteEditorWidget *MarkdownView::editorWidget() const { return m_editorWidget; }

void MarkdownView::setVault(Vault *vault)
{
    m_editorWidget->setVault(vault);
}

void MarkdownView::setHoverPopover(HoverPopover *popover)
{
    m_editorWidget->setHoverPopover(popover);
}

void MarkdownView::setEditorSuggestManager(EditorSuggestManager *manager)
{
    m_editorWidget->setEditorSuggestManager(manager);
}

void MarkdownView::onOpen()
{
    TextFileView::onOpen();
}

void MarkdownView::onClose()
{
    TextFileView::onClose();
}

void MarkdownView::onLoadFile(NoteDocument *file)
{
    m_editorWidget->setNoteDocument(file);
    TextFileView::onLoadFile(file);
}

} // namespace Corbomite
