// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteEditorWidget.h"
#include "CompletionPopup.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/models/VaultModel.h"
#include "dialogs/QuickSwitcherModel.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QTextCursor>
#include <QStringListModel>
#include <QApplication>
#include <QRegularExpression>

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

void NoteEditorWidget::setVaultModel(VaultModel *vault)
{
    m_vault = vault;
}

int NoteEditorWidget::currentLine() const
{
    return textCursor().blockNumber() + 1;
}

int NoteEditorWidget::currentColumn() const
{
    return textCursor().columnNumber() + 1;
}

void NoteEditorWidget::keyPressEvent(QKeyEvent *event)
{
    // If completion popup is visible, handle navigation keys
    if (m_completionPopup) {
        switch (event->key()) {
        case Qt::Key_Down:
            m_completionPopup->selectNext();
            return;
        case Qt::Key_Up:
            m_completionPopup->selectPrevious();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
            if (m_completionPopup->hasSelection()) {
                onCompletionAccepted(m_completionPopup->selectedText(),
                                     m_completionPopup->selectedData());
                return;
            }
            break;
        case Qt::Key_Escape:
            dismissCompletion();
            return;
        default:
            break;
        }
    }

    // Let the base class handle the key first (inserts the character)
    QMarkdownTextEdit::keyPressEvent(event);

    // Check for completion triggers after the character is inserted
    if (event->text().isEmpty()) return;

    QChar typed = event->text().at(0);

    if (typed == QLatin1Char('[')) {
        // Check if we just typed the second [
        QTextCursor cursor = textCursor();
        int pos = cursor.positionInBlock();
        QString blockText = cursor.block().text();
        if (pos >= 2 && blockText.mid(pos - 2, 2) == QStringLiteral("[[")) {
            triggerWikiLinkCompletion();
            return;
        }
    }

    if (typed == QLatin1Char('#') && !m_completionPopup) {
        // Check it's not a heading (# at line start followed by space)
        QTextCursor cursor = textCursor();
        int pos = cursor.positionInBlock();
        if (pos == 1) {
            // Just # at start of line — wait for next char to determine
            // Don't trigger yet; if next char is space it's a heading
        } else if (pos > 1) {
            // # in the middle of a line — trigger tag completion
            triggerTagCompletion();
            return;
        }
    }

    // Update filter if completion is active
    if (m_completionPopup) {
        updateCompletionFilter();

        // Dismiss if user typed ]] (closed the wikilink manually)
        if (m_completionMode == CompletionMode::WikiLink && typed == QLatin1Char(']')) {
            QTextCursor cursor = textCursor();
            int pos = cursor.positionInBlock();
            QString blockText = cursor.block().text();
            if (pos >= 2 && blockText.mid(pos - 2, 2) == QStringLiteral("]]")) {
                dismissCompletion();
            }
        }

        // Dismiss tag completion on space or punctuation
        if (m_completionMode == CompletionMode::Tag &&
            (typed.isSpace() || typed == QLatin1Char(',') || typed == QLatin1Char('.'))) {
            dismissCompletion();
        }
    }
}

void NoteEditorWidget::mousePressEvent(QMouseEvent *event)
{
    // Ctrl+Click to follow wikilink
    if (event->button() == Qt::LeftButton &&
        event->modifiers() & Qt::ControlModifier) {
        QString target = wikiLinkTargetAtCursor(event->pos());
        if (!target.isEmpty()) {
            QString resolved = resolveWikiLinkTarget(target);
            Q_EMIT linkActivated(resolved);
            return;
        }
    }

    QMarkdownTextEdit::mousePressEvent(event);
}

