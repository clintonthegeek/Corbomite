// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include <QList>
#include <QString>

namespace Corbomite {

class DataAdapter;

/// One point-in-time recovery copy of a file.
struct RecoverySnapshot {
    QString path;          ///< adapter path of the snapshot file
    qint64 timestampMs = 0;  ///< ms since the Unix epoch (from the filename)
    bool failure = false;  ///< a save-failure backup (never pruned)
};

/// File Recovery's on-disk store — the single source of truth for where
/// snapshots live and how they are named. Everything lives OUTSIDE the vault
/// (2026-04-27 decision): a copy inside the vault shows up in the tree, search
/// and graph and re-triggers `Vault::modified` for the leaf that just failed.
///
/// Layout:  <AppLocalDataLocation>/file-recovery/<vault-id>/<basename>-<hash>/
///            <ms>.snapshot.md      periodic snapshot (pruned by age/count)
///            <ms>.failure.md       save-failure backup (never pruned)
///
/// The per-file directory is keyed on a hash of the vault-relative path, so
/// `a/foo.md` and `b/foo.md` cannot collide (a same-basename collision the
/// flat `<basename>-<timestamp>` naming would have allowed).
class RecoveryStore
{
public:
    RecoveryStore(DataAdapter *adapter, QString vaultRoot);

    /// False when the store cannot be addressed (no adapter, empty vault root,
    /// or no writable AppLocalDataLocation). Every other method then no-ops.
    bool isValid() const { return m_valid; }

    /// `<dataRoot>/file-recovery/<vault-id>` — the per-vault snapshot root.
    QString vaultDir() const;

    /// Per-file snapshot directory for a vault-relative path.
    QString fileDir(const QString &relativePath) const;

    /// Write a snapshot of `content`. `timestampMs` defaults to now. Returns
    /// the adapter path written, or an empty string when invalid.
    QString write(const QString &relativePath,
                  const QString &content,
                  bool failure,
                  qint64 timestampMs = 0);

    /// Snapshots for one file, newest first.
    QList<RecoverySnapshot> list(const QString &relativePath) const;

    /// Read a snapshot's content.
    std::optional<QString> read(const QString &snapshotPath) const;

    /// Drop periodic snapshots older than `maxAgeDays`, then the oldest
    /// periodic ones beyond `maxPerFile`. Save-failure backups are NEVER
    /// removed (Obsidian's `{"failure": true}` exemption). Returns the number
    /// removed. `maxAgeDays <= 0` disables the age ceiling; `maxPerFile <= 0`
    /// disables the count ceiling.
    int prune(const QString &relativePath,
              int maxAgeDays,
              int maxPerFile,
              qint64 nowMs = 0);

    /// Remove every snapshot for a file — the vault-exit purge (a file
    /// deleted or renamed leaves no retained entry; Obsidian removes it
    /// immediately rather than keeping it for undelete).
    void removeAll(const QString &relativePath);

private:
    DataAdapter *m_adapter = nullptr;
    QString m_vaultRoot;
    QString m_dataRoot;
    QString m_vaultId;
    bool m_valid = false;
};

}  // namespace Corbomite
