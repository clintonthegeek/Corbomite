// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

class QProcess;

namespace Corbomite {

/// Process-spawning facade for plugins with the "process" permission.
/// All invocations logged at qCDebug with the owning plugin id.
/// Stub — wire-up lands in Cluster Q Task 10.
class ProcessSpawner
{
public:
    explicit ProcessSpawner(QString pluginId) : m_pluginId(std::move(pluginId)) {}

    QProcess *start(const QString &program, const QStringList &args = {});
    bool startDetached(const QString &program, const QStringList &args = {});

    const QString &pluginId() const { return m_pluginId; }

private:
    QString m_pluginId;
};

} // namespace Corbomite
