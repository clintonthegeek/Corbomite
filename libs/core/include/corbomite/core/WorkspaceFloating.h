// libs/core/include/corbomite/core/WorkspaceFloating.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>

namespace Corbomite {

class WorkspaceWindow;

/// Container for popout windows. Holds the list of `WorkspaceWindow*`
/// in `floating.children` order, matching the Obsidian
/// `workspace.floatingSplit` shape. Cluster Y Phase 7.5 ships this as a
/// thin facade over the existing `Workspace::windows()` list — Phase 5
/// already owns the popout lifecycle, this object just exposes that
/// list under the Obsidian-shape accessor.
class WorkspaceFloating : public QObject
{
    Q_OBJECT
public:
    explicit WorkspaceFloating(QObject *parent = nullptr);

    QList<WorkspaceWindow *> windows() const { return m_windows; }
    void addWindow(WorkspaceWindow *window);
    void removeWindow(WorkspaceWindow *window);

Q_SIGNALS:
    void windowAdded(WorkspaceWindow *window);
    void windowRemoved(WorkspaceWindow *window);

private:
    QList<WorkspaceWindow *> m_windows;
};

} // namespace Corbomite
