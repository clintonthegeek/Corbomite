// SPDX-License-Identifier: GPL-3.0-or-later
#include "RecentVaults.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QFileInfo>
#include <QUrl>

namespace Corbomite {

RecentVaults::RecentVaults()
{
    load();
}

QStringList RecentVaults::list() const
{
    QStringList out;
    out.reserve(m_paths.size());
    for (const QString &p : m_paths) {
        if (QFileInfo::exists(p)) out.append(p);
    }
    return out;
}

void RecentVaults::add(const QString &path)
{
    if (path.isEmpty()) return;
    m_paths.removeAll(path);
    m_paths.prepend(path);
    if (m_paths.size() > kMaxEntries) m_paths = m_paths.mid(0, kMaxEntries);
}

void RecentVaults::clear()
{
    m_paths.clear();
}

void RecentVaults::load()
{
    m_paths.clear();
    auto config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QStringLiteral("RecentVaults"));
    for (int i = 1; i <= kMaxEntries; ++i) {
        const QString entry = group.readPathEntry(
            QStringLiteral("File%1").arg(i), QString());
        if (entry.isEmpty()) continue;
        const QUrl url(entry);
        const QString path =
            url.isLocalFile() ? url.toLocalFile() : entry;
        if (!path.isEmpty()) m_paths.append(path);
    }
}

void RecentVaults::save()
{
    auto config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QStringLiteral("RecentVaults"));
    // Wipe old entries so shrinking the list doesn't leak stale keys.
    for (int i = 1; i <= kMaxEntries; ++i) {
        group.deleteEntry(QStringLiteral("File%1").arg(i));
        group.deleteEntry(QStringLiteral("Name%1").arg(i));
    }
    for (int i = 0; i < m_paths.size(); ++i) {
        const QUrl url = QUrl::fromLocalFile(m_paths.at(i));
        group.writePathEntry(QStringLiteral("File%1").arg(i + 1),
                             url.toString());
    }
    config->sync();
}

} // namespace Corbomite
