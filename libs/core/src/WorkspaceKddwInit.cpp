// libs/core/src/WorkspaceKddwInit.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "WorkspaceKddwInit.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/KDDockWidgets.h>

namespace Corbomite::detail {

void ensureKddwInit()
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);
    auto &cfg = KDDockWidgets::Config::self();
    cfg.setFlags(cfg.flags()
                 | KDDockWidgets::Config::Flag_AlwaysShowTabs
                 | KDDockWidgets::Config::Flag_AllowReorderTabs
                 | KDDockWidgets::Config::Flag_TabsHaveCloseButton
                 // Cluster L Phase L4 (D1): with AlwaysShowTabs set, every
                 // group was rendering both its own title bar *and* the tab
                 // bar underneath it — a redundant chrome row KDE apps
                 // (Kate/Dolphin split views) don't have. HideTitleBarWhenTabsVisible
                 // collapses that into just the tab bar, dragging the empty
                 // tab-bar space to move the group instead.
                 | KDDockWidgets::Config::Flag_HideTitleBarWhenTabsVisible
                 // Without a title bar, the close/float/maximize buttons it
                 // carried would vanish entirely; this flag re-homes them
                 // onto the tab bar itself so they stay reachable.
                 | KDDockWidgets::Config::Flag_ShowButtonsOnTabBarIfTitleBarHidden);
}

} // namespace Corbomite::detail
