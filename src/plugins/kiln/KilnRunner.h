// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

class QProcess;

namespace Corbomite {

class ProcessSpawner;

/// Runs the engine and hands back its report.
///
/// A seam rather than a QProcess call inside the panel. A flow that owns its process
/// cannot be asserted — and what is worth asserting here is the decision about what
/// to show, not whether a subprocess can run. The test supplies a runner that answers
/// immediately, which turns the whole flow into a straight line.
class KilnRunner : public QObject
{
    Q_OBJECT
public:
    explicit KilnRunner(ProcessSpawner *spawner, QObject *parent = nullptr);
    ~KilnRunner() override;

    /// `kiln <mode> --config - --json`, with the config written to the child's stdin.
    ///
    /// Stdin rather than a file: the plugin already holds the config, and a file would
    /// mean either a temporary that has to be cleaned up or a guess at where the host
    /// keeps a plugin's data. Returns false when no process could be started at all.
    virtual bool run(const QString &enginePath, const QString &mode, const QJsonObject &config);

Q_SIGNALS:
    /// `exitCode` is the engine's, or -1 when it never started. `report` is its JSON
    /// report, or empty when it produced none.
    void finished(int exitCode, const QJsonObject &report, const QString &standardError);

private:
    ProcessSpawner *m_spawner = nullptr;
};

} // namespace Corbomite
