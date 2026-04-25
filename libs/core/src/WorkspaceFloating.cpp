// libs/core/src/WorkspaceFloating.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceFloating.h"

#include "corbomite/core/WorkspaceWindow.h"

namespace Corbomite {

WorkspaceFloating::WorkspaceFloating(QObject *parent)
    : QObject(parent)
{
}

void WorkspaceFloating::addWindow(WorkspaceWindow *window)
{
    if (!window || m_windows.contains(window))
        return;
    m_windows.append(window);
    Q_EMIT windowAdded(window);
}

void WorkspaceFloating::removeWindow(WorkspaceWindow *window)
{
    if (!window || !m_windows.removeOne(window))
        return;
    Q_EMIT windowRemoved(window);
}

} // namespace Corbomite
