// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/SecretStorage.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>

namespace Corbomite {

namespace {

QMutex &storeMutex()
{
    static QMutex m;
    return m;
}

QHash<QString, QString> &store()
{
    static QHash<QString, QString> s;
    return s;
}

QString fullKey(const QString &pluginId, const QString &id)
{
    return pluginId + QLatin1Char(':') + id;
}

} // namespace

SecretStorage::SecretStorage(QString pluginId) : m_pluginId(std::move(pluginId)) {}

bool SecretStorage::setSecret(const QString &id, const QString &value)
{
    if (id.isEmpty()) return false;
    QMutexLocker lock(&storeMutex());
    store().insert(fullKey(m_pluginId, id), value);
    return true;
}

QString SecretStorage::getSecret(const QString &id) const
{
    QMutexLocker lock(&storeMutex());
    return store().value(fullKey(m_pluginId, id));
}

bool SecretStorage::deleteSecret(const QString &id)
{
    QMutexLocker lock(&storeMutex());
    return store().remove(fullKey(m_pluginId, id)) > 0;
}

QStringList SecretStorage::listSecrets() const
{
    const QString prefix = m_pluginId + QLatin1Char(':');
    QStringList out;
    {
        QMutexLocker lock(&storeMutex());
        const auto keys = store().keys();
        for (const QString &k : keys) {
            if (k.startsWith(prefix)) out.append(k.mid(prefix.size()));
        }
    }
    out.sort();
    return out;
}

} // namespace Corbomite
