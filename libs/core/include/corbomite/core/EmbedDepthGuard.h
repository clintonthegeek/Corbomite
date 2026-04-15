// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_EMBEDDEPTHGUARD_H
#define CORBOMITE_CORE_EMBEDDEPTHGUARD_H

#include <QString>

namespace Corbomite::Core {

/// Embed-depth recursion guard. Mirrors Obsidian's `JZ` guard; the cap
/// constant is audit-confirmed in
/// `docs/superpowers/research/2026-04-15-embed-depth-findings.md`
/// (literal `5` at Obsidian `_internal.js` ~line 627926).
///
/// Caller contract: increment `depth` before passing to `allow()`. First
/// embed runs with depth = 1; deepest reachable embed runs with depth = 5;
/// an attempted sixth level hits the fallback placeholder.
///
/// Compat-mode: match Obsidian exactly. A user-configurable cap is an
/// explicit post-parity follow-up (not Cluster J scope).
class EmbedDepthGuard
{
public:
    /// Audit-confirmed at Obsidian `_internal.js` ~line 627926.
    static constexpr int kMaxDepth = 5;

    /// Returns true if an embed attempted at `currentDepth` is permitted.
    bool allow(int currentDepth) const { return currentDepth < kMaxDepth; }

    /// Human-readable placeholder shown when the cap is exceeded.
    /// Mirrors Obsidian's `oJ` fallback (missing-embed variant).
    static QString placeholder(const QString &targetLabel);

    /// Exposed for clickable-placeholder UX: Obsidian's `oJ` opens the
    /// target in a new pane on click; host widgets read this to wire the
    /// click handler.
    static QString placeholderTarget(const QString &targetLabel)
    {
        return targetLabel;
    }
};

} // namespace Corbomite::Core

#endif // CORBOMITE_CORE_EMBEDDEPTHGUARD_H
