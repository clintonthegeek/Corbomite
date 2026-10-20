// SPDX-License-Identifier: GPL-3.0-or-later
//
// RE-ENTRANCY HAZARD: every keyring operation here waits on a local
// `QEventLoop::exec()` (three call sites: setSecret, getSecret,
// deleteSecret). The loop is local to the call — it is not nested inside
// another one — but it *blocks the calling thread* until the keychain job
// finishes, and it spins a nested event loop while it waits. A plugin that
// calls this from a slot that the keychain's own completion could re-enter
// would deadlock; so would a caller that holds a lock this path needs.
//
// The synchronous shape is deliberate for now (the plugin API is
// synchronous), and the fallback to the in-process QHash keeps the call
// working when the keyring is unavailable. The async rework — jobs that
// report through a callback instead of a local event loop — is filed in the
// punch list.
#include "corbomite/core/proxies/SecretStorage.h"

#ifdef CORBOMITE_HAVE_KEYRING
#include <qt6keychain/keychain.h>
#endif

#include <QEventLoop>
#include <QHash>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>

namespace Corbomite {

namespace {

Q_LOGGING_CATEGORY(lcSecretStorage, "corbomite.plugin.secret-storage")

constexpr char kSecretsToken[] = "secrets";
constexpr char kKeyringService[] = "corbomite-plugin";

// Granted-by-default legacy ctor seeds this token so the single-arg form
// behaves like the old unconditional store.
QSet<QString> allPermissionsGranted()
{
    return {QStringLiteral("secrets")};
}

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
    // "." separator for keyring-friendly namespacing (GNOME Keyring, KWallet
    // and macOS Keychain all accept arbitrary strings, but a dotted key is
    // convention and keeps the scoping visible to the user inspecting the
    // keyring).
    return pluginId + QLatin1Char('.') + id;
}

} // namespace

SecretStorage::SecretStorage(QString pluginId)
    : m_pluginId(std::move(pluginId)),
      m_granted(allPermissionsGranted()) {}

SecretStorage::SecretStorage(QString pluginId, QSet<QString> granted)
    : m_pluginId(std::move(pluginId)),
      m_granted(std::move(granted)) {}

bool SecretStorage::hasSecretsPermission() const
{
    return m_granted.contains(QString::fromLatin1(kSecretsToken));
}

bool SecretStorage::setSecret(const QString &id, const QString &value)
{
    if (id.isEmpty()) return false;
    if (!hasSecretsPermission()) return false;

    const QString key = fullKey(m_pluginId, id);

#ifdef CORBOMITE_HAVE_KEYRING
    QKeychain::WritePasswordJob job(QString::fromLatin1(kKeyringService));
    job.setAutoDelete(false);
    job.setKey(key);
    job.setTextData(value);
    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
    if (job.error() == QKeychain::NoError) return true;
    qCWarning(lcSecretStorage)
        << "keyring write failed for" << m_pluginId << ":" << job.errorString()
        << "- falling back to in-process QHash (will not persist)";
#endif

    QMutexLocker lock(&storeMutex());
    store().insert(key, value);
    return true;
}

QString SecretStorage::getSecret(const QString &id) const
{
    if (id.isEmpty()) return {};
    if (!hasSecretsPermission()) return {};

    const QString key = fullKey(m_pluginId, id);

#ifdef CORBOMITE_HAVE_KEYRING
    QKeychain::ReadPasswordJob job(QString::fromLatin1(kKeyringService));
    job.setAutoDelete(false);
    job.setKey(key);
    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
    if (job.error() == QKeychain::NoError) return job.textData();
    // EntryNotFound is an expected miss; only log other failures.
    if (job.error() != QKeychain::EntryNotFound) {
        qCWarning(lcSecretStorage)
            << "keyring read failed for" << m_pluginId << ":" << job.errorString()
            << "- consulting in-process QHash fallback";
    }
#endif

    QMutexLocker lock(&storeMutex());
    return store().value(key);
}

bool SecretStorage::deleteSecret(const QString &id)
{
    if (id.isEmpty()) return false;
    if (!hasSecretsPermission()) return false;

    const QString key = fullKey(m_pluginId, id);
    bool removed = false;

#ifdef CORBOMITE_HAVE_KEYRING
    QKeychain::DeletePasswordJob job(QString::fromLatin1(kKeyringService));
    job.setAutoDelete(false);
    job.setKey(key);
    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
    if (job.error() == QKeychain::NoError) {
        removed = true;
    } else if (job.error() != QKeychain::EntryNotFound) {
        qCWarning(lcSecretStorage)
            << "keyring delete failed for" << m_pluginId << ":" << job.errorString();
    }
#endif

    QMutexLocker lock(&storeMutex());
    if (store().remove(key) > 0) removed = true;
    return removed;
}

QStringList SecretStorage::listSecrets() const
{
    if (!hasSecretsPermission()) return {};

    const QString prefix = m_pluginId + QLatin1Char('.');
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
