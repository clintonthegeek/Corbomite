// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/PaneLayoutBridge.h"

#include <QSplitter>
#include <QWidget>

namespace Corbomite {
namespace PaneLayoutBridge {

namespace {

// --- Serialise ---

void populateIndexFromWidget(
    PaneLayoutIndex *index,
    QWidget *child,
    const std::function<QList<PaneLeaf>(QWidget *)> &spaceToLeaves,
    QWidget *activeSpace,
    QString *outActiveLeafId);

void populateIndexFromSplitter(
    PaneLayoutIndex *index,
    QSplitter *splitter,
    const std::function<QList<PaneLeaf>(QWidget *)> &spaceToLeaves,
    QWidget *activeSpace,
    QString *outActiveLeafId)
{
    const int count = splitter->count();
    if (count == 0) return;

    index->setOrientation(splitter->orientation());

    // Capture relative sizes as Obsidian dimensions (ratios 0-100 across
    // siblings). Note: QSplitter sizes() returns zeros for not-yet-shown
    // widgets; in that case we leave dimension unset (= flexible).
    const QList<int> sizes = splitter->sizes();
    int total = 0;
    for (int s : sizes) total += std::max(0, s);

    // Flatten to binary form: first child direct, rest chained into second.
    if (count == 1) {
        // Degenerate splitter with a single child — collapse into this index.
        populateIndexFromWidget(index, splitter->widget(0),
                                spaceToLeaves, activeSpace, outActiveLeafId);
        return;
    }

    // Two or more: build a binary chain.
    auto first = std::make_unique<PaneLayoutIndex>(index);
    populateIndexFromWidget(first.get(), splitter->widget(0),
                            spaceToLeaves, activeSpace, outActiveLeafId);
    if (total > 0) {
        first->setDimension(sizes[0] * 100.0 / total);
    }
    index->setFirst(std::move(first));

    // Chain remaining children into second.
    PaneLayoutIndex *cursor = index;
    for (int i = 1; i < count; ++i) {
        auto child = std::make_unique<PaneLayoutIndex>(cursor);
        populateIndexFromWidget(child.get(), splitter->widget(i),
                                spaceToLeaves, activeSpace, outActiveLeafId);
        if (total > 0) {
            child->setDimension(sizes[i] * 100.0 / total);
        }
        if (i == count - 1) {
            // Last child: put it directly as second.
            cursor->setSecond(std::move(child));
        } else {
            // Intermediate: wrap in a split node preserving orientation.
            auto wrap = std::make_unique<PaneLayoutIndex>(cursor);
            wrap->setOrientation(splitter->orientation());
            wrap->setFirst(std::move(child));
            auto *raw = wrap.get();
            cursor->setSecond(std::move(wrap));
            cursor = raw;
        }
    }
}

void populateIndexFromWidget(
    PaneLayoutIndex *index,
    QWidget *child,
    const std::function<QList<PaneLeaf>(QWidget *)> &spaceToLeaves,
    QWidget *activeSpace,
    QString *outActiveLeafId)
{
    if (auto *nested = qobject_cast<QSplitter *>(child)) {
        populateIndexFromSplitter(index, nested,
                                  spaceToLeaves, activeSpace, outActiveLeafId);
        return;
    }
    // Leaf pane widget.
    QList<PaneLeaf> leaves = spaceToLeaves ? spaceToLeaves(child) : QList<PaneLeaf>{};
    for (auto &leaf : leaves) {
        if (leaf.id.isEmpty()) leaf.id = PaneLayout::newId();
    }
    // Record the active-leaf id if this is the focused pane.
    if (child == activeSpace && !leaves.isEmpty() && outActiveLeafId) {
        // We don't know which tab is "active" from here — callers will either
        // already have set the right PaneLeaf.id to something the caller uses,
        // or we just default to the first leaf. Document behaviour: the
        // caller should put the active tab first in the list if it wants
        // to be the active leaf.
        *outActiveLeafId = leaves.first().id;
    }
    index->setViews(std::move(leaves));
}

// --- Deserialise ---

QWidget *realiseNode(
    const PaneLayoutIndex &node,
    QSplitter *parentSplitter,
    const std::function<QWidget *()> &createSpace,
    const std::function<void(QWidget *, const PaneLeaf &)> &openTab,
    QList<QWidget *> &created);

QSplitter *realiseSplit(
    const PaneLayoutIndex &node,
    QSplitter *parentSplitter,
    const std::function<QWidget *()> &createSpace,
    const std::function<void(QWidget *, const PaneLeaf &)> &openTab,
    QList<QWidget *> &created)
{
    QSplitter *splitter = nullptr;
    // Reuse the parent if it's empty and same orientation — matches the
    // old rebuildSplitLayout behaviour (avoids one level of nesting).
    if (parentSplitter->count() == 0
            && parentSplitter->orientation() == node.orientation()) {
        splitter = parentSplitter;
    } else {
        splitter = new QSplitter(node.orientation(), parentSplitter);
        parentSplitter->addWidget(splitter);
    }

    if (node.first())  realiseNode(*node.first(), splitter, createSpace, openTab, created);
    if (node.second()) realiseNode(*node.second(), splitter, createSpace, openTab, created);

    // Apply dimensions (convert ratios to QSplitter sizes). We defer to
    // Qt's own stretch behaviour when dimensions are absent.
    QList<double> ratios;
    auto collect = [&ratios](const PaneLayoutIndex *n) {
        ratios.append(n && n->dimension() ? *n->dimension() : 0.0);
    };
    collect(node.first());
    collect(node.second());
    const bool anyDimension = std::any_of(ratios.begin(), ratios.end(),
                                          [](double d) { return d > 0.0; });
    if (anyDimension && splitter->count() == ratios.size()) {
        // Convert ratios to integer sizes summing to some base; Qt will
        // clamp when widgets are shown.
        constexpr int base = 10000;
        QList<int> sizes;
        for (double r : ratios) sizes.append(static_cast<int>(r * base / 100.0));
        splitter->setSizes(sizes);
    }
    return splitter;
}

QWidget *realiseNode(
    const PaneLayoutIndex &node,
    QSplitter *parentSplitter,
    const std::function<QWidget *()> &createSpace,
    const std::function<void(QWidget *, const PaneLeaf &)> &openTab,
    QList<QWidget *> &created)
{
    if (node.isSplit()) {
        return realiseSplit(node, parentSplitter, createSpace, openTab, created);
    }
    QWidget *space = createSpace ? createSpace() : nullptr;
    if (!space) return nullptr;
    parentSplitter->addWidget(space);
    created.append(space);
    if (openTab) {
        for (const auto &leaf : node.views()) openTab(space, leaf);
    }
    return space;
}

} // namespace

PaneLayout serializeFromSplitter(
    QSplitter *root,
    const std::function<QList<PaneLeaf>(QWidget *)> &spaceToLeaves,
    QWidget *activeSpace)
{
    PaneLayout out;
    QString activeId;
    if (root) {
        populateIndexFromSplitter(out.root(), root,
                                  spaceToLeaves, activeSpace, &activeId);
    }
    if (!activeId.isEmpty()) out.setActiveLeafId(activeId);
    return out;
}

QList<QWidget *> deserializeIntoSplitter(
    const PaneLayout &layout,
    QSplitter *root,
    const std::function<QWidget *()> &createSpace,
    const std::function<void(QWidget *, const PaneLeaf &)> &openTab)
{
    QList<QWidget *> created;
    if (!root || !layout.root()) return created;

    // Walk the root layout node into the caller's splitter. If the root
    // is a leaf, materialise a single pane; if split, recurse.
    const PaneLayoutIndex &topNode = *layout.root();
    if (topNode.isSplit()) {
        // Align the provided splitter's orientation to match the layout's
        // root orientation so realiseSplit can reuse it directly.
        root->setOrientation(topNode.orientation());
        if (topNode.first())  realiseNode(*topNode.first(),  root, createSpace, openTab, created);
        if (topNode.second()) realiseNode(*topNode.second(), root, createSpace, openTab, created);

        QList<double> ratios;
        auto collect = [&ratios](const PaneLayoutIndex *n) {
            ratios.append(n && n->dimension() ? *n->dimension() : 0.0);
        };
        collect(topNode.first());
        collect(topNode.second());
        const bool anyDimension = std::any_of(ratios.begin(), ratios.end(),
                                              [](double d) { return d > 0.0; });
        if (anyDimension && root->count() == ratios.size()) {
            constexpr int base = 10000;
            QList<int> sizes;
            for (double r : ratios) sizes.append(static_cast<int>(r * base / 100.0));
            root->setSizes(sizes);
        }
    } else {
        realiseNode(topNode, root, createSpace, openTab, created);
    }
    return created;
}

} // namespace PaneLayoutBridge
} // namespace Corbomite
