// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_ATOMICBLOCK_H
#define MARKOFF_ATOMICBLOCK_H

#include <QObject>
#include <QRectF>
#include <QSizeF>
#include <QString>

class QPainter;
class QKeyEvent;
class QMouseEvent;
class QContextMenuEvent;
class QWidget;

namespace Markoff {

/// Base class for blocks that render as interactive graphical units
/// in the editor (code blocks, tables, callouts, math, etc.).
///
/// An atomic block:
/// - Occupies a range of QTextDocument lines (the raw markdown)
/// - Renders as a graphical widget instead of raw text
/// - Captures keyboard/mouse input when focused
/// - Serializes changes back to the underlying markdown
///
/// See docs/07-atomic-blocks-and-tables.md for the full pattern.
class AtomicBlock : public QObject {
    Q_OBJECT
public:
    explicit AtomicBlock(QObject *parent = nullptr);
    ~AtomicBlock() override;

    /// Source range in the QTextDocument (first and last block numbers)
    int firstBlock() const { return m_firstBlock; }
    int lastBlock() const { return m_lastBlock; }
    void setBlockRange(int first, int last);

    /// Whether this block currently has focus (captures input)
    bool isFocused() const { return m_focused; }

    /// Compute the rendered size given available width
    virtual QSizeF sizeForWidth(qreal width) const = 0;

    /// Paint the block
    virtual void paint(QPainter *painter, const QRectF &rect) const = 0;

    /// Input handling — return true if the event was consumed
    virtual bool handleKeyPress(QKeyEvent *event);
    virtual bool handleMousePress(QMouseEvent *event, const QPointF &localPos);
    virtual bool handleMouseMove(QMouseEvent *event, const QPointF &localPos);
    virtual bool handleContextMenu(QContextMenuEvent *event, const QPointF &localPos);

    /// Base font size (set by Editor to match user's font size setting)
    void setBaseFontSize(int pointSize) { m_baseFontSize = pointSize; }
    int baseFontSize() const { return m_baseFontSize; }

    /// Focus management
    virtual void enterBlock(int cursorPosition);
    virtual void leaveBlock();

    /// The cursor offset within the block's content, set by enterBlock
    int cursorOffset() const { return m_cursorOffset; }

Q_SIGNALS:
    /// Emitted when the block's rendered content changed (needs repaint)
    void contentChanged();

    /// Emitted when the block wants to modify the underlying markdown
    void markdownChanged(const QString &newMarkdown);

protected:
    int m_firstBlock = -1;
    int m_lastBlock = -1;
    bool m_focused = false;
    int m_baseFontSize = 14;
    int m_cursorOffset = 0;
};

} // namespace Markoff

#endif // MARKOFF_ATOMICBLOCK_H
