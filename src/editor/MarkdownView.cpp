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

void MarkdownView::insertFrontmatterProperty()
{
    if (!m_editorWidget) return;

    // If currently in Reading, flip to LivePreview so the new row is visible.
    if (m_editorWidget->viewMode() == NoteEditorWidget::ViewMode::Reading)
        m_editorWidget->setViewMode(NoteEditorWidget::ViewMode::LivePreview);

    auto *doc = m_editorWidget->noteDocument();
    if (!doc) return;

    QString body = doc->markdown();

    // Obsidian's convention: a frontmatter block opens with `---` on the
    // very first line and closes with `---` on its own line. If the opening
    // fence is missing, prepend a minimal 3-line block with one empty row.
    if (!body.startsWith(QStringLiteral("---\n"))
        && !body.startsWith(QStringLiteral("---\r\n"))
        && body != QStringLiteral("---")) {
        // Prepend a new block containing one blank key.
        const QString fm = QStringLiteral("---\n: \n---\n");
        doc->setMarkdown(fm + body);
        return;
    }

    // Locate the closing fence and append a blank key before it.
    const int closeIdx = body.indexOf(QStringLiteral("\n---"), /*from=*/3);
    if (closeIdx < 0) {
        // Opening fence present but no closing fence — treat as malformed;
        // append a fresh block at the top.
        const QString fm = QStringLiteral("---\n: \n---\n");
        doc->setMarkdown(fm + body);
        return;
    }

    // Insert a blank property row directly before the closing `---`.
    // Preserve a trailing newline if the closing fence is the last line.
    const QString insert = QStringLiteral(": \n");
    QString out = body;
    out.insert(closeIdx + 1, insert);  // +1 skips the leading '\n'
    doc->setMarkdown(out);
}

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
