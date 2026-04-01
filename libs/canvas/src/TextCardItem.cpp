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

    // Pre-process text: extract header and strip frontmatter
    QString cardText = m_data.text;
    QString headerText;

    // Strip YAML frontmatter (---\n...\n---)
    static const QRegularExpression frontmatterPattern(
        QStringLiteral(R"(^---\n[\s\S]*?\n---\n?)"), QRegularExpression::MultilineOption);
    cardText.replace(frontmatterPattern, QString());

    // Extract callout-style header: >[!cc-header] Title or >[!some-type] Title
    static const QRegularExpression calloutHeaderPattern(
        QStringLiteral(R"(^>\s*\[![\w-]+\]\s*(.+)$)"), QRegularExpression::MultilineOption);
    auto headerMatch = calloutHeaderPattern.match(cardText);
    if (headerMatch.hasMatch()) {
        headerText = headerMatch.captured(1).trimmed();
        cardText.replace(calloutHeaderPattern, QString());
    }

    // Also extract regular markdown # headers as card header (if no callout header found)
    if (headerText.isEmpty()) {
        static const QRegularExpression mdHeaderPattern(
            QStringLiteral(R"(^#{1,3}\s+(.+)$)"), QRegularExpression::MultilineOption);
        auto mdMatch = mdHeaderPattern.match(cardText);
        if (mdMatch.hasMatch()) {
            headerText = mdMatch.captured(1).trimmed();
            cardText.replace(mdMatch.capturedStart(), mdMatch.capturedLength(), QString());
        }
    }

    // Trim leading/trailing whitespace after stripping
    cardText = cardText.trimmed();

    // 2. Draw color stripe / header bar
    const QColor stripeColor = colorFromCanvasColor(m_data.color);
    qreal headerBarHeight = 0;

    // Process header text: strip markdown syntax, convert to HTML for rendering
    auto processHeaderHtml = [](const QString &raw) -> QString {
        QString h = raw;
        // Strip leading # markers: "# Welcome!" → "Welcome!"
        h.replace(QRegularExpression(QStringLiteral(R"(^#{1,6}\s+)")), QString());
        // Bold: **text** → <b>text</b>
        h.replace(QRegularExpression(QStringLiteral(R"(\*\*(.+?)\*\*)")),
                  QStringLiteral("<b>\\1</b>"));
        // Italic: *text* → <i>text</i>
        h.replace(QRegularExpression(QStringLiteral(R"((?<!\*)\*([^*]+?)\*(?!\*))")),
                  QStringLiteral("<i>\\1</i>"));
        return h;
    };

    if (!headerText.isEmpty() && stripeColor.isValid()) {
        // Full header bar with text inside the color stripe
        headerBarHeight = 32.0;
        painter->save();
        painter->setClipPath(cardPath);
        painter->fillRect(QRectF(0, 0, rect.width(), headerBarHeight), stripeColor);
        painter->restore();

        // Render header with QTextDocument for inline markdown support
        QTextDocument headerDoc;
        QString headerHtml = QStringLiteral("<span style='color:white;font-size:11pt'>")
            + processHeaderHtml(headerText) + QStringLiteral("</span>");
        headerDoc.setHtml(headerHtml);
        headerDoc.setTextWidth(rect.width() - 2 * kTextPadding);
        headerDoc.setDocumentMargin(0);

        painter->save();
        painter->translate(kTextPadding, (headerBarHeight - headerDoc.size().height()) / 2.0);
        headerDoc.drawContents(painter);
        painter->restore();
    } else if (!headerText.isEmpty()) {
        // Header text without color
        headerBarHeight = 28.0;

        QTextDocument headerDoc;
        QString headerHtml = QStringLiteral("<span style='color:#282828;font-size:11pt'>")
            + processHeaderHtml(headerText) + QStringLiteral("</span>");
        headerDoc.setHtml(headerHtml);
        headerDoc.setTextWidth(rect.width() - 2 * kTextPadding);
        headerDoc.setDocumentMargin(0);

        painter->save();
        painter->translate(kTextPadding, (headerBarHeight - headerDoc.size().height()) / 2.0);
        headerDoc.drawContents(painter);
        painter->restore();
    } else if (stripeColor.isValid()) {
        // Color stripe only (thin accent, no header text)
        headerBarHeight = kColorStripeHeight;
        painter->save();
        painter->setClipPath(cardPath);
        painter->fillRect(QRectF(0, 0, rect.width(), headerBarHeight), stripeColor);
        painter->restore();
    }

    // 3. Draw border (1px gray, darker if selected)
    QPen borderPen(selected ? QColor(58, 134, 255) : QColor(200, 200, 200));
    borderPen.setWidthF(selected ? 2.0 : 1.0);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(rect, kCornerRadius, kCornerRadius);

    // 4. Render body text via QTextDocument
    if (!cardText.isEmpty()) {
        const qreal textTop = headerBarHeight > 0 ? headerBarHeight + kTextPadding : kTextPadding;
        const qreal textWidth = rect.width() - 2 * kTextPadding;

        // Convert remaining markdown to HTML
        QString html = cardText;

        // Bold: **text** → <b>text</b>
        html.replace(QRegularExpression(QStringLiteral(R"(\*\*(.+?)\*\*)")),
                      QStringLiteral("<b>\\1</b>"));
        // Italic: *text* → <i>text</i>
        html.replace(QRegularExpression(QStringLiteral(R"((?<!\*)\*([^*]+?)\*(?!\*))")),
                      QStringLiteral("<i>\\1</i>"));
        // Wikilinks with alias: [[target|display]] → styled link showing display text
        html.replace(QRegularExpression(QStringLiteral(R"(\[\[([^\]|]+)\|([^\]]+)\]\])")),
                      QStringLiteral("<a style='color:#7b6cd9;text-decoration:underline'>\\2</a>"));
        // Wikilinks: [[text]] → styled link
        html.replace(QRegularExpression(QStringLiteral(R"(\[\[([^\]]+)\]\])")),
                      QStringLiteral("<a style='color:#7b6cd9;text-decoration:underline'>\\1</a>"));
        // Markdown links: [text](url) → styled link
        html.replace(QRegularExpression(QStringLiteral(R"(\[([^\]]+)\]\(([^)]+)\))")),
                      QStringLiteral("<a style='color:#7b6cd9;text-decoration:underline'>\\1</a>"));
        // Inline code: `code` → <code>
        html.replace(QRegularExpression(QStringLiteral(R"(`([^`]+)`)")),
                      QStringLiteral("<code style='background:#f0f0f0;padding:1px 3px'>\\1</code>"));
        // Unordered list items: - item → bullet
        html.replace(QRegularExpression(QStringLiteral(R"(^- (.+)$)"), QRegularExpression::MultilineOption),
                      QStringLiteral("&bull; \\1"));
        // Line breaks
        html.replace(QStringLiteral("\n"), QStringLiteral("<br>"));

        // TODO: Use Corbomite's MarkdownRenderer for full rendering when integrated
        // (libcanvas is standalone, so we do a lightweight version here)

        QTextDocument doc;
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
