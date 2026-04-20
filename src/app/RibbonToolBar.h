// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QHash>
#include <QIcon>
#include <QString>
#include <QStringList>

#include <KToolBar>

class QAction;

namespace Corbomite {

/// Top-docked, programmatically-managed second toolbar. Serves as the
/// Corbomite translation of Obsidian's left-edge ribbon. Action identity
/// is a full id string (Obsidian convention: "<pluginId>:<title>" for
/// plugin-registered items, a stable internal id for core items).
///
/// Per-vault state — visibility per icon — is held in
/// workspace.json['left-ribbon']. This class is stateless with respect
/// to the vault; RibbonStateController drives setIconVisible/...
/// based on SessionManager content.
///
/// Not managed by KXMLGUI. Actions are added programmatically and do
/// not appear in Settings → Configure Toolbars.
class RibbonToolBar : public KToolBar {
    Q_OBJECT

public:
    using Handle = QString;

    explicit RibbonToolBar(QWidget *parent = nullptr);
    explicit RibbonToolBar(const QString &objectName, QMainWindow *parent);

    /// Returns `id` on success, empty Handle if `id` is empty or already
    /// registered (Obsidian title-collision quirk, preserved).
    Handle addRibbonIcon(const Handle &id,
                         const QIcon &icon,
                         const QString &title,
                         std::function<void()> onActivated);

    bool removeRibbonIcon(const Handle &id);
    int iconCount() const;
    bool hasIcon(const Handle &id) const;

    QStringList iconIdsInOrder() const;

    void setIconVisible(const Handle &id, bool visible);
    bool isIconVisible(const Handle &id) const;

    /// Accessor for tests and for plugin code that needs to wire further
    /// behaviour (e.g. keyboard shortcut) onto the generated QAction.
    QAction *actionForId(const Handle &id) const;

Q_SIGNALS:
    void iconAdded(const QString &id);
    void iconRemoved(const QString &id);
    void iconVisibilityChanged(const QString &id, bool visible);

private:
    void commonInit();

    QHash<QString, QAction *> m_actions;
    QStringList m_order;
};

} // namespace Corbomite
