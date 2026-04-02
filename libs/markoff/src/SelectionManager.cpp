// SPDX-License-Identifier: GPL-3.0-or-later
#include "SelectionManager.h"
#include "SelectableItem.h"

#include <QClipboard>
#include <QGuiApplication>
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
    SelectableItem *pressedItem = itemAt(scenePos);

    // Shift+Click: extend from existing anchor to new position
    if (modifiers & Qt::ShiftModifier && m_anchorItem) {
        m_currentItem = pressedItem ? pressedItem : m_anchorItem;
        m_currentTextPos = m_currentItem->hitTest(scenePos);
        m_mode = SelectionMode::CrossBoundary;
        applySelection();
        return true; // consumed — we handle this entirely
    }

    // Click without Shift: clear any existing cross-boundary selection
    if (m_mode == SelectionMode::CrossBoundary || hasSelection()) {
        clearSelection();
    }

    m_anchorItem = pressedItem;
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
    // Ctrl+A: select all
    if (event->key() == Qt::Key_A && event->modifiers() == Qt::ControlModifier) {
        if (m_items.isEmpty())
            return false;
        m_anchorItem = m_items.first();
        m_anchorTextPos = 0;
        m_currentItem = m_items.last();
        m_currentTextPos = m_currentItem->isTextItem()
            ? m_currentItem->allMarkdown().length()
            : -1;
        m_mode = SelectionMode::CrossBoundary;
        applySelection();
        return true;
    }

    // Escape: clear selection
    if (event->key() == Qt::Key_Escape && m_mode == SelectionMode::CrossBoundary) {
        clearSelection();
        return true;
    }

    // Ctrl+C: copy
    if (event->key() == Qt::Key_C && event->modifiers() == Qt::ControlModifier
        && m_mode == SelectionMode::CrossBoundary) {
        QMimeData *data = createMimeData();
        QGuiApplication::clipboard()->setMimeData(data);
        return true;
    }

    return false;
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
    if (m_mode == SelectionMode::CrossBoundary && m_anchorItem && m_currentItem)
        return true;
    for (auto *item : m_items) {
        if (item->isTextItem() && !item->selectedMarkdown().isEmpty())
            return true;
        if (!item->isTextItem() && item->isFullySelected())
            return true;
    }
    return false;
}

void SelectionManager::applySelection()
{
    if (!m_anchorItem || !m_currentItem)
        return;

    int anchorIdx = m_items.indexOf(m_anchorItem);
    int currentIdx = m_items.indexOf(m_currentItem);
    if (anchorIdx < 0 || currentIdx < 0)
        return;

    bool forward = currentIdx >= anchorIdx;
    int lo = qMin(anchorIdx, currentIdx);
    int hi = qMax(anchorIdx, currentIdx);

    for (int i = 0; i < m_items.size(); ++i) {
        SelectableItem *item = m_items[i];

        if (i < lo || i > hi) {
            if (item->isTextItem())
                item->clearSelection();
            else
                item->setFullySelected(false);
        } else if (i == anchorIdx && i == currentIdx) {
            if (item->isTextItem())
                item->setSelection(m_anchorTextPos, m_currentTextPos);
            else
                item->setFullySelected(true);
        } else if (i == anchorIdx) {
            if (item->isTextItem()) {
                int end = item->allMarkdown().length();
                if (forward)
                    item->setSelection(m_anchorTextPos, end);
                else
                    item->setSelection(m_anchorTextPos, 0);
            } else {
                item->setFullySelected(true);
            }
        } else if (i == currentIdx) {
            if (item->isTextItem()) {
                int end = item->allMarkdown().length();
                if (forward)
                    item->setSelection(0, m_currentTextPos);
                else
                    item->setSelection(end, m_currentTextPos);
            } else {
                item->setFullySelected(true);
            }
        } else {
            if (item->isTextItem()) {
                int end = item->allMarkdown().length();
                item->setSelection(0, end);
            } else {
                item->setFullySelected(true);
            }
        }
    }
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
    if (!m_anchorItem || !m_currentItem)
        return {};

    int anchorIdx = m_items.indexOf(m_anchorItem);
    int currentIdx = m_items.indexOf(m_currentItem);
    if (anchorIdx < 0 || currentIdx < 0)
        return {};

    int lo = qMin(anchorIdx, currentIdx);
    int hi = qMax(anchorIdx, currentIdx);
    QString result;

    for (int i = lo; i <= hi; ++i) {
        SelectableItem *item = m_items[i];
        if (i == anchorIdx || i == currentIdx) {
            if (item->isTextItem())
                result += item->selectedMarkdown();
            else
                result += item->toMarkdown();
        } else {
            if (item->isTextItem())
                result += item->allMarkdown();
            else
                result += item->toMarkdown();
        }
    }
    return result;
}

} // namespace Markoff
