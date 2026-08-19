// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/FileCardItem.h"

#include <QFileInfo>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTextDocument>
#include <KLocalizedString>

namespace Canvas {

static constexpr qreal kCornerRadius = 8.0;
static constexpr qreal kTitleBarHeight = 28.0;
static constexpr qreal kTextPadding = 8.0;
static constexpr qreal kHandleSize = 6.0;

FileCardItem::FileCardItem(const CanvasNode &data, QGraphicsItem *parent)
    : CanvasNodeItem(data, parent)
{
    setZValue(1);
}

void FileCardItem::setRenderedDocument(std::unique_ptr<Corbomite::RenderedDocument> doc)
{
    m_renderedDoc = std::move(doc);
    update();
}

bool FileCardItem::hasRenderedDocument() const
{
    return m_renderedDoc && m_renderedDoc->toQTextDocument() != nullptr;
}

QString FileCardItem::displayTitle() const
{
    // Extract filename without extension
    QString title = QFileInfo(m_data.file).completeBaseName();
    if (title.isEmpty())
        title = m_data.file;

    // Append subpath if present
    if (!m_data.subpath.isEmpty()) {
        QString sub = m_data.subpath;
        if (sub.startsWith(QLatin1Char('#')))
            sub = sub.mid(1);
        if (sub.startsWith(QLatin1Char('^')))
            sub = sub.mid(1);
        title += QStringLiteral(" > ") + sub;
    }

    return title;
}

QRectF FileCardItem::boundingRect() const
{
    return QRectF(0, 0, m_data.width, m_data.height);
}

void FileCardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing);

    const QRectF rect = boundingRect();
    const bool selected = (option->state & QStyle::State_Selected);

    // 1. Rounded rect with white fill
    QPainterPath cardPath;
    cardPath.addRoundedRect(rect, kCornerRadius, kCornerRadius);
    painter->fillPath(cardPath, QColor(255, 255, 255));

    // 2. Title bar
    const QColor stripeColor = colorFromCanvasColor(m_data.color);
    const QColor titleBg = stripeColor.isValid() ? stripeColor : QColor(240, 240, 240);

    painter->save();
    painter->setClipPath(cardPath);
    painter->fillRect(QRectF(0, 0, rect.width(), kTitleBarHeight), titleBg);
    painter->restore();

    // Title text
    QFont titleFont = painter->font();
    titleFont.setPointSize(10);
    titleFont.setBold(true);
    painter->save();
    painter->setFont(titleFont);
    painter->setPen(stripeColor.isValid() ? QColor(255, 255, 255) : QColor(40, 40, 40));
    painter->drawText(QRectF(kTextPadding, 0, rect.width() - 2 * kTextPadding, kTitleBarHeight),
                      Qt::AlignVCenter | Qt::AlignLeft,
                      displayTitle());
    painter->restore();

    // 3. Render body content
    if (m_renderedDoc && m_renderedDoc->toQTextDocument()) {
        const qreal textTop = kTitleBarHeight + kTextPadding;
        const qreal textWidth = rect.width() - 2 * kTextPadding;
        const qreal availableHeight = rect.height() - textTop - kTextPadding;

        QTextDocument *doc = m_renderedDoc->toQTextDocument();
        doc->setTextWidth(textWidth);

        painter->save();
        painter->translate(kTextPadding, textTop);
        painter->setClipRect(QRectF(0, 0, textWidth, availableHeight));
        doc->drawContents(painter);
        painter->restore();
    } else {
        // No content — show placeholder
        painter->save();
        painter->setPen(QColor(160, 160, 160));
        QFont placeholderFont = painter->font();
        placeholderFont.setItalic(true);
        painter->setFont(placeholderFont);
        const qreal textTop = kTitleBarHeight + kTextPadding;
        painter->drawText(QRectF(kTextPadding, textTop, rect.width() - 2 * kTextPadding, 30),
                          Qt::AlignLeft | Qt::AlignTop,
                          i18n("File not found"));
        painter->restore();
    }

    // 4. Border
    QPen borderPen(selected ? QColor(58, 134, 255) : QColor(200, 200, 200));
    borderPen.setWidthF(selected ? 2.0 : 1.0);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(rect, kCornerRadius, kCornerRadius);

    // 5. Resize handles when selected
    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(58, 134, 255));

        const qreal hs = kHandleSize;
        const qreal hh = hs / 2.0;
        const qreal w = rect.width();
        const qreal h = rect.height();

        painter->drawRect(QRectF(-hh, -hh, hs, hs));
        painter->drawRect(QRectF(w - hh, -hh, hs, hs));
        painter->drawRect(QRectF(w - hh, h - hh, hs, hs));
        painter->drawRect(QRectF(-hh, h - hh, hs, hs));
        painter->drawRect(QRectF(w / 2.0 - hh, -hh, hs, hs));
        painter->drawRect(QRectF(w - hh, h / 2.0 - hh, hs, hs));
        painter->drawRect(QRectF(w / 2.0 - hh, h - hh, hs, hs));
        painter->drawRect(QRectF(-hh, h / 2.0 - hh, hs, hs));
    }
}

} // namespace Canvas
