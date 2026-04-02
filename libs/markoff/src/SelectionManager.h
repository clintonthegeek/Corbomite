// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_SELECTIONMANAGER_H
#define MARKOFF_SELECTIONMANAGER_H

#include <QObject>
#include <QList>
#include <QPointF>

class QGraphicsSceneMouseEvent;
class QKeyEvent;
class QMimeData;

namespace Markoff {

class SelectableItem;

enum class SelectionMode { None, WithinItem, CrossBoundary };

class SelectionManager : public QObject {
    Q_OBJECT
public:
    explicit SelectionManager(QObject *parent = nullptr);

    /// Set the ordered list of items in the scene (top to bottom by Y).
    void setItems(const QList<SelectableItem *> &items);

    /// Mouse event handlers. Return true if the event was consumed
    /// (caller should NOT call the base class).
    bool handleMousePress(const QPointF &scenePos, Qt::KeyboardModifiers modifiers);
    bool handleMouseMove(const QPointF &scenePos);
    bool handleMouseRelease(const QPointF &scenePos);

    /// Key event handler for Ctrl+C, Ctrl+A, Escape.
    bool handleKeyPress(QKeyEvent *event);

    /// Create MIME data from current selection for clipboard.
    QMimeData *createMimeData() const;

    /// Clear all selection state across all items.
    void clearSelection();

    /// Whether there is any active cross-boundary selection.
    bool hasSelection() const;

    /// Current mode (for testing and UI feedback).
    SelectionMode mode() const { return m_mode; }

Q_SIGNALS:
    void modeChanged(Markoff::SelectionMode mode);

private:
    void setMode(SelectionMode mode);

    void applySelection();
    SelectableItem *itemAt(const QPointF &scenePos) const;
    QString serializeAsMarkdown() const;

    SelectionMode m_mode = SelectionMode::None;

    SelectableItem *m_anchorItem = nullptr;
    int m_anchorTextPos = -1;

    SelectableItem *m_currentItem = nullptr;
    int m_currentTextPos = -1;

    QList<SelectableItem *> m_items;
};

} // namespace Markoff

#endif // MARKOFF_SELECTIONMANAGER_H
