// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/RecoveryStore.h"

#include "corbomite/core/DataAdapter.h"
#include "corbomite/core/PathUtils.h"

#include <algorithm>

#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QStandardPaths>

namespace Corbomite {

namespace {
constexpr qint64 kMsPerDay = 24 * 60 * 60 * 1000;
const QString kSnapshotSuffix = QStringLiteral(".snapshot.md");
const QString kFailureSuffix = QStringLiteral(".failure.md");

QString shortHash(const QString &text)
{
    const QByteArray digest =
        QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(digest.left(16));
}
}  // namespace

RecoveryStore::RecoveryStore(DataAdapter *adapter, QString vaultRoot)
    : m_adapter(adapter)
    , m_vaultRoot(std::move(vaultRoot))
{
    if (!m_adapter || m_vaultRoot.isEmpty())
        return;
    m_dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (m_dataRoot.isEmpty())
        return;
    m_vaultId = PathUtils::vaultId(m_vaultRoot);
    m_valid = !m_vaultId.isEmpty();
}

QString RecoveryStore::vaultDir() const
{
    if (!m_valid)
        return {};
    return m_dataRoot + QStringLiteral("/file-recovery/") + m_vaultId;
}

QString RecoveryStore::fileDir(const QString &relativePath) const
{
    if (!m_valid)
        return {};
    const QString base = QFileInfo(relativePath).completeBaseName();
    return vaultDir() + QLatin1Char('/') + base + QLatin1Char('-')
           + shortHash(relativePath);
}

QString RecoveryStore::write(const QString &relativePath,
                             const QString &content,
                             bool failure,
                             qint64 timestampMs)
{
    if (!m_valid || relativePath.isEmpty())
        return {};

    const qint64 ts = timestampMs > 0 ? timestampMs : QDateTime::currentMSecsSinceEpoch();
    const QString dir = fileDir(relativePath);
    m_adapter->mkpath(dir);
    const QString path = dir + QLatin1Char('/') + QString::number(ts)
                         + (failure ? kFailureSuffix : kSnapshotSuffix);
    if (!m_adapter->write(path, content))
        return {};
    return path;
}

QList<RecoverySnapshot> RecoveryStore::list(const QString &relativePath) const
{
    QList<RecoverySnapshot> out;
    if (!m_valid)
        return out;

    const QString dir = fileDir(relativePath);
    for (const QString &name : m_adapter->list(dir)) {
        bool failure = false;
        QString stem = name;
        if (name.endsWith(kFailureSuffix)) {
            failure = true;
            stem.chop(kFailureSuffix.size());
        } else if (name.endsWith(kSnapshotSuffix)) {
            stem.chop(kSnapshotSuffix.size());
        } else {
            continue;  // not a snapshot we own
        }
        bool ok = false;
        const qint64 ts = stem.toLongLong(&ok);
        if (!ok)
            continue;
        out.append(RecoverySnapshot{dir + QLatin1Char('/') + name, ts, failure});
    }

    std::sort(out.begin(), out.end(),
              [](const RecoverySnapshot &a, const RecoverySnapshot &b) {
                  return a.timestampMs > b.timestampMs;  // newest first
              });
    return out;
}

std::optional<QString> RecoveryStore::read(const QString &snapshotPath) const
{
    if (!m_valid)
        return std::nullopt;
    return m_adapter->read(snapshotPath);
}

int RecoveryStore::prune(const QString &relativePath,
                         int maxAgeDays,
                         int maxPerFile,
                         qint64 nowMs)
{
    if (!m_valid)
        return 0;
    const qint64 now = nowMs > 0 ? nowMs : QDateTime::currentMSecsSinceEpoch();

    // Periodic snapshots only, oldest first.
    QList<RecoverySnapshot> periodic;
    for (const RecoverySnapshot &s : list(relativePath)) {
        if (!s.failure)
            periodic.append(s);
    }
    std::sort(periodic.begin(), periodic.end(),
              [](const RecoverySnapshot &a, const RecoverySnapshot &b) {
                  return a.timestampMs < b.timestampMs;
              });

    int removed = 0;
    auto drop = [&](const RecoverySnapshot &s) {
        if (m_adapter->remove(s.path))
            ++removed;
    };

    const qint64 cutoff =
        maxAgeDays > 0 ? now - static_cast<qint64>(maxAgeDays) * kMsPerDay : 0;
    QList<RecoverySnapshot> survivors;
    for (const RecoverySnapshot &s : periodic) {
        if (cutoff > 0 && s.timestampMs < cutoff)
            drop(s);
        else
            survivors.append(s);
    }

    if (maxPerFile > 0 && survivors.size() > maxPerFile) {
        const int excess = survivors.size() - maxPerFile;
        for (int i = 0; i < excess; ++i)
            drop(survivors.at(i));
    }
    return removed;
}

void RecoveryStore::removeAll(const QString &relativePath)
{
    if (!m_valid)
        return;
    const QString dir = fileDir(relativePath);
    for (const RecoverySnapshot &s : list(relativePath))
        m_adapter->remove(s.path);
    m_adapter->rmdir(dir);  // best-effort
}

}  // namespace Corbomite
