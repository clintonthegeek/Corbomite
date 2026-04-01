// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_BLOCKDATA_H
#define MARKOFF_BLOCKDATA_H

#include <QTextBlockUserData>
#include <QPixmap>

namespace Markoff {

class AtomicBlock;

class MarkoffBlockData : public QTextBlockUserData {
public:
    enum DisplayMode { Raw, Rendered };

    DisplayMode displayMode = Raw;
    int renderedHeight = -1;
    QPixmap renderedCache;
    bool cacheValid = false;

    /// If non-null, this text block is part of an atomic block.
    /// The atomic block is owned by the Editor, not by this data object.
    AtomicBlock *atomicBlock = nullptr;

    /// True if this is the first text block of the atomic block
    /// (used to know which block to paint the atomic block at)
    bool isAtomicBlockStart = false;
};

} // namespace Markoff
#endif
