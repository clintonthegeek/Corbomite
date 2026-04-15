// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Qt>
#include <QString>

namespace Corbomite {

// Per docs/obsidian-audit/domains/workspace.md §7. A view type registers
// itself as a "hover-link source" so the workspace knows whether to fire
// the hover-link event for hovers happening inside that view.
//
//  - id           : stable identifier (e.g. "editor", "search", "graph")
//  - display      : human-readable label shown in Obsidian's settings UI
//  - defaultMod   : modifier the user must hold for the hover to fire
//                   (Qt::NoModifier = plain hover; common alternative is
//                   Qt::ControlModifier for "ctrl-hover" preview)
struct HoverLinkSource {
    QString id;
    QString display;
    Qt::KeyboardModifier defaultMod = Qt::NoModifier;
};

} // namespace Corbomite
