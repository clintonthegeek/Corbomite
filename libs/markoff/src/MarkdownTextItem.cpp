// SPDX-License-Identifier: GPL-3.0-or-later
#include "MarkdownTextItem.h"
#include "TextControl.h"

#include <QPainter>
#include <QTextDocument>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QFocusEvent>

namespace Markoff {

MarkdownTextItem::MarkdownTextItem(QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_document(new QTextDocument(this))
    , m_control(new TextControl(this))
{
    m_control->setDocument(m_document);
    m_control->setTextInteractionFlags(Qt::TextEditorInteraction);
    m_document->setDocumentMargin(0);

    setFlag(ItemIsFocusable);
    setFlag(ItemAcceptsInputMethod);
    setAcceptedMouseButtons(Qt::AllButtons);

    connect(m_document->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
            this, &MarkdownTextItem::updateGeometry);
    connect(m_control, &TextControl::updateRequest,
            this, [this]() { update(); });
}

MarkdownTextItem::~MarkdownTextItem() = default;

void MarkdownTextItem::setPlainText(const QString &text)
{
    m_document->setPlainText(text);
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

void MarkdownTextItem::updateGeometry()
{
    prepareGeometryChange();
}

} // namespace Markoff
