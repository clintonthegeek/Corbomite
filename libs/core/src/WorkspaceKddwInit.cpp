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
                 | KDDockWidgets::Config::Flag_TabsHaveCloseButton);
}

} // namespace Corbomite::detail
