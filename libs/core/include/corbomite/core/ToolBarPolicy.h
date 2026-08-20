// libs/core/include/corbomite/core/ToolBarPolicy.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite {

/// Cluster O Phase O3 (O3.T4, doctrine §D4) — tri-state, user-overridable
/// visibility policy for one `ViewActions` provider's persistent toolbar.
/// Persisted per-provider in `corbomite.kcfg`'s `<group name="Toolbars">`
/// (one named entry per provider view type, e.g. `MarkdownToolBarPolicy`).
enum class ToolBarPolicy {
    /// Visible iff the active leaf's view type matches this toolbar's
    /// provider type. Default.
    Auto,
    /// Pinned visible regardless of context; out-of-context its actions are
    /// disabled (Tier B — the provider is unbound, so its own `refresh()`
    /// already disables everything it owns).
    AlwaysShow,
    /// Pinned hidden regardless of context.
    AlwaysHide,
};

inline QString toolBarPolicyToString(ToolBarPolicy policy)
{
    switch (policy) {
    case ToolBarPolicy::AlwaysShow: return QStringLiteral("AlwaysShow");
    case ToolBarPolicy::AlwaysHide: return QStringLiteral("AlwaysHide");
    case ToolBarPolicy::Auto:
    default:
        return QStringLiteral("Auto");
    }
}

inline ToolBarPolicy toolBarPolicyFromString(const QString &s)
{
    if (s == QStringLiteral("AlwaysShow")) return ToolBarPolicy::AlwaysShow;
    if (s == QStringLiteral("AlwaysHide")) return ToolBarPolicy::AlwaysHide;
    return ToolBarPolicy::Auto;
}

/// Pure Tier-A visibility decision — no widgets involved, directly
/// unit-testable independent of any real toolbar (`tst_toolbar_policy`).
inline bool toolBarShouldBeVisible(ToolBarPolicy policy, bool inContext)
{
    switch (policy) {
    case ToolBarPolicy::AlwaysShow: return true;
    case ToolBarPolicy::AlwaysHide: return false;
    case ToolBarPolicy::Auto:
    default:
        return inContext;
    }
}

} // namespace Corbomite
