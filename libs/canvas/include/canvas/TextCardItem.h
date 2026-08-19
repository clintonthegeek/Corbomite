// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>
#include "CanvasNodeItem.h"
#include "corbomite/core/RenderedDocument.h"

namespace Canvas {

class TextCardItem : public CanvasNodeItem {
    Q_OBJECT

public:
    TextCardItem(const CanvasNode &data, QGraphicsItem *parent = nullptr);

    void setRenderedDocument(std::unique_ptr<Corbomite::RenderedDocument> doc);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    std::unique_ptr<Corbomite::RenderedDocument> m_renderedDoc;
};

} // namespace Canvas
