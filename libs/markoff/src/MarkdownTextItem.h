// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_MARKDOWNTEXTITEM_H
#define MARKOFF_MARKDOWNTEXTITEM_H

#include "SelectableItem.h"
#include <QGraphicsObject>

class QTextDocument;

namespace Markoff {

class TextControl;

/// Editable markdown text region in the graphics scene.
/// Wraps TextControl + QTextDocument. Implements SelectableItem
/// for text-level selection operations.
class MarkdownTextItem : public QGraphicsObject, public SelectableItem {
    Q_OBJECT
public:
    explicit MarkdownTextItem(QGraphicsItem *parent = nullptr);
    ~MarkdownTextItem() override;

    /// Set the raw markdown text content.
    void setPlainText(const QString &text);

    /// Access the underlying document and control.
    QTextDocument *document() const;
    TextControl *textControl() const { return m_control; }

    // QGraphicsItem
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    // SelectableItem
    QGraphicsItem *asGraphicsItem() override { return this; }
    bool isTextItem() const override { return true; }
    int hitTest(const QPointF &scenePos) const override;
    void setSelection(int anchorPos, int cursorPos) override;
    void clearSelection() override;
    QString selectedMarkdown() const override;
    QString allMarkdown() const override;
    QString toMarkdown() const override;

Q_SIGNALS:
    /// Emitted when Shift+Arrow can't move further.
    void cursorAtBoundary(Qt::Edge edge);

private:
    void updateGeometry();

    TextControl *m_control = nullptr;
    QTextDocument *m_document = nullptr;
    qreal m_width = 600.0;
};

} // namespace Markoff

#endif // MARKOFF_MARKDOWNTEXTITEM_H
