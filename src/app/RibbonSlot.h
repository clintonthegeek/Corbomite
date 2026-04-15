// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QHash>
#include <QIcon>
#include <QString>
#include <QWidget>

class KToolBar;
class QAction;

namespace Corbomite {

// Cluster H Phase 4 — Obsidian's vertical "ribbon" plugin slot, backed by
// a left-docked vertical KToolBar with icon-only buttons.
//
// Per docs/obsidian-audit/02-extension-surfaces.md "addRibbonIcon":
// addRibbonIcon **keys on `title`, NOT on `icon`** — same-title collision is
// silent and the second registration is dropped. This mirrors Obsidian's
// behaviour (technically a bug, preserved as a compat invariant).
//
// The widget is self-contained and has no app-state dependencies; callers
// add ribbon entries by `addRibbonIcon(icon, title, callback)` and unregister
// via the returned Handle. Plugin code (Cluster N) will use this surface
// directly.
class RibbonSlot : public QWidget {
    Q_OBJECT

public:
    using Handle = QString;  // == the title — opaque to callers

    explicit RibbonSlot(QWidget *parent = nullptr);

    // Returns Handle (== title) on success; empty Handle if a ribbon entry
    // with the same title is already registered.
    Handle addRibbonIcon(const QIcon &icon,
                          const QString &title,
                          std::function<void()> onActivated);

    bool removeRibbonIcon(const Handle &handle);

    // Number of currently-registered ribbon entries (test/inspection).
    int iconCount() const;

    bool hasIcon(const Handle &handle) const;

private:
    KToolBar *m_toolbar;
    QHash<QString, QAction *> m_actions;
};

} // namespace Corbomite
