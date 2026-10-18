// SPDX-License-Identifier: GPL-3.0-or-later
#include "KilnRunner.h"

#include "corbomite/core/proxies/ProcessSpawner.h"

#include <QJsonDocument>
#include <QProcess>

namespace Corbomite {

KilnRunner::KilnRunner(ProcessSpawner *spawner, QObject *parent)
    : QObject(parent)
    , m_spawner(spawner)
{
}

KilnRunner::~KilnRunner() = default;

bool KilnRunner::run(const QString &enginePath, const QString &mode, const QJsonObject &config)
{
    if (!m_spawner)
        return false;

    QProcess *process = m_spawner->start(enginePath,
                                         {mode, QStringLiteral("--config"), QStringLiteral("-"),
                                          QStringLiteral("--json")},
                                         this);
    if (!process)
        return false;

    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        // `finished()` never arrives for a process that never started, so this is the
        // only place the panel can learn that the engine path is wrong. Without it the
        // button would sit there having done nothing.
        const QString message = process->errorString();
        process->deleteLater();
        Q_EMIT finished(-1, QJsonObject(), message);
    });

    connect(process, &QProcess::finished, this,
            [this, process](int exitCode, QProcess::ExitStatus) {
                const QByteArray standardOutput = process->readAllStandardOutput();
                const QString standardError = QString::fromUtf8(process->readAllStandardError());
                process->deleteLater();

                const QJsonDocument document = QJsonDocument::fromJson(standardOutput);
                Q_EMIT finished(exitCode,
                                document.isObject() ? document.object() : QJsonObject(),
                                standardError);
            });

    // QProcess buffers writes until the child starts, so this needs no waiting.
    process->write(QJsonDocument(config).toJson(QJsonDocument::Compact));
    process->closeWriteChannel();
    return true;
}

} // namespace Corbomite
