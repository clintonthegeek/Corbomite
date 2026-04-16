// libs/core/include/corbomite/core/WorkspaceParent.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/WorkspaceItem.h"
#include <QVector>

namespace Corbomite {

class WorkspaceParent : public WorkspaceItem
{
    Q_OBJECT
public:
    explicit WorkspaceParent(QObject *parent = nullptr);

    int childCount() const;
    WorkspaceItem *childAt(int index) const;
    int indexOf(WorkspaceItem *child) const;
    QVector<WorkspaceItem *> children() const;

    void addChild(WorkspaceItem *child, int index = -1);
    void removeChild(WorkspaceItem *child, bool deleteChild = false);
    void moveChild(int from, int to);

Q_SIGNALS:
    void childAdded(WorkspaceItem *child, int index);
    void childRemoved(WorkspaceItem *child);

protected:
    QVector<WorkspaceItem *> m_children;
};

} // namespace Corbomite
