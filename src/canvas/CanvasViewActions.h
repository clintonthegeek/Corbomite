// src/canvas/CanvasViewActions.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/ViewActions.h"

#include <QMetaObject>

namespace Canvas {
class CanvasScene;
}

namespace Corbomite {

class CanvasFileView;
class View;

/// Cluster O Phase O4 — canvas's `ViewActions` provider. Closes punch-list
/// `[ui-bundle][canvas][P2][cluster-o]`: snap-to-grid, snap-to-objects, and
/// grid-visibility had no KActions anywhere, and canvas zoom had no
/// type-specific menu/toolbar home despite the O1.T3 virtuals already
/// existing on `CanvasFileView`.
///
/// The three toggles are app-wide (corbomite.kcfg's `Canvas` group, per
/// doctrine §C3's "app-wide, survives restart" row), not per-document —
/// toggling one fans out to every open canvas via
/// `MainWindow::applyCanvasSettings()` on `CorbomiteSettings::configChanged`,
/// not just the focused tab.
class CanvasViewActions : public ViewActions
{
    Q_OBJECT
public:
    explicit CanvasViewActions(QObject *parent = nullptr);

    QString viewType() const override;
    void bind(View *view) override;
    void unbind() override;
    void refresh() override;
    QList<QAction *> toolBarActions() const override;

private:
    void setupActions();
    CanvasFileView *canvas() const { return m_boundView; }

    /// M5.5 — the bound canvas's scene, or null.
    Canvas::CanvasScene *canvasScene() const;

    /// M5.5 — Obsidian's `canvas:jump-to-group` needs at least one group to
    /// list (`app/own/2028-n9.js:673`), and `canvas:convert-to-file` needs
    /// exactly one selected text node (`:686-688`).
    bool canvasHasGroups() const;
    QString selectedTextNodeId() const;

    CanvasFileView *m_boundView = nullptr;
    QMetaObject::Connection m_contextConnection;
};

} // namespace Corbomite
