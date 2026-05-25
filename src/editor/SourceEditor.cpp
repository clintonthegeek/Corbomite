// SPDX-License-Identifier: GPL-3.0-or-later
#include "SourceEditor.h"

// TODO(port-foundation-exploration): qutepart.h was the public header of the
// retired standalone Qutepart library. Stubbed to QPlainTextEdit pending the
// source-editor swap port to Markoff::Source::Editor.

#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

namespace Corbomite {

SourceEditor::SourceEditor(QWidget *parent)
    : QWidget(parent)
    , m_qutepart(new QPlainTextEdit(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_qutepart);

    // Capture the default font so `resetZoom()` can undo any accumulated
    // QPlainTextEdit::zoomIn/zoomOut offsets.
    m_defaultFont = m_qutepart->font();

    // QPlainTextEdit::textChanged → our textChanged
    connect(m_qutepart, &QPlainTextEdit::textChanged,
            this, &SourceEditor::textChanged);

    // QPlainTextEdit::cursorPositionChanged → emit our CursorPos-bearing signal
    connect(m_qutepart, &QPlainTextEdit::cursorPositionChanged,
            this, [this]() {
        Q_EMIT cursorPositionChanged(cursorPosition());
    });

    // Vertical scrollbar value change → emit visual-line float
    connect(m_qutepart->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int) {
        Q_EMIT scrollPositionChanged(scrollPosition());
    });
}

SourceEditor::~SourceEditor() = default;

void SourceEditor::setPlainText(const QString &text)
{
    m_qutepart->setPlainText(text);
}

QString SourceEditor::toPlainText() const
{
    return m_qutepart->toPlainText();
}

SourceEditor::CursorPos SourceEditor::cursorPosition() const
{
    const QTextCursor c = m_qutepart->textCursor();
    return CursorPos{c.blockNumber(), c.columnNumber()};
}

void SourceEditor::setCursorPosition(CursorPos pos)
{
    QTextBlock block = m_qutepart->document()->findBlockByNumber(pos.line);
    if (!block.isValid()) {
        return;
    }
    QTextCursor c(block);
    const int clampedCol = std::max(0, std::min(pos.column, block.length() - 1));
    if (clampedCol > 0) {
        c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, clampedCol);
    }
    m_qutepart->setTextCursor(c);
}

float SourceEditor::scrollPosition() const
{
    // TODO(port-foundation-exploration): Qutepart::Qutepart had a visual-line
    // float scroll API; QPlainTextEdit only exposes block-line scroll. Approximate
    // by returning the scrollbar value as a float — accurate enough for
    // EphemeralState round-trips during the port, off by visual-soft-wrap
    // adjustments that the real Markoff::Source::Editor will need to handle.
    return static_cast<float>(m_qutepart->verticalScrollBar()->value());
}

void SourceEditor::setScrollPosition(float visualLine)
{
    // TODO(port-foundation-exploration): see scrollPosition() — symmetric stub.
    m_qutepart->verticalScrollBar()->setValue(static_cast<int>(visualLine));
}

QVector<int> SourceEditor::foldedHeadings() const
{
    // TODO(phase-4/phase-7): derive from Qutepart's fold-block state once
    // the FoldCalculator (Phase 4) + section-fold markdown hierarchy
    // (Phase 7) land. Phase 2 scaffold: round-trip whatever was set.
    return m_pendingFoldedHeadings;
}

void SourceEditor::setFoldedHeadings(const QVector<int> &foldedLines)
{
    // TODO(phase-4/phase-7): apply to Qutepart's internal fold engine.
    // Phase 2: store for round-trip only. This keeps the EphemeralState
    // persistence plumbing in Cluster E Phase 1 unblocked without binding
    // the shim to the (about-to-be-replaced) Kate-XML fold engine.
    m_pendingFoldedHeadings = foldedLines;
}

void SourceEditor::setReadOnly(bool readOnly)
{
    m_qutepart->setReadOnly(readOnly);
}

bool SourceEditor::isReadOnly() const
{
    return m_qutepart->isReadOnly();
}

void SourceEditor::zoomIn()
{
    m_qutepart->zoomIn(1);
}

void SourceEditor::zoomOut()
{
    m_qutepart->zoomOut(1);
}

void SourceEditor::resetZoom()
{
    m_qutepart->setFont(m_defaultFont);
}

} // namespace Corbomite
