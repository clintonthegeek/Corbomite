// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

class QObject;
class QProcess;

namespace Corbomite {

/// Process-spawning facade for plugins with the "process" permission.
///
/// All invocations log at qCDebug under "corbomite.process-spawner" with
/// the owning plugin id, so audit trails attribute external process
/// launches back to the plugin that invoked them.
class ProcessSpawner
{
public:
    explicit ProcessSpawner(QString pluginId);

    ProcessSpawner(const ProcessSpawner &) = delete;
    ProcessSpawner &operator=(const ProcessSpawner &) = delete;

    /// Start a child process. The returned QProcess is parented to
    /// `parent` (or nullptr — caller owns the lifetime). Started via
    /// QProcess::start().
    QProcess *start(const QString &program, const QStringList &args = {},
                    QObject *parent = nullptr);

    /// Spawn a detached process. Returns true on success.
    bool startDetached(const QString &program, const QStringList &args = {});

    const QString &pluginId() const { return m_pluginId; }

private:
    QString m_pluginId;
};

} // namespace Corbomite
