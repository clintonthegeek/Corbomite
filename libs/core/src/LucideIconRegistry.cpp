// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/LucideIconRegistry.h"

#include <QPixmap>
#include <QSvgRenderer>
#include <QPainter>

namespace Corbomite {

LucideIconRegistry &LucideIconRegistry::instance()
{
    static LucideIconRegistry singleton;
    return singleton;
}

void LucideIconRegistry::addIcon(const QString &name, const QByteArray &svg)
{
    if (name.isEmpty()) return;
    QSvgRenderer renderer(svg);
    if (!renderer.isValid()) {
        m_icons.remove(name);
        return;
    }
    // Pre-render at a generic size into a QPixmap-backed QIcon. QIcon
    // handles scaling at paint time via Qt's icon engine.
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    renderer.render(&painter);
    painter.end();
    m_icons.insert(name, QIcon(pix));
}

void LucideIconRegistry::removeIcon(const QString &name)
{
    m_icons.remove(name);
}

QIcon LucideIconRegistry::get(const QString &name) const
{
    return m_icons.value(name);
}

bool LucideIconRegistry::hasIcon(const QString &name) const
{
    auto it = m_icons.constFind(name);
    return it != m_icons.constEnd() && !it.value().isNull();
}

void LucideIconRegistry::clearForTesting()
{
    m_icons.clear();
}

} // namespace Corbomite
