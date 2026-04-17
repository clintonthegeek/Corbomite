// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>

namespace Corbomite {

/// Atomic JSON persistence at `<pluginDir>/data.json`.
///
/// `pluginDir` is typically the vault's
/// `.obsidian/plugins/<plugin-id>/` directory; PluginManager creates it
/// before handing a PluginDataStore to the plugin via PluginContext.
class PluginDataStore
{
public:
    explicit PluginDataStore(QString pluginDir);

    /// Returns the stored object, or an empty object if the file is
    /// missing / unreadable / not a JSON object.
    QJsonObject load() const;

    /// Atomically writes `obj` via QSaveFile. Returns false on I/O error.
    bool save(const QJsonObject &obj);

private:
    QString m_pluginDir;

    QString dataFilePath() const;
};

} // namespace Corbomite
