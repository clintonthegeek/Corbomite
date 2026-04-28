// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/HoverLinkSource.h"

#include <QString>
#include <QStringList>

namespace Corbomite {

class HoverLinkSourceRegistry;

/// Hover-link-source registration facade for plugins with the
/// "ui.rendering" permission. Auto-namespaces source ids as
/// `<pluginId>:<localId>` so plugins cannot collide with built-in or
/// other-plugin sources. Tracks every full id and removes them all on
/// destruction.
class HoverLinkSourceRegistrar
{
public:
    HoverLinkSourceRegistrar(HoverLinkSourceRegistry *registry, QString pluginId);
    ~HoverLinkSourceRegistrar();

    HoverLinkSourceRegistrar(const HoverLinkSourceRegistrar &) = delete;
    HoverLinkSourceRegistrar &operator=(const HoverLinkSourceRegistrar &) = delete;

    /// Register a hover-link source. Mutates `source.id` in place to
    /// `<pluginId>:<id>`, then forwards to the registry. Returns false if
    /// the registry rejected the registration (e.g. id collision after
    /// prefixing — extremely unlikely).
    bool registerSource(HoverLinkSource &source);

    /// Remove a source by *local* id (without the plugin prefix).
    void unregisterSource(const QString &localId);

    const QString &pluginId() const { return m_pluginId; }

private:
    HoverLinkSourceRegistry *m_registry;
    QString m_pluginId;
    QStringList m_registeredIds;
};

} // namespace Corbomite
