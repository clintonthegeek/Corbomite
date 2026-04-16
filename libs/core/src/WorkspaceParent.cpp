// libs/core/src/WorkspaceParent.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceParent.h"

namespace Corbomite {

WorkspaceParent::WorkspaceParent(QObject *parent)
    : WorkspaceItem(parent)
{
}

int WorkspaceParent::childCount() const { return m_children.size(); }

WorkspaceItem *WorkspaceParent::childAt(int index) const
{
    if (index < 0 || index >= m_children.size())
        return nullptr;
    return m_children.at(index);
}

int WorkspaceParent::indexOf(WorkspaceItem *child) const
{
    return m_children.indexOf(child);
}

QVector<WorkspaceItem *> WorkspaceParent::children() const { return m_children; }

void WorkspaceParent::addChild(WorkspaceItem *child, int index)
{
    if (!child || m_children.contains(child))
        return;

    if (child->parentItem())
        child->parentItem()->removeChild(child);

    child->setParentItem(this);
    if (index < 0 || index >= m_children.size())
        m_children.append(child);
    else
        m_children.insert(index, child);

    Q_EMIT childAdded(child, m_children.indexOf(child));
}

void WorkspaceParent::removeChild(WorkspaceItem *child, bool deleteChild)
{
    if (!child || !m_children.contains(child))
        return;

    m_children.removeOne(child);
    child->setParentItem(nullptr);
    Q_EMIT childRemoved(child);

    if (deleteChild)
        delete child;
}

void WorkspaceParent::moveChild(int from, int to)
{
    if (from < 0 || from >= m_children.size() ||
        to < 0 || to >= m_children.size() || from == to)
        return;

    auto *child = m_children.takeAt(from);
    m_children.insert(to, child);
}

} // namespace Corbomite
