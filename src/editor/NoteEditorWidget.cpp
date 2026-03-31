// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteEditorWidget.h"
#include "corbomite/core/NoteDocument.h"
#include <QTextCursor>

namespace Corbomite {

NoteEditorWidget::NoteEditorWidget(QWidget *parent)
    : QMarkdownTextEdit(parent)
{
    connect(this, &QPlainTextEdit::textChanged, this, &NoteEditorWidget::onTextChanged);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &NoteEditorWidget::onCursorPositionChanged);
}

void NoteEditorWidget::setNoteDocument(NoteDocument *doc)
{
    m_doc = doc;
    if (m_doc) {
        syncFromDocument();
    } else {
        clear();
    }
}

NoteDocument *NoteEditorWidget::noteDocument() const
{
    return m_doc;
}

int NoteEditorWidget::currentLine() const
{
    return textCursor().blockNumber() + 1;
}

int NoteEditorWidget::currentColumn() const
{
    return textCursor().columnNumber() + 1;
}

void NoteEditorWidget::onTextChanged()
{
    if (m_updatingFromDoc || !m_doc) return;
    m_doc->setMarkdown(toPlainText());
}

void NoteEditorWidget::onCursorPositionChanged()
{
    if (!m_doc) return;
    Q_EMIT cursorInfoChanged(currentLine(), currentColumn(), m_doc->wordCount());
}

void NoteEditorWidget::syncFromDocument()
{
    if (!m_doc) return;
    m_updatingFromDoc = true;
    setPlainText(m_doc->markdown());
    m_doc->setModified(false);
    m_updatingFromDoc = false;
}

} // namespace Corbomite
