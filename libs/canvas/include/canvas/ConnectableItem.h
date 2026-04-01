// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointF>
#include <QString>
#include "CanvasTypes.h"

class QGraphicsObject;

namespace Canvas {

class ConnectableItem {
public:
    virtual ~ConnectableItem() = default;
    virtual QPointF connectionPoint(Side side) const = 0;
    virtual QString nodeId() const = 0;
    virtual QGraphicsObject *asGraphicsObject() = 0;
};

} // namespace Canvas
