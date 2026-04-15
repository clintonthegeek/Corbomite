// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/HoverLinkSourceRegistry.h"

namespace Corbomite {

HoverLinkSourceRegistry::HoverLinkSourceRegistry(QObject *parent)
    : QObject(parent)
{
}

bool HoverLinkSourceRegistry::registerSource(const HoverLinkSource &source)
{
    if (source.id.isEmpty() || m_sources.contains(source.id)) return false;
    m_sources.insert(source.id, source);
    Q_EMIT sourceRegistered(source.id);
    return true;
}

void HoverLinkSourceRegistry::unregisterSource(const QString &id)
{
    if (m_sources.remove(id) > 0) Q_EMIT sourceUnregistered(id);
}

bool HoverLinkSourceRegistry::isRegistered(const QString &id) const
{
    return m_sources.contains(id);
}

HoverLinkSource HoverLinkSourceRegistry::lookup(const QString &id) const
{
    return m_sources.value(id);
}

QList<HoverLinkSource> HoverLinkSourceRegistry::allSources() const
{
    return m_sources.values();
}

void HoverLinkSourceRegistry::registerBuiltins()
{
    registerSource({QStringLiteral("editor"),    QStringLiteral("Editor"),    Qt::NoModifier});
    registerSource({QStringLiteral("search"),    QStringLiteral("Search"),    Qt::ControlModifier});
    registerSource({QStringLiteral("backlinks"), QStringLiteral("Backlinks"), Qt::NoModifier});
    registerSource({QStringLiteral("outlinks"),  QStringLiteral("Outgoing links"), Qt::NoModifier});
    registerSource({QStringLiteral("graph"),     QStringLiteral("Graph"),     Qt::NoModifier});
    // "bases" is hardcoded in Obsidian's rendering layer (domains/rendering.md §11)
    registerSource({QStringLiteral("bases"),     QStringLiteral("Bases"),     Qt::NoModifier});
}

} // namespace Corbomite
