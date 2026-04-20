// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>

namespace Corbomite {

class RibbonToolBar;
class SessionManager;

/// Bridges RibbonToolBar visibility state and SessionManager's
/// workspace.json['left-ribbon'].hiddenItems. Lifetime is owned by
/// MainWindow; rebind() on vault open/close. Not thread-safe.
///
/// The controller outlives any single SessionManager — one is created
/// per vault in MainWindow::onVaultOpened. rebind() swaps the current
/// SessionManager pointer; applyFromSession() re-applies visibility
/// to whichever icons are currently registered on the toolbar.
class RibbonStateController : public QObject {
    Q_OBJECT

public:
    RibbonStateController(RibbonToolBar *toolBar,
                          SessionManager *session,
                          QObject *parent = nullptr);
    ~RibbonStateController() override;

    /// Swap the backing SessionManager. The next applyFromSession()
    /// call (or the next icon visibility change) uses the new pointer.
    /// Passing nullptr suspends write-through until a new session binds.
    void rebind(SessionManager *session);

    /// Read the current session's `left-ribbon.hiddenItems` and apply
    /// visibility to every currently-registered icon. Safe to call
    /// repeatedly and before icons are registered (in which case the
    /// controller caches the map and applies on iconAdded).
    void applyFromSession();

private Q_SLOTS:
    void onIconAdded(const QString &id);
    void onIconVisibilityChanged(const QString &id, bool visible);

private:
    void writeThrough(const QString &id, bool hidden);

    QPointer<RibbonToolBar> m_toolBar;
    QPointer<SessionManager> m_session;
    QJsonObject m_cachedHiddenItems;
};

} // namespace Corbomite