void NoteEditorWidget::mouseMoveEvent(QMouseEvent *event)
{
    // Show pointing hand cursor when Ctrl+hovering over wikilink
    if (event->modifiers() & Qt::ControlModifier) {
        QString target = wikiLinkTargetAtCursor(event->pos());
        if (!target.isEmpty()) {
            viewport()->setCursor(Qt::PointingHandCursor);
            QMarkdownTextEdit::mouseMoveEvent(event);
            return;
        }
    }
    viewport()->setCursor(Qt::IBeamCursor);
    QMarkdownTextEdit::mouseMoveEvent(event);
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

// --- Completion ---

void NoteEditorWidget::triggerWikiLinkCompletion()
{
    if (!m_vault) return;
    dismissCompletion();

    m_completionMode = CompletionMode::WikiLink;
    m_completionTriggerPos = textCursor().position();

    // Reuse QuickSwitcherModel for note list
    auto *model = new QuickSwitcherModel(this);
    model->setNotes(m_vault->allNotes());
    // Future: match against frontmatter aliases

    m_completionPopup = new CompletionPopup(model, this);
    connect(m_completionPopup, &CompletionPopup::itemSelected,
            this, &NoteEditorWidget::onCompletionAccepted);
    connect(m_completionPopup, &CompletionPopup::dismissed,
            this, [this]() { m_completionPopup = nullptr; m_completionMode = CompletionMode::None; });

    // Position below cursor
    QRect cr = cursorRect();
    QPoint pos = mapToGlobal(QPoint(cr.left(), cr.bottom() + 2));
    m_completionPopup->move(pos);
    m_completionPopup->show();
}

void NoteEditorWidget::triggerTagCompletion()
{
    if (!m_vault) return;
    dismissCompletion();

    m_completionMode = CompletionMode::Tag;
    m_completionTriggerPos = textCursor().position();

    auto tags = m_vault->allTags();
    auto *model = new QStringListModel(tags, this);
    // Future: include tags from frontmatter properties
    // Future: show tag usage count alongside name

    m_completionPopup = new CompletionPopup(model, this);
    connect(m_completionPopup, &CompletionPopup::itemSelected,
            this, &NoteEditorWidget::onCompletionAccepted);
    connect(m_completionPopup, &CompletionPopup::dismissed,
            this, [this]() { m_completionPopup = nullptr; m_completionMode = CompletionMode::None; });

    QRect cr = cursorRect();
    QPoint pos = mapToGlobal(QPoint(cr.left(), cr.bottom() + 2));
    m_completionPopup->move(pos);
    m_completionPopup->show();
}

void NoteEditorWidget::dismissCompletion()
{
    if (m_completionPopup) {
        m_completionPopup->close();
        m_completionPopup = nullptr;
    }
    m_completionMode = CompletionMode::None;
    m_completionTriggerPos = -1;
}

void NoteEditorWidget::onCompletionAccepted(const QString &text, const QString &data)
{
    Q_UNUSED(data)

    QTextCursor cursor = textCursor();

    if (m_completionMode == CompletionMode::WikiLink) {
        // Delete text typed after [[ trigger
        int currentPos = cursor.position();
        int deleteCount = currentPos - m_completionTriggerPos;
        if (deleteCount > 0) {
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, deleteCount);
            cursor.removeSelectedText();
        }
        // Insert NoteName]] — the [[ is already typed
        // Future: respect "use markdown links" setting — insert [text](path.md) instead
        cursor.insertText(text + QStringLiteral("]]"));
    } else if (m_completionMode == CompletionMode::Tag) {
        // Delete text typed after # trigger
        int currentPos = cursor.position();
        int deleteCount = currentPos - m_completionTriggerPos;
        if (deleteCount > 0) {
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, deleteCount);
            cursor.removeSelectedText();
        }
        // Insert tag name — the # is already typed
        cursor.insertText(text);
    }

    setTextCursor(cursor);
    dismissCompletion();
}

void NoteEditorWidget::updateCompletionFilter()
{
    if (!m_completionPopup || m_completionTriggerPos < 0) return;
    QString filterText = textFromTrigger();
    m_completionPopup->setFilterText(filterText);
}

int NoteEditorWidget::completionTriggerPos() const
{
    return m_completionTriggerPos;
}

QString NoteEditorWidget::textFromTrigger() const
{
    if (m_completionTriggerPos < 0) return {};
    QTextCursor cursor = textCursor();
    int length = cursor.position() - m_completionTriggerPos;
    if (length <= 0) return {};

    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, length);
    return cursor.selectedText();
}

// --- Link Navigation ---

QString NoteEditorWidget::wikiLinkTargetAtCursor(const QPoint &pos) const
{
    QTextCursor cursor = cursorForPosition(pos);
    int posInBlock = cursor.positionInBlock();
    QString blockText = cursor.block().text();

    // Find surrounding [[ and ]]
    int openPos = blockText.lastIndexOf(QStringLiteral("[["), posInBlock);
    if (openPos == -1) return {};

    int closePos = blockText.indexOf(QStringLiteral("]]"), openPos + 2);
    if (closePos == -1 || posInBlock > closePos + 1) return {};

    // Extract content between [[ and ]]
    QString content = blockText.mid(openPos + 2, closePos - openPos - 2);
    if (content.isEmpty()) return {};

    // Handle [[target|display]] — take the target part
    int pipePos = content.indexOf(QLatin1Char('|'));
    if (pipePos >= 0) {
        content = content.left(pipePos);
    }

    // Handle [[target#heading]] — take just the note path
    // Future: scroll to heading after navigation
    int hashPos = content.indexOf(QLatin1Char('#'));
    if (hashPos >= 0) {
        content = content.left(hashPos);
    }

    return content.trimmed();
}

QString NoteEditorWidget::resolveWikiLinkTarget(const QString &rawTarget) const
{
    if (rawTarget.isEmpty()) return {};

    // If it already has an extension, use as-is
    if (rawTarget.endsWith(QStringLiteral(".md")) || rawTarget.endsWith(QStringLiteral(".canvas"))) {
        return rawTarget;
    }

    // Append .md
    return rawTarget + QStringLiteral(".md");
}

} // namespace Corbomite
