// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Corbomite {

class LucideIconRegistry;

/// Lucide-icon registration facade for plugins with the "ui.icons"
/// permission. Auto-namespaces names as `<pluginId>:<localName>` so
/// plugins cannot clobber built-in icon names. Tracks every full name
/// and removes them all on destruction.
class LucideIconRegistrar
{
public:
    LucideIconRegistrar(LucideIconRegistry *registry, QString pluginId);
    ~LucideIconRegistrar();

    LucideIconRegistrar(const LucideIconRegistrar &) = delete;
    LucideIconRegistrar &operator=(const LucideIconRegistrar &) = delete;

    /// Register an SVG-encoded icon. Stored as `<pluginId>:<localName>`.
    /// Returns the full namespaced name on success; empty string on
    /// failure (no registry, empty name, or invalid SVG).
    QString addIcon(const QString &localName, const QByteArray &svg);

    /// Remove an icon by *local* name (without the plugin prefix).
    void removeIcon(const QString &localName);

    const QString &pluginId() const { return m_pluginId; }

private:
    LucideIconRegistry *m_registry;
    QString m_pluginId;
    QStringList m_registeredNames;
};

} // namespace Corbomite
