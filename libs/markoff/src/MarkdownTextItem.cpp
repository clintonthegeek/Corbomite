// SPDX-License-Identifier: GPL-3.0-or-later
#include "MarkdownTextItem.h"
#include "TextControl.h"

#include <QPainter>
#include <QTextDocument>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <QStyleOptionGraphicsItem>
#include "MarkdownHighlighter.h"
#include "SourceSpan.h"
#include <QGraphicsSceneMouseEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QSyntaxHighlighter>

namespace Markoff {

MarkdownTextItem::MarkdownTextItem(QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_document(new QTextDocument(this))
    , m_control(new TextControl(this))
{
    m_control->setDocument(m_document);
    m_control->setTextInteractionFlags(Qt::TextEditorInteraction);
    m_document->setDocumentMargin(8);

    setFlag(ItemIsFocusable);
    setFlag(ItemAcceptsInputMethod);
    setAcceptedMouseButtons(Qt::AllButtons);

    connect(m_document->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
            this, &MarkdownTextItem::updateGeometry);
    connect(m_control, &TextControl::updateRequest,
            this, [this]() { update(); });
    connect(m_control, &TextControl::textChanged,
            this, &MarkdownTextItem::textChanged);
    connect(m_control, &TextControl::cursorPositionChanged,
            this, &MarkdownTextItem::onCursorPositionChanged);
}

MarkdownTextItem::~MarkdownTextItem() = default;

void MarkdownTextItem::setPlainText(const QString &text)
{
    m_document->setPlainText(text);
}

void MarkdownTextItem::setTextWidth(qreal width)
{
    if (qFuzzyCompare(m_width, width))
        return;
    prepareGeometryChange();
    m_width = width;
    m_document->setTextWidth(width);
}

QTextDocument *MarkdownTextItem::document() const
{
    return m_document;
}

QRectF MarkdownTextItem::boundingRect() const
{
    return {0, 0, m_width, m_document->size().height()};
}

void MarkdownTextItem::paint(QPainter *painter,
                             const QStyleOptionGraphicsItem * /*option*/,
                             QWidget *widget)
{
    painter->save();
    m_control->drawContents(painter, boundingRect(), widget);
    painter->restore();
}

int MarkdownTextItem::hitTest(const QPointF &scenePos) const
{
    QPointF localPos = mapFromScene(scenePos);
    return m_document->documentLayout()->hitTest(localPos, Qt::FuzzyHit);
}

void MarkdownTextItem::setSelection(int anchorPos, int cursorPos)
{
    QTextCursor cursor(m_document);
    cursor.setPosition(anchorPos);
    cursor.setPosition(cursorPos, QTextCursor::KeepAnchor);
    m_control->setTextCursor(cursor);
}

void MarkdownTextItem::clearSelection()
{
    QTextCursor cursor = m_control->textCursor();
    cursor.clearSelection();
    m_control->setTextCursor(cursor);
}

QString MarkdownTextItem::selectedMarkdown() const
{
    QTextCursor cursor = m_control->textCursor();
    return cursor.selectedText();
}

QString MarkdownTextItem::allMarkdown() const
{
    return m_document->toPlainText();
}

QString MarkdownTextItem::toMarkdown() const
{
    return allMarkdown();
}

void MarkdownTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_control->processEvent(event);
    event->accept(); // Accept all buttons to hold grab for middle-click paste
}

void MarkdownTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    m_control->processEvent(event);
}

void MarkdownTextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    m_control->processEvent(event);
}

void MarkdownTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    m_control->processEvent(event);
}

void MarkdownTextItem::keyPressEvent(QKeyEvent *event)
{
    // Check for cursor-at-boundary before forwarding
    QTextCursor cursor = m_control->textCursor();
    bool atStart = cursor.atStart();
    bool atEnd = cursor.atEnd();

    m_control->processEvent(event);

    // If cursor didn't move for arrow keys, we're at a boundary
    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Home) {
        if (atStart && m_control->textCursor().atStart())
            emit cursorAtBoundary(Qt::TopEdge);
    } else if (event->key() == Qt::Key_Down || event->key() == Qt::Key_End) {
        if (atEnd && m_control->textCursor().atEnd())
            emit cursorAtBoundary(Qt::BottomEdge);
    }
}

void MarkdownTextItem::inputMethodEvent(QInputMethodEvent *event)
{
    m_control->processEvent(event);
}

QVariant MarkdownTextItem::inputMethodQuery(Qt::InputMethodQuery query) const
{
    return m_control->inputMethodQuery(query, QVariant());
}

void MarkdownTextItem::focusInEvent(QFocusEvent *event)
{
    m_control->setFocus(true, event->reason());
    QGraphicsObject::focusInEvent(event);
}

void MarkdownTextItem::focusOutEvent(QFocusEvent *event)
{
    m_control->setFocus(false, event->reason());
    QGraphicsObject::focusOutEvent(event);
}

void MarkdownTextItem::onCursorPositionChanged()
{
    if (m_snappingCursor)
        return;

    // 1. Snap cursor past hidden delimiters
    snapCursorPastDelimiters();

    // 2. Notify highlighter of cursor position (shows/hides delimiters)
    auto *hl = qobject_cast<MarkdownHighlighter *>(
        m_document->findChild<QSyntaxHighlighter *>());
    if (hl) {
        QTextCursor cursor = m_control->textCursor();
        hl->setCursorPosition(cursor.block().blockNumber(),
                              cursor.positionInBlock());
    }
}

void MarkdownTextItem::snapCursorPastDelimiters()
{
    auto *hl = qobject_cast<MarkdownHighlighter *>(
        m_document->findChild<QSyntaxHighlighter *>());
    if (!hl || hl->mode() != MarkdownHighlighter::Mode::LivePreview)
        return;

    QTextCursor cursor = m_control->textCursor();
    if (cursor.hasSelection())
        return; // don't snap during selection

    int pos = cursor.position();

    for (const SourceSpan &span : hl->spans()) {
        if (!span.isDelimiter)
            continue;
        int spanStart = span.charOffset;
        int spanEnd = span.charOffset + span.charLength;
        if (pos >= spanStart && pos < spanEnd) {
            // Cursor is inside a hidden delimiter — snap to end
            m_snappingCursor = true;
            cursor.setPosition(spanEnd);
            m_control->setTextCursor(cursor);
            m_snappingCursor = false;
            return;
        }
    }
}

void MarkdownTextItem::updateGeometry()
{
    prepareGeometryChange();
}

} // namespace Markoff
