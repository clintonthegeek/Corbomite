// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesHeaderView.h"
#include <QPainter>

namespace Corbomite::Bases {

BasesHeaderView::BasesHeaderView(QWidget *parent)
    : QHeaderView(Qt::Horizontal, parent)
{
    setSectionsClickable(true);
    setSectionsMovable(true);
}

void BasesHeaderView::setProviders(SortProvider sort, PropertyForColumn prop)
{
    m_sort = std::move(sort);
    m_prop = std::move(prop);
    update();
}

void BasesHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    QHeaderView::paintSection(painter, rect, logicalIndex);
    if (!m_sort || !m_prop) return;
    const PropertyId pid = m_prop(logicalIndex);
    const QVector<SortKey> keys = m_sort();
    for (int i = 0; i < keys.size(); ++i) {
        if (!(keys[i].property == pid)) continue;
        const bool asc = keys[i].direction == QLatin1String("ASC");
        const QString glyph = (asc ? QStringLiteral("▲") : QStringLiteral("▼"))
                            + (keys.size() > 1 ? QString::number(i + 1) : QString{});
        painter->save();
        painter->drawText(rect.adjusted(0, 0, -2, 0), Qt::AlignRight | Qt::AlignVCenter, glyph);
        painter->restore();
        break;
    }
}

}  // namespace Corbomite::Bases
