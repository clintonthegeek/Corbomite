// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include "CanvasNodeItem.h"
#include "corbomite/core/RenderedDocument.h"

namespace Corbomite::Canvas {

class FileCardItem : public CanvasNodeItem {
    Q_OBJECT

public:
    FileCardItem(const CanvasNode &data, QGraphicsItem *parent = nullptr);

    void setRenderedDocument(std::unique_ptr<Corbomite::RenderedDocument> doc);

    /// Test/introspection accessor: true once a rendered document with a backing
    /// QTextDocument has been set on this card.
    bool hasRenderedDocument() const;

    // ---- missing-file placeholder ----
    //
    // A file card whose `file` does not resolve shows a "File not found:
    // <path>" message plus Create / Locate… / Remove buttons instead of a
    // blank card. CanvasScene drives this via setMissing(); the buttons are
    // hit-tested here and routed back to CanvasScene.
    void setMissing(bool missing);
    bool isMissing() const { return m_missing; }
    /// The vault-relative path the placeholder names.
    QString missingPath() const { return m_data.file; }

    enum Button { CreateButton = 0, LocateButton = 1, RemoveButton = 2 };
    /// Item-coordinate rect of button `index`, or a null rect when the card
    /// is not missing or `index` is out of range.
    QRectF buttonRect(int index) const;
    /// Button under `itemPos` (item coordinates), or -1.
    int buttonAt(const QPointF &itemPos) const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

Q_SIGNALS:
    void refreshRequested();

private:
    QString displayTitle() const;
    QVector<QRectF> buttonRects() const;
    static QStringList buttonLabels();

    std::unique_ptr<Corbomite::RenderedDocument> m_renderedDoc;
    bool m_missing = false;
};

} // namespace Corbomite::Canvas
