// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_CODEATOMICBLOCK_H
#define MARKOFF_CODEATOMICBLOCK_H

#include "AtomicBlock.h"
#include <QPixmap>
#include <QFont>
#include <QColor>

namespace Markoff {

/// A fenced code block rendered as an interactive graphical unit.
///
/// Appearance:
/// - Rounded rectangle with background tint
/// - Language label in the top-right corner
/// - Syntax-highlighted code content
/// - Line numbers along the left edge
///
/// Interaction:
/// - Click to focus (cursor enters the code block)
/// - Type to edit code content
/// - Escape to leave
/// - Backspace from outside selects the whole block
class CodeAtomicBlock : public AtomicBlock {
    Q_OBJECT
public:
    explicit CodeAtomicBlock(QObject *parent = nullptr);

    /// Set the code content and language
    void setCode(const QString &code, const QString &language);
    QString code() const { return m_code; }
    QString language() const { return m_language; }

    // AtomicBlock interface
    QSizeF sizeForWidth(qreal width) const override;
    void paint(QPainter *painter, const QRectF &rect) const override;
    bool handleKeyPress(QKeyEvent *event) override;
    bool handleMousePress(QMouseEvent *event, const QPointF &localPos) override;
    void enterBlock(int cursorPosition) override;
    void leaveBlock() override;

private:
    void invalidateCache();
    void rebuildCache(qreal width) const;

    QString m_code;
    QString m_language;

    // Rendering
    mutable QFont m_codeFont;
    QColor m_bgColor = QColor(0xf5, 0xf5, 0xf5);
    QColor m_borderColor = QColor(0xe0, 0xe0, 0xe0);
    QColor m_labelColor = QColor(0x9e, 0x9e, 0x9e);
    int m_padding = 8;
    int m_labelHeight = 20;
    int m_cornerRadius = 4;

    // Cache
    mutable QPixmap m_cache;
    mutable qreal m_cachedWidth = -1;
    mutable QSizeF m_cachedSize;

    // Editing state (when focused)
    int m_cursorLine = 0;
    int m_cursorCol = 0;
};

} // namespace Markoff

#endif // MARKOFF_CODEATOMICBLOCK_H
