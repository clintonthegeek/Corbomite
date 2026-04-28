// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QIcon>
#include <QString>
#include <QStringList>

namespace Corbomite {

class RibbonHandle;

/// Ribbon-icon registration facade for plugins with the "ui.commands"
/// permission. Auto-namespaces ids as `<pluginId>:<localId>` so plugins
/// cannot collide with built-in or other-plugin icons. Tracks every
/// full id and removes them all on destruction.
class RibbonRegistrar
{
public:
    RibbonRegistrar(RibbonHandle *ribbon, QString pluginId);
    ~RibbonRegistrar();

    RibbonRegistrar(const RibbonRegistrar &) = delete;
    RibbonRegistrar &operator=(const RibbonRegistrar &) = delete;

    /// Register an icon. Returns the full namespaced id (`<pluginId>:<id>`)
    /// on success, empty string on failure.
    QString addRibbonIcon(const QString &localId,
                            const QIcon &icon,
                            const QString &title,
                            std::function<void()> onActivated);

    /// Remove an icon by *local* id (without the plugin prefix).
    bool removeRibbonIcon(const QString &localId);

    const QString &pluginId() const { return m_pluginId; }

private:
    RibbonHandle *m_ribbon;
    QString m_pluginId;
    QStringList m_registeredIds;
};

} // namespace Corbomite
