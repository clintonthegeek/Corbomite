// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include <QList>
#include <QString>

#include "corbomite/core/RecoveryStore.h"

namespace Corbomite {

class DataAdapter;

/// Read/restore surface for one vault's recovery store — the non-UI half of
/// the Version History modal (P1.5/P1.6). Kept free of widgets so restore
/// semantics can be tested headlessly.
class VersionHistoryService
{
public:
    VersionHistoryService(DataAdapter *adapter, QString vaultRoot);

    bool isValid() const { return m_store.isValid(); }

    /// Snapshots for one file, newest first.
    QList<RecoverySnapshot> snapshots(const QString &relativePath) const;

    /// Content of a snapshot.
    std::optional<QString> preview(const RecoverySnapshot &snapshot) const;

    /// Restore `snapshot` over the file at `relativePath` (P1.6, Q4): the
    /// current on-disk content is first written as a periodic snapshot so the
    /// restore is itself recoverable, then the file is hard-overwritten
    /// (atomic adapter write). Returns false if the snapshot can't be read or
    /// the write fails.
    bool restore(const QString &relativePath, const RecoverySnapshot &snapshot);

    /// Drop every snapshot for a file (vault-exit purge).
    void purge(const QString &relativePath);

private:
    DataAdapter *m_adapter = nullptr;
    RecoveryStore m_store;
    QString m_vaultRoot;
};

}  // namespace Corbomite
