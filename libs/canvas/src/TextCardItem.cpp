// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/TextCardItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTextDocument>
#include <QGraphicsSceneMouseEvent>
#include <QRegularExpression>

namespace Canvas {

static constexpr qreal kCornerRadius = 8.0;
static constexpr qreal kColorStripeHeight = 20.0;
static constexpr qreal kTextPadding = 8.0;
static constexpr qreal kHandleSize = 6.0;
static constexpr qreal kResizeZone = 8.0;

TextCardItem::TextCardItem(const CanvasNode &data, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_data(data)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(1);
    setPos(data.x, data.y);
}

void TextCardItem::setNodeData(const CanvasNode &data)
{
    prepareGeometryChange();
    m_data = data;
    setPos(data.x, data.y);
    update();
}

CanvasNode TextCardItem::nodeData() const
{
    return m_data;
}

QString TextCardItem::nodeId() const
{
    return m_data.id;
}

QRectF TextCardItem::boundingRect() const
{
    return QRectF(0, 0, m_data.width, m_data.height);
}

void TextCardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing);

    const QRectF rect = boundingRect();
    const bool selected = (option->state & QStyle::State_Selected);

    // 1. Draw rounded rect with white fill
    QPainterPath cardPath;
    cardPath.addRoundedRect(rect, kCornerRadius, kCornerRadius);

    painter->fillPath(cardPath, QColor(255, 255, 255));

    // 2. If color is set, draw color stripe at top of card
    const QColor stripeColor = colorFromCanvasColor(m_data.color);
    if (stripeColor.isValid()) {
        painter->save();
        painter->setClipPath(cardPath);

        const QRectF stripeRect(0, 0, rect.width(), kColorStripeHeight);
        painter->fillRect(stripeRect, stripeColor);

        painter->restore();
    }

    // 3. Draw border (1px gray, darker if selected)
    QPen borderPen(selected ? QColor(58, 134, 255) : QColor(200, 200, 200));
    borderPen.setWidthF(selected ? 2.0 : 1.0);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(rect, kCornerRadius, kCornerRadius);

    // 4. Render text via QTextDocument
    if (!m_data.text.isEmpty()) {
        const qreal textTop = stripeColor.isValid() ? kColorStripeHeight + kTextPadding : kTextPadding;
        const qreal textWidth = rect.width() - 2 * kTextPadding;

        QTextDocument doc;
        // Render markdown as HTML for proper formatting (headers, bold, links, etc.)
        // Simple inline conversion — handles the most common patterns
        QString html = m_data.text;
        // Headers: # Title → <h1>Title</h1>
        html.replace(QRegularExpression(QStringLiteral(R"(^### (.+)$)"), QRegularExpression::MultilineOption),
                      QStringLiteral("<h3>\\1</h3>"));
        html.replace(QRegularExpression(QStringLiteral(R"(^## (.+)$)"), QRegularExpression::MultilineOption),
                      QStringLiteral("<h2>\\1</h2>"));
        html.replace(QRegularExpression(QStringLiteral(R"(^# (.+)$)"), QRegularExpression::MultilineOption),
                      QStringLiteral("<h1>\\1</h1>"));
        // Bold: **text** → <b>text</b>
        html.replace(QRegularExpression(QStringLiteral(R"(\*\*(.+?)\*\*)")),
                      QStringLiteral("<b>\\1</b>"));
        // Italic: *text* → <i>text</i>
        html.replace(QRegularExpression(QStringLiteral(R"(\*(.+?)\*)")),
                      QStringLiteral("<i>\\1</i>"));
        // Wikilinks: [[text]] → <a>text</a> (styled, not clickable in paint)
        html.replace(QRegularExpression(QStringLiteral(R"(\[\[([^\]]+)\]\])")),
                      QStringLiteral("<a style='color:#7b6cd9'>\\1</a>"));
        // Line breaks: \n → <br> (except those already in block elements)
        html.replace(QStringLiteral("\n"), QStringLiteral("<br>"));
        // TODO: Use Corbomite's MarkdownRenderer for full rendering when integrated
        // (libcanvas is standalone, so we do a lightweight version here)
        doc.setHtml(html);
        doc.setTextWidth(textWidth);
        doc.setDocumentMargin(0);

        painter->save();
        painter->translate(kTextPadding, textTop);

        // Clip text to card bounds
        const qreal availableHeight = rect.height() - textTop - kTextPadding;
        painter->setClipRect(QRectF(0, 0, textWidth, availableHeight));

        doc.drawContents(painter);
        painter->restore();
    }

    // 5. If selected, draw resize handles at 8 positions
    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(58, 134, 255));

        const qreal hs = kHandleSize;
        const qreal hh = hs / 2.0;
        const qreal w = rect.width();
        const qreal h = rect.height();

        // Corners
        painter->drawRect(QRectF(-hh, -hh, hs, hs));                          // TopLeft
        painter->drawRect(QRectF(w - hh, -hh, hs, hs));                       // TopRight
        painter->drawRect(QRectF(w - hh, h - hh, hs, hs));                    // BottomRight
        painter->drawRect(QRectF(-hh, h - hh, hs, hs));                       // BottomLeft

        // Edge midpoints
        painter->drawRect(QRectF(w / 2.0 - hh, -hh, hs, hs));                // Top
        painter->drawRect(QRectF(w - hh, h / 2.0 - hh, hs, hs));             // Right
        painter->drawRect(QRectF(w / 2.0 - hh, h - hh, hs, hs));             // Bottom
        painter->drawRect(QRectF(-hh, h / 2.0 - hh, hs, hs));                // Left
    }
}

QPointF TextCardItem::connectionPoint(Side side) const
{
    const QRectF rect = boundingRect();
    QPointF local;
    switch (side) {
    case Side::Top:
        local = QPointF(rect.width() / 2.0, 0);
        break;
    case Side::Right:
        local = QPointF(rect.width(), rect.height() / 2.0);
        break;
    case Side::Bottom:
        local = QPointF(rect.width() / 2.0, rect.height());
        break;
    case Side::Left:
        local = QPointF(0, rect.height() / 2.0);
        break;
    }
    return mapToScene(local);
}

TextCardItem::ResizeMode TextCardItem::resizeModeAtPos(const QPointF &localPos) const
{
    const QRectF rect = boundingRect();
    const qreal x = localPos.x();
    const qreal y = localPos.y();
    const qreal w = rect.width();
    const qreal h = rect.height();

    const bool nearLeft   = x < kResizeZone;
    const bool nearRight  = x > w - kResizeZone;
    const bool nearTop    = y < kResizeZone;
    const bool nearBottom = y > h - kResizeZone;

    // Corners first (have priority)
    if (nearTop && nearLeft)     return TopLeft;
    if (nearTop && nearRight)    return TopRight;
    if (nearBottom && nearRight) return BottomRight;
    if (nearBottom && nearLeft)  return BottomLeft;

    // Edges
    if (nearTop)    return Top;
    if (nearRight)  return Right;
    if (nearBottom) return Bottom;
    if (nearLeft)   return Left;

    return NoResize;
}

QVariant TextCardItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        Q_EMIT positionChanged();
    }
    return QGraphicsObject::itemChange(change, value);
}

void TextCardItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    Q_EMIT editRequested();
}

} // namespace Canvas
