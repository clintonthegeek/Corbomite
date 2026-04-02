// SPDX-License-Identifier: GPL-3.0-or-later
#include "SelectionManager.h"
#include "SelectableItem.h"

#include <QKeyEvent>
#include <QMimeData>
#include <QGraphicsItem>

namespace Markoff {

SelectionManager::SelectionManager(QObject *parent)
    : QObject(parent)
{
}

void SelectionManager::setItems(const QList<SelectableItem *> &items)
{
    m_items = items;
}

bool SelectionManager::handleMousePress(const QPointF &scenePos,
                                        Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers);
    m_anchorItem = itemAt(scenePos);
    if (!m_anchorItem) {
        m_mode = SelectionMode::None;
        return false;
    }
    m_anchorTextPos = m_anchorItem->hitTest(scenePos);
    m_mode = SelectionMode::WithinItem;
    return false; // let Qt handle the press normally
}

bool SelectionManager::handleMouseMove(const QPointF &scenePos)
{
    if (m_mode == SelectionMode::None)
        return false;

    SelectableItem *hoverItem = itemAt(scenePos);

    if (m_mode == SelectionMode::WithinItem) {
        if (!m_anchorItem)
            return false;
        // Check if we've left the anchor item
        QGraphicsItem *gi = m_anchorItem->asGraphicsItem();
        if (gi->boundingRect().contains(gi->mapFromScene(scenePos)))
            return false; // still within item, let Qt handle
        // Transition to CrossBoundary
        m_mode = SelectionMode::CrossBoundary;
    }

    // CrossBoundary mode
    m_currentItem = hoverItem ? hoverItem : m_anchorItem;
    m_currentTextPos = m_currentItem->hitTest(scenePos);
    applySelection();
    return true; // consumed — don't call base class
}

bool SelectionManager::handleMouseRelease(const QPointF &scenePos)
{
    Q_UNUSED(scenePos);
    if (m_mode == SelectionMode::CrossBoundary) {
        m_mode = SelectionMode::None;
        return true; // consumed
    }
    m_mode = SelectionMode::None;
    return false;
}

bool SelectionManager::handleKeyPress(QKeyEvent *event)
{
    Q_UNUSED(event);
    return false; // stubbed — implemented in Task 7
}

QMimeData *SelectionManager::createMimeData() const
{
    auto *data = new QMimeData;
    data->setText(serializeAsMarkdown());
    return data;
}

void SelectionManager::clearSelection()
{
    for (auto *item : m_items) {
        if (item->isTextItem())
            item->clearSelection();
        else
            item->setFullySelected(false);
    }
    m_anchorItem = nullptr;
    m_currentItem = nullptr;
    m_anchorTextPos = -1;
    m_currentTextPos = -1;
    m_mode = SelectionMode::None;
}

bool SelectionManager::hasSelection() const
{
    return m_mode == SelectionMode::CrossBoundary && m_anchorItem && m_currentItem;
}

void SelectionManager::applySelection()
{
    // Stubbed — implemented in Task 6
}

SelectableItem *SelectionManager::itemAt(const QPointF &scenePos) const
{
    for (auto *item : m_items) {
        QGraphicsItem *gi = item->asGraphicsItem();
        if (gi->sceneBoundingRect().contains(scenePos))
            return item;
    }
    // Fallback: find nearest item by Y
    if (m_items.isEmpty())
        return nullptr;
    if (scenePos.y() <= m_items.first()->asGraphicsItem()->sceneBoundingRect().top())
        return m_items.first();
    return m_items.last();
}

QString SelectionManager::serializeAsMarkdown() const
{
    return {}; // stubbed — implemented in Task 7
}

} // namespace Markoff
