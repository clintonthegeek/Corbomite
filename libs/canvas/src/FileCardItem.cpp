// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/FileCardItem.h"
#include "canvas/CanvasScene.h"
#include <canvas/CanvasPalette.h>

#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QStringList>
#include <QStyleOptionGraphicsItem>
#include <QTextDocument>
#include <KLocalizedString>

// Canvas dark mode — the item's scene palette, or the
// light default when the item has no scene (unit tests, detached items). The
// fallback matters: a `paint()` that returned a default-constructed
// CanvasPalette would silently render light, which is exactly the class of
// bug this change exists to remove.
// `static` (internal linkage) rather than a namespace-scope non-static
// function: this helper is replicated in three translation units, so a plain
// definition would be a duplicate symbol at link time.
static const Canvas::CanvasPalette &canvasPaletteFor(const QGraphicsScene *scene)
{
    static const Canvas::CanvasPalette kLight = Canvas::CanvasPalette::light();
    const auto *cs = qobject_cast<const Canvas::CanvasScene *>(scene);
    return cs ? cs->canvasPalette() : kLight;
}

namespace Canvas {



static constexpr qreal kCornerRadius = 8.0;
static constexpr qreal kTitleBarHeight = 28.0;
static constexpr qreal kTextPadding = 8.0;

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

void FileCardItem::setMissing(bool missing)
{
    if (m_missing == missing)
        return;
    m_missing = missing;
    update();
}

QStringList FileCardItem::buttonLabels()
{
    return { i18n("Create"), i18n("Locate…"), i18n("Remove") };
}

QVector<QRectF> FileCardItem::buttonRects() const
{
    if (!m_missing)
        return {};

    // Deterministic font so paint() and buttonAt() agree without a painter.
    QFont f;
    f.setPointSize(9);
    const QFontMetricsF fm(f);

    constexpr qreal kButtonHeight = 20.0;
    constexpr qreal kButtonGap = 6.0;
    const qreal y = kTitleBarHeight + kTextPadding + 20.0; // below the message
    qreal x = kTextPadding;

    QVector<QRectF> rects;
    const QStringList labels = buttonLabels();
    rects.reserve(labels.size());
    for (const QString &label : labels) {
        const qreal w = fm.horizontalAdvance(label) + 16.0;
        rects.append(QRectF(x, y, w, kButtonHeight));
        x += w + kButtonGap;
    }
    return rects;
}

QRectF FileCardItem::buttonRect(int index) const
{
    const QVector<QRectF> rects = buttonRects();
    if (index < 0 || index >= rects.size())
        return {};
    return rects.at(index);
}

int FileCardItem::buttonAt(const QPointF &itemPos) const
{
    const QVector<QRectF> rects = buttonRects();
    for (int i = 0; i < rects.size(); ++i) {
        if (rects.at(i).contains(itemPos))
            return i;
    }
    return -1;
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
    // Canvas dark mode.
    const Canvas::CanvasPalette &pal = canvasPaletteFor(scene());
    painter->fillPath(cardPath, pal.cardBackground);

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
    painter->setPen(stripeColor.isValid() ? QColor(255, 255, 255)
                                   : pal.titleStripForeground);
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
        // No content — missing-file placeholder: message + buttons.
        painter->save();
        painter->setPen(pal.missingForeground);
        QFont placeholderFont = painter->font();
        placeholderFont.setItalic(true);
        painter->setFont(placeholderFont);
        const qreal textTop = kTitleBarHeight + kTextPadding;
        painter->drawText(QRectF(kTextPadding, textTop, rect.width() - 2 * kTextPadding, 18),
                          Qt::AlignLeft | Qt::AlignTop,
                          i18n("File not found: %1", m_data.file));
        painter->restore();

        // Buttons drawn with the light body font (not the italic message
        // font). geometry is shared with buttonAt() via buttonRects().
        painter->save();
        QFont buttonFont = painter->font();
        buttonFont.setItalic(false);
        buttonFont.setPointSize(9);
        painter->setFont(buttonFont);
        const QStringList labels = buttonLabels();
        const QVector<QRectF> rects = buttonRects();
        for (int i = 0; i < rects.size() && i < labels.size(); ++i) {
            const QRectF r = rects.at(i);
            painter->setPen(QPen(pal.border));
            painter->setBrush(pal.cardBackground.lighter(103));
            painter->drawRoundedRect(r, 4.0, 4.0);
            painter->setPen(pal.titleStripForeground);
            painter->drawText(r, Qt::AlignCenter, labels.at(i));
        }
        painter->restore();
    }

    // 4. Border
    QPen borderPen(selected ? pal.borderSelected : pal.border);
    borderPen.setWidthF(selected ? 2.0 : 1.0);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(rect, kCornerRadius, kCornerRadius);

    // Resize-handle drawing moved to CanvasNodeChromeOverlay — one
    // shared, zoom-constant overlay retargeted to the active node, instead
    // of this triplicated per-item block. Do not re-add handle painting
    // here; see CanvasNodeChromeOverlay.h.
}

} // namespace Canvas
