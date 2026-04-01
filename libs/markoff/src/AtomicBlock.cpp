// SPDX-License-Identifier: GPL-3.0-or-later
#include "AtomicBlock.h"

namespace Markoff {

AtomicBlock::AtomicBlock(QObject *parent)
    : QObject(parent)
{
}

AtomicBlock::~AtomicBlock() = default;

void AtomicBlock::setBlockRange(int first, int last)
{
    m_firstBlock = first;
    m_lastBlock = last;
}

bool AtomicBlock::handleKeyPress(QKeyEvent *)
{
    return false; // not consumed by default
}

bool AtomicBlock::handleMousePress(QMouseEvent *, const QPointF &)
{
    return false;
}

bool AtomicBlock::handleMouseMove(QMouseEvent *, const QPointF &)
{
    return false;
}

bool AtomicBlock::handleContextMenu(QContextMenuEvent *, const QPointF &)
{
    return false;
}

void AtomicBlock::enterBlock(int)
{
    m_focused = true;
}

void AtomicBlock::leaveBlock()
{
    m_focused = false;
}

} // namespace Markoff
