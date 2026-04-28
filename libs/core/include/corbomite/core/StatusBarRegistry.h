// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

class QStatusBar;
class QWidget;

namespace Corbomite {

/// Host-side registry that lets plugins add permanent widgets to the
/// application status bar. Wraps `QMainWindow::statusBar()` so plugin
/// proxies don't depend on `MainWindow` directly.
class StatusBarRegistry : public QObject
{
    Q_OBJECT
public:
    explicit StatusBarRegistry(QStatusBar *bar, QObject *parent = nullptr);

    /// Add `widget` as a permanent status-bar item under `id`. Takes
    /// ownership via reparenting onto the status bar. Returns false if
    /// `id` was already registered (caller's widget is NOT taken in
    /// that case).
    bool addItem(const QString &id, QWidget *widget);

    /// Remove the item under `id`. Returns true if an item was removed.
    /// The widget is `deleteLater()`d.
    bool removeItem(const QString &id);

    bool hasItem(const QString &id) const;
    int itemCount() const { return m_items.size(); }

private:
    QStatusBar *m_bar;
    QHash<QString, QPointer<QWidget>> m_items;
};

} // namespace Corbomite
