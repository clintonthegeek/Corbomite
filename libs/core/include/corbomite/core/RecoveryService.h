// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include "corbomite/core/RecoveryStore.h"

class QTimer;

namespace Corbomite {

class DataAdapter;
class TextFileView;

/// Host-level periodic snapshot of dirty text leaves — Obsidian's File
/// Recovery core plugin. One timer for the whole app (not one per leaf), so
/// there is a single vault-aware place to start/stop it and it composes with,
/// rather than races, each view's 2 s save debounce.
///
/// The service owns the retention policy and writes periodic snapshots
/// through a RecoveryStore (the same on-disk layout as the save-failure path
/// on TextFileView), pruning on each write per P1.2.
class RecoveryService : public QObject
{
    Q_OBJECT
public:
    /// Supplies the live text leaves to consider each pass. MainWindow wires
    /// this to the workspace; tests supply their own.
    using ViewProvider = std::function<QList<TextFileView *>()>;

    RecoveryService(ViewProvider provider,
                    DataAdapter *adapter,
                    QString vaultRoot,
                    QObject *parent = nullptr);

    /// Snapshot interval in minutes. 0 disables the timer (start() then no-ops).
    /// Obsidian's default is 60.
    void setIntervalMinutes(int minutes);
    int intervalMinutes() const { return m_intervalMinutes; }

    /// Retention ceilings applied after each periodic write. Both are in the
    /// same units as RecoveryStore::prune; <= 0 disables that ceiling.
    void setRetention(int maxAgeDays, int maxPerFile);
    int maxAgeDays() const { return m_maxAgeDays; }
    int maxPerFile() const { return m_maxPerFile; }

    void start();
    void stop();
    bool isRunning() const;

    /// Run one snapshot pass immediately. Returns the number of snapshots
    /// written. Skips clean views, and views whose content equals the last
    /// content this service snapshotted (so an idle but dirty leaf is not
    /// rewritten every tick).
    int snapshotNow();

private:
    ViewProvider m_provider;
    RecoveryStore m_store;
    QTimer *m_timer = nullptr;
    QHash<TextFileView *, QString> m_lastSnapshot;
    int m_intervalMinutes = 60;
    int m_maxAgeDays = 7;
    int m_maxPerFile = 50;
};

}  // namespace Corbomite
