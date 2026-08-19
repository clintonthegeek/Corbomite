// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>
#include "CanvasNodeItem.h"
#include "corbomite/core/RenderedDocument.h"

namespace Canvas {

class FileCardItem : public CanvasNodeItem {
    Q_OBJECT

public:
    FileCardItem(const CanvasNode &data, QGraphicsItem *parent = nullptr);

    void setRenderedDocument(std::unique_ptr<Corbomite::RenderedDocument> doc);

    /// Test/introspection accessor: true once a rendered document with a backing
    /// QTextDocument has been set on this card.
    bool hasRenderedDocument() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

Q_SIGNALS:
    void refreshRequested();

private:
    QString displayTitle() const;

    std::unique_ptr<Corbomite::RenderedDocument> m_renderedDoc;
};

} // namespace Canvas
