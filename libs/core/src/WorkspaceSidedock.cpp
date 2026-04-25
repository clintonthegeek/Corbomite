// libs/core/src/WorkspaceSidedock.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceSidedock.h"

namespace Corbomite {

WorkspaceSidedock::WorkspaceSidedock(QString id, Side side, QObject *parent)
    : WorkspaceContainer(std::move(id),
                          QStringLiteral("vertical"),
                          parent)
    , m_side(side)
{
}

void WorkspaceSidedock::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed)
        return;
    m_collapsed = collapsed;
    Q_EMIT collapsedChanged(m_collapsed);
}

void WorkspaceSidedock::setSize(int size)
{
    if (m_size == size)
        return;
    m_size = size;
    Q_EMIT sizeChanged(m_size);
}

} // namespace Corbomite
