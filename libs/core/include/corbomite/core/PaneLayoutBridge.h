// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CORBOMITE_CORE_PANELAYOUTBRIDGE_H
#define CORBOMITE_CORE_PANELAYOUTBRIDGE_H

#include <functional>

#include <QList>

#include "corbomite/core/PaneLayout.h"

class QSplitter;
class QWidget;

namespace Corbomite {

/// Bridges a `PaneLayout` (the domain model) to an on-screen `QSplitter`
/// tree populated with opaque pane widgets.
///
/// The widget side is intentionally a plain `QWidget *` so this utility
/// works against any "pane widget" class (`EditorViewSpace` in the real
/// app, a simple `QWidget` in tests). Tab semantics are delegated to the
/// caller via the two callbacks.
namespace PaneLayoutBridge {

/// Serialise an existing splitter tree into a `PaneLayout`.
///
/// - `root`: the root `QSplitter`. Its direct children are either other
///   `QSplitter`s or pane widgets.
/// - `spaceToLeaves`: called for each pane widget encountered; must return
///   the tabs at that pane as a list of `PaneLeaf` (in visual order).
/// - `activeSpace`: the currently focused pane widget, used to locate the
///   active leaf id for the returned `PaneLayout`. May be `nullptr`.
///
/// Leaf ids are preserved if already set on the returned `PaneLeaf`s.
/// Unset ids are minted via `PaneLayout::newId()` inside the bridge.
PaneLayout serializeFromSplitter(
    QSplitter *root,
    const std::function<QList<PaneLeaf>(QWidget *space)> &spaceToLeaves,
    QWidget *activeSpace = nullptr);

/// Rebuild a splitter subtree from a `PaneLayout`.
///
/// - `layout`: consumed; ownership of its internal nodes is left intact
///   because the JSON-side value keeps living on the caller.
/// - `root`: populated with sub-splitters and pane widgets. Must start
///   empty (caller clears it first).
/// - `createSpace`: factory that returns a fresh pane widget ready to
///   accept tabs.
/// - `openTab`: called once per `PaneLeaf` with the owning pane widget.
///   Implementor is responsible for realising the file-open in that pane.
///
/// Returns the list of created pane widgets, in left-to-right visual
/// order (matching `PaneLayout::walk()` pre-order).
QList<QWidget *> deserializeIntoSplitter(
    const PaneLayout &layout,
    QSplitter *root,
    const std::function<QWidget *()> &createSpace,
    const std::function<void(QWidget *space, const PaneLeaf &leaf)> &openTab);

} // namespace PaneLayoutBridge

} // namespace Corbomite

#endif // CORBOMITE_CORE_PANELAYOUTBRIDGE_H
