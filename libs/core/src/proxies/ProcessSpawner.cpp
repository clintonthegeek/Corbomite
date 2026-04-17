// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/ProcessSpawner.h"

#include <QLoggingCategory>
#include <QObject>
#include <QProcess>

namespace Corbomite {

Q_LOGGING_CATEGORY(lcProcessSpawner, "corbomite.process-spawner")

ProcessSpawner::ProcessSpawner(QString pluginId) : m_pluginId(std::move(pluginId)) {}

QProcess *ProcessSpawner::start(const QString &program, const QStringList &args,
                                QObject *parent)
{
    qCDebug(lcProcessSpawner) << "plugin" << m_pluginId << "start"
                              << program << args;
    auto *p = new QProcess(parent);
    p->start(program, args);
    return p;
}

bool ProcessSpawner::startDetached(const QString &program, const QStringList &args)
{
    qCDebug(lcProcessSpawner) << "plugin" << m_pluginId << "startDetached"
                              << program << args;
    return QProcess::startDetached(program, args);
}

} // namespace Corbomite
