// SPDX-License-Identifier: GPL-3.0-or-later
#include "CalloutAtomicBlock.h"

#include <QPainter>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>

namespace Markoff {

// ---------------------------------------------------------------------------
// Color and title tables for callout types
// ---------------------------------------------------------------------------

QColor CalloutAtomicBlock::colorForType(const QString &type)
{
    // 13 built-in types from the Obsidian spec
    static const QHash<QString, QColor> colors = {
        {QStringLiteral("note"),     QColor(0x44, 0x8a, 0xff)},
        {QStringLiteral("info"),     QColor(0x44, 0x8a, 0xff)},
        {QStringLiteral("todo"),     QColor(0x44, 0x8a, 0xff)},
        {QStringLiteral("abstract"), QColor(0x00, 0xb8, 0xd4)},
        {QStringLiteral("summary"),  QColor(0x00, 0xb8, 0xd4)},
        {QStringLiteral("tldr"),     QColor(0x00, 0xb8, 0xd4)},
        {QStringLiteral("tip"),      QColor(0x00, 0xbf, 0xa5)},
        {QStringLiteral("hint"),     QColor(0x00, 0xbf, 0xa5)},
        {QStringLiteral("important"),QColor(0x00, 0xbf, 0xa5)},
        {QStringLiteral("success"),  QColor(0x00, 0xc8, 0x53)},
        {QStringLiteral("check"),    QColor(0x00, 0xc8, 0x53)},
        {QStringLiteral("done"),     QColor(0x00, 0xc8, 0x53)},
        {QStringLiteral("question"), QColor(0xff, 0xab, 0x00)},
        {QStringLiteral("help"),     QColor(0xff, 0xab, 0x00)},
        {QStringLiteral("faq"),      QColor(0xff, 0xab, 0x00)},
        {QStringLiteral("warning"),  QColor(0xff, 0x91, 0x00)},
        {QStringLiteral("caution"),  QColor(0xff, 0x91, 0x00)},
        {QStringLiteral("attention"),QColor(0xff, 0x91, 0x00)},
        {QStringLiteral("failure"),  QColor(0xff, 0x52, 0x52)},
        {QStringLiteral("fail"),     QColor(0xff, 0x52, 0x52)},
        {QStringLiteral("missing"),  QColor(0xff, 0x52, 0x52)},
        {QStringLiteral("danger"),   QColor(0xff, 0x17, 0x44)},
        {QStringLiteral("error"),    QColor(0xff, 0x17, 0x44)},
        {QStringLiteral("bug"),      QColor(0xff, 0x17, 0x44)},
        {QStringLiteral("example"),  QColor(0x7c, 0x4d, 0xff)},
        {QStringLiteral("quote"),    QColor(0x9e, 0x9e, 0x9e)},
        {QStringLiteral("cite"),     QColor(0x9e, 0x9e, 0x9e)},
    };

    return colors.value(type.toLower(), QColor(0x44, 0x8a, 0xff));
}

QString CalloutAtomicBlock::defaultTitle(const QString &type)
{
    if (type.isEmpty())
        return QStringLiteral("Note");
    return type.at(0).toUpper() + type.mid(1);
}

// ---------------------------------------------------------------------------
// CalloutAtomicBlock
// ---------------------------------------------------------------------------

CalloutAtomicBlock::CalloutAtomicBlock(QObject *parent)
    : AtomicBlock(parent)
{
}

void CalloutAtomicBlock::setCallout(const QString &type, const QString &title,
                                     const QString &body, bool foldable, bool collapsed)
{
    m_type = type;
    m_title = title.isEmpty() ? defaultTitle(type) : title;
    m_body = body;
    m_foldable = foldable;
    m_collapsed = collapsed;
    invalidateCache();
    Q_EMIT contentChanged();
}

QSizeF CalloutAtomicBlock::sizeForWidth(qreal width) const
{
    if (m_cachedWidth == width && m_cachedSize.isValid())
        return m_cachedSize;
    rebuildCache(width);
    return m_cachedSize;
}

void CalloutAtomicBlock::paint(QPainter *painter, const QRectF &rect) const
{
    if (m_cache.isNull() || m_cachedWidth != rect.width())
        rebuildCache(rect.width());
    if (!m_cache.isNull())
        painter->drawPixmap(rect.topLeft(), m_cache);
}

bool CalloutAtomicBlock::handleKeyPress(QKeyEvent *event)
{
    if (!m_focused)
        return false;

    if (event->key() == Qt::Key_Escape) {
        leaveBlock();
        return true;
    }

    // Toggle fold on Enter when focused
    if (m_foldable && event->key() == Qt::Key_Return) {
        m_collapsed = !m_collapsed;
        invalidateCache();
        Q_EMIT contentChanged();
        return true;
    }

    return true; // consume all keys when focused (read-only for now)
}

bool CalloutAtomicBlock::handleMousePress(QMouseEvent *, const QPointF &localPos)
{
    if (!m_focused) {
        enterBlock(0);
        return true;
    }

    // Click on the title bar area toggles fold
    if (m_foldable && localPos.y() < 36) {
        m_collapsed = !m_collapsed;
        invalidateCache();
        Q_EMIT contentChanged();
        return true;
    }

    return false;
}

void CalloutAtomicBlock::invalidateCache()
{
    m_cache = QPixmap();
    m_cachedWidth = -1;
    m_cachedSize = QSizeF();
}

void CalloutAtomicBlock::rebuildCache(qreal width) const
{
    if (width <= 0) {
        m_cachedSize = QSizeF(0, 0);
        return;
    }

    const QColor accentColor = colorForType(m_type);
    const int borderWidth = 4;
    const int padding = 12;
    const int titleBarHeight = 32;

    // Measure body text
    QFont bodyFont;
    bodyFont.setPointSize(11);
    QFont titleFont = bodyFont;
    titleFont.setBold(true);

    qreal bodyHeight = 0;
    QTextDocument bodyDoc;

    if (!m_collapsed && !m_body.isEmpty()) {
        bodyDoc.setDefaultFont(bodyFont);
        bodyDoc.setTextWidth(width - borderWidth - padding * 2);
        bodyDoc.setPlainText(m_body);
        bodyHeight = bodyDoc.size().height();
    }

    const qreal totalHeight = titleBarHeight + (m_collapsed ? 0 : (bodyHeight + padding));
    m_cachedSize = QSizeF(width, totalHeight);
    m_cachedWidth = width;

    const int pw = static_cast<int>(width);
    const int ph = static_cast<int>(totalHeight);
    if (pw <= 0 || ph <= 0)
        return;

    QPixmap pixmap(pw, ph);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    QRectF bgRect(borderWidth, 0, width - borderWidth, totalHeight);
    QColor bgColor = accentColor;
    bgColor.setAlpha(20);
    p.setPen(Qt::NoPen);
    p.setBrush(bgColor);
    p.drawRoundedRect(bgRect, 4, 4);

    // Left border
    p.setBrush(accentColor);
    p.drawRoundedRect(QRectF(0, 0, borderWidth, totalHeight), 2, 2);

    // Focus indicator
    if (m_focused) {
        p.setPen(QPen(accentColor, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(bgRect.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    }

    // Title bar
    p.setFont(titleFont);
    p.setPen(accentColor);

    // Fold indicator
    QString foldIndicator;
    if (m_foldable)
        foldIndicator = m_collapsed ? QStringLiteral("\u25B6 ") : QStringLiteral("\u25BC ");

    QRectF titleRect(borderWidth + padding, 0, width - borderWidth - padding * 2, titleBarHeight);
    p.drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft, foldIndicator + m_title);

    // Body text
    if (!m_collapsed && !m_body.isEmpty()) {
        p.translate(borderWidth + padding, titleBarHeight);
        p.setPen(QColor(0x33, 0x33, 0x33));
        bodyDoc.drawContents(&p);
    }

    p.end();
    m_cache = pixmap;
}

} // namespace Markoff
