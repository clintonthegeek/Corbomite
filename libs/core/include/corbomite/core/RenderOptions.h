// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <optional>

namespace Corbomite {

struct RenderOptions {
    // Subpath extraction: render only content under this heading/block
    // Empty = render full document
    // "#heading" = render from that heading to next same-level heading
    // "#^block-id" = render only the paragraph containing that block ID
    QString subpath;

    // Profile overrides (applied on top of the engine's default profile)
    std::optional<int> baseFontSizePt;
    std::optional<int> maxWidthPx;
    std::optional<int> marginPx;

    // Vault context for resolving links and embeds
    QString vaultRoot;
    QString notePath;
};

} // namespace Corbomite
