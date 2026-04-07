// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_MARKDOWNTEXTITEM_H
#define MARKOFF_MARKDOWNTEXTITEM_H

#include "SelectableItem.h"
#include "DecoratedRange.h"
#include <QGraphicsObject>

class QTextDocument;

namespace Markoff {

class TextControl;
class MathTextObject;

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

    /// Set the text width (for word wrap). Triggers relayout.
    void setTextWidth(qreal width);

    /// Decorated ranges detected in this item's document.
    const QList<DecoratedRange> &decoratedRanges() const { return m_decoratedRanges; }

    // QGraphicsItem
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

public:

    // SelectableItem
    QGraphicsItem *asGraphicsItem() override { return this; }
    bool isTextItem() const override { return true; }
    int hitTest(const QPointF &scenePos) const override;
    void setSelection(int anchorPos, int cursorPos) override;
    void clearSelection() override;
    QString selectedMarkdown() const override;
    QString allMarkdown() const override;
    QString toMarkdown() const override;

    /// Reapply inline-math glyph substitution based on the highlighter's
    /// current span map. Strips any existing math objects first, so this
    /// is also the right place to call after the underlying source text
    /// has been replaced.
    ///
    /// Mutates the document but is guarded so it does not retrigger
    /// reparses.
    void refreshMathSubstitution();

    /// Replace any existing math object characters with their stored
    /// source text, leaving the document in canonical source form.
    /// SceneCoordinator calls this between reparse and re-substitution
    /// so the highlighter span offsets line up with document positions.
    int stripMathSubstitution();

Q_SIGNALS:
    void textChanged();
    /// Emitted when arrow key can't move further.
    void cursorAtBoundary(Qt::Edge edge);

private:
    void updateGeometry();
    void onCursorPositionChanged();
    void snapCursorPastDelimiters();
    void detectDecoratedRanges();
    void paintDecoratedRanges(QPainter *painter);

    /// Walk the highlighter span map and replace each math span's
    /// source text range with U+FFFC + a math format. Must be called on
    /// a document that is in canonical source form (no leftover U+FFFC).
    void applyMathSubstitution();

    TextControl *m_control = nullptr;
    QTextDocument *m_document = nullptr;
    MathTextObject *m_mathObject = nullptr;
    qreal m_width = 600.0;
    bool m_snappingCursor = false;
    bool m_inMathSubstitution = false;
    QList<DecoratedRange> m_decoratedRanges;
};

} // namespace Markoff

#endif // MARKOFF_MARKDOWNTEXTITEM_H
