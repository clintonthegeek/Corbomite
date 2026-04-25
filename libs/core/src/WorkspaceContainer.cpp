// libs/core/src/WorkspaceContainer.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceContainer.h"

namespace Corbomite {

WorkspaceContainer::WorkspaceContainer(QString id,
                                         QString direction,
                                         QObject *parent)
    : QObject(parent)
    , m_id(std::move(id))
    , m_direction(std::move(direction))
{
}

void WorkspaceContainer::setDirection(const QString &direction)
{
    if (m_direction == direction)
        return;
    m_direction = direction;
    Q_EMIT directionChanged(m_direction);
}

} // namespace Corbomite
