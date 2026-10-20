// src/bases/BasesViewActions.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/ViewActions.h"

#include <QMetaObject>

namespace Corbomite::Bases {
class BasesView;
}

namespace Corbomite {

class View;

/// Bases' `ViewActions` provider.
///
/// The seven bases toolbar controls (Properties, Sort+group, Views, Filters,
/// Properties pane, +New, Results) were bare `QToolButton`s whose entire
/// behaviour lived in `clicked` lambdas inside the ctor. So none of them
/// appeared in the menubar, the command palette, or the Hotkeys page, and
/// none could be triggered without the pointer.
///
/// The in-view toolbar keeps its exact current appearance — the buttons
/// become plain `setDefaultAction()` carriers ("one definition, four
/// surfaces"). Anchoring still works because the buttons survive; a menu- or
/// palette-triggered action shows the same panel under the same button.
///
/// The Bases menu itself merges just after Edit (`append="view"`), so it
/// disappears with the tab rather than greying out, matching the Canvas menu's
/// precedent.
/// "Results" is a container action owning the Copy-table/Export-CSV menu,
/// mirroring the RE's `Results menu ($X)` — `bases.md` §1 records it as
/// also holding a limit input and "Show all", which Corbomite's bases table
/// has no equivalent for (no per-view row limit is parsed yet), so those two
/// are deliberately absent rather than stubbed.
class BasesViewActions : public ViewActions
{
    Q_OBJECT
public:
    explicit BasesViewActions(QObject *parent = nullptr);

    QString viewType() const override;
    void bind(View *view) override;
    void unbind() override;
    void refresh() override;
    QList<QAction *> toolBarActions() const override;

private:
    void setupActions();
    /// Hand our actions to the bound view's toolbar buttons, and
    /// take them back on unbind. Symmetric pair: every adopted slot is
    /// detached again, so a view can never keep a pointer into a provider
    /// that is being destroyed.
    void adoptActions();
    void releaseActions();
    Bases::BasesView *bases() const { return m_boundView; }

    Bases::BasesView *m_boundView = nullptr;
    QMetaObject::Connection m_contextConnection;
};

} // namespace Corbomite