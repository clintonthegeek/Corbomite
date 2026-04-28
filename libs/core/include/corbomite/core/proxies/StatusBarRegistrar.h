// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

class QWidget;

namespace Corbomite {

class StatusBarRegistry;

/// Status-bar registration facade for plugins with the "ui.statusbar"
/// permission. Auto-namespaces ids as `<pluginId>:<localId>`. Tracks
/// every full id and removes them all on destruction.
class StatusBarRegistrar
{
public:
    StatusBarRegistrar(StatusBarRegistry *registry, QString pluginId);
    ~StatusBarRegistrar();

    StatusBarRegistrar(const StatusBarRegistrar &) = delete;
    StatusBarRegistrar &operator=(const StatusBarRegistrar &) = delete;

    /// Add `widget` to the status bar under `<pluginId>:<localId>`.
    /// Takes ownership of `widget` via the registry. Returns the full
    /// namespaced id on success, empty string on failure.
    QString addItem(const QString &localId, QWidget *widget);

    /// Remove an item by *local* id (without the plugin prefix).
    bool removeItem(const QString &localId);

    const QString &pluginId() const { return m_pluginId; }

private:
    StatusBarRegistry *m_registry;
    QString m_pluginId;
    QStringList m_registeredIds;
};

} // namespace Corbomite
