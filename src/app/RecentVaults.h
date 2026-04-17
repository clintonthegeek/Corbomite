// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace Corbomite {

/// Application-level recent-vaults list, persisted to the global
/// `KSharedConfig::openConfig()` under the `[RecentVaults]` group using
/// the KRecentFilesAction-compatible `File1..File10` keys. Shares its
/// on-disk format with the `KRecentFilesAction` wired from MainWindow so
/// that both views of the list stay in sync across launches.
class RecentVaults
{
public:
    static constexpr int kMaxEntries = 10;

    RecentVaults();

    /// Returns the in-memory list, filtered to existing paths (most-recent
    /// first). Does not re-read the backing store — call `load()` first.
    QStringList list() const;

    /// Prepends `path` to the list and dedups; the list stays capped at
    /// `kMaxEntries`. Call `save()` to persist.
    void add(const QString &path);

    /// Drops every entry. Call `save()` to persist.
    void clear();

    /// Reads the list from `KSharedConfig::openConfig()`.
    void load();

    /// Writes the list to `KSharedConfig::openConfig()` and syncs.
    void save();

private:
    QStringList m_paths;
};

} // namespace Corbomite
