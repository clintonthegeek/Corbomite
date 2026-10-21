// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/LinkRewriter.h"

namespace Corbomite::Canvas {

/// `Corbomite::LinkRewriter` for `.canvas` files — the Corbomite counterpart
/// of Obsidian's `linkUpdaters["canvas"]` (`domains/canvas.md` §1). On a vault
/// rename it updates file-node `file` fields, group `background` fields, and
/// wikilink/markdown-link targets inside text-node `text`, through
/// `CanvasDocument` load→mutate→serialize so unknown fields and key order
/// survive.
///
/// When the canvas is open, the registered live `CanvasDocument` is mutated
/// instead and no bytes are returned: the open view owns the file, so writing
/// a second copy to disk would desync it.
class CanvasLinkRewriter : public Corbomite::LinkRewriter {
public:
    QString extension() const override;

    std::optional<QByteArray> rewrite(const QString &path,
                                      const QByteArray &bytes,
                                      const TargetMapper &mapTarget) override;
};

} // namespace Corbomite::Canvas
