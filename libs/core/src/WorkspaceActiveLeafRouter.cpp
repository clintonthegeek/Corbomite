// libs/core/src/WorkspaceActiveLeafRouter.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceActiveLeafRouter.h"

#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"

#include <QApplication>
#include <QWidget>

namespace Corbomite {

WorkspaceActiveLeafRouter::WorkspaceActiveLeafRouter(Workspace *workspace)
    : QObject(workspace)
    , m_workspace(workspace)
{
    if (auto *app = qApp) {
        connect(app, &QApplication::focusChanged,
                this, &WorkspaceActiveLeafRouter::onFocusChanged);
    }
}

void WorkspaceActiveLeafRouter::onFocusChanged(QWidget * /*previous*/, QWidget *current)
{
    if (!m_workspace || !current)
        return;

    // Walk the focused widget's parent chain. The first ancestor whose
    // QWidget* matches a leaf's dockWidget wins. setActiveLeaf handles
    // identity gating + layout-ready suppression on the Workspace side,
    // so the router can stay deliberately dumb.
    for (QWidget *w = current; w; w = w->parentWidget()) {
        for (auto *leaf : m_workspace->allLeaves()) {
            if (leaf->widget() == w) {
                m_workspace->setActiveLeaf(leaf);
                return;
            }
        }
    }
}

} // namespace Corbomite
