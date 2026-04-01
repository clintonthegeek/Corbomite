// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_CALLOUTATOMICBLOCK_H
#define MARKOFF_CALLOUTATOMICBLOCK_H

#include "AtomicBlock.h"
#include <QPixmap>
#include <QColor>

namespace Markoff {

/// A callout block rendered as an interactive graphical unit.
///
/// Appearance:
/// - Colored left border (4px, type-specific color)
/// - Faint type-specific background
/// - Type label in bold (e.g., "Warning", "Note", "Tip")
/// - Body text rendered below the label
/// - Fold toggle indicator if foldable (+/-)
///
/// Interaction:
/// - Click to focus
/// - Escape to leave
/// - Backspace from outside selects the whole block
class CalloutAtomicBlock : public AtomicBlock {
    Q_OBJECT
public:
    explicit CalloutAtomicBlock(QObject *parent = nullptr);

    void setCallout(const QString &type, const QString &title,
                    const QString &body, bool foldable, bool collapsed);

    QString calloutType() const { return m_type; }
    QString title() const { return m_title; }
    QString body() const { return m_body; }

    // AtomicBlock interface
    QSizeF sizeForWidth(qreal width) const override;
    void paint(QPainter *painter, const QRectF &rect) const override;
    bool handleKeyPress(QKeyEvent *event) override;
    bool handleMousePress(QMouseEvent *event, const QPointF &localPos) override;

    /// Get the accent color for a callout type
    static QColor colorForType(const QString &type);
    /// Get a display title for a callout type (if no custom title)
    static QString defaultTitle(const QString &type);

private:
    void invalidateCache();
    void rebuildCache(qreal width) const;

    QString m_type;
    QString m_title;
    QString m_body;
    bool m_foldable = false;
    bool m_collapsed = false;

    // Cache
    mutable QPixmap m_cache;
    mutable qreal m_cachedWidth = -1;
    mutable QSizeF m_cachedSize;
};

} // namespace Markoff

#endif // MARKOFF_CALLOUTATOMICBLOCK_H
