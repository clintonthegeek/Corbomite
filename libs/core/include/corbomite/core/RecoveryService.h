// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

class QTimer;

namespace Corbomite {

class TextFileView;

/// Host-level periodic snapshot of dirty text leaves — Obsidian's File
/// Recovery core plugin. One timer for the whole app (not one per leaf), so
/// there is a single vault-aware place to start/stop it and it composes with,
/// rather than races, each view's 2 s save debounce.
///
/// The service owns no storage: it asks each dirty view to write its own
/// snapshot (`TextFileView::writeRecoverySnapshot`), which keeps the
/// out-of-vault path and the adapter seam in one place.
class RecoveryService : public QObject
{
    Q_OBJECT
public:
    /// Supplies the live text leaves to consider each pass. MainWindow wires
    /// this to the workspace; tests supply their own.
    using ViewProvider = std::function<QList<TextFileView *>()>;

    explicit RecoveryService(ViewProvider provider, QObject *parent = nullptr);

    /// Snapshot interval in minutes. 0 disables the timer (start() then no-ops).
    /// Obsidian's default is 60.
    void setIntervalMinutes(int minutes);
    int intervalMinutes() const { return m_intervalMinutes; }

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
    QTimer *m_timer = nullptr;
    QHash<TextFileView *, QString> m_lastSnapshot;
    int m_intervalMinutes = 60;
};

}  // namespace Corbomite
