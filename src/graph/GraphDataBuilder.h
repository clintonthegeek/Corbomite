// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QVector>
#include <corbomite/forcegraph/GraphTypes.h>

#include "corbomite/storage/IgnoreFilter.h"

namespace Corbomite {

class SearchProxy;
class VaultProxy;

class GraphDataBuilder {
public:
    struct Result {
        QVector<Corbomite::ForceGraph::GraphNode> nodes;
        QVector<Corbomite::ForceGraph::GraphEdge> edges;
    };

    // Proxy-typed only. The raw-typed
    // (SQLiteIndex*, Vault*) overloads were removed once the last
    // production caller — the graph-view and local-graph plugins —
    // migrated onto SearchProxy / VaultProxy. Tests drive the builder
    // through proxies as well.
    //
    // `ignore` is Obsidian's `userIgnoreFilters`, which gates graph nodes
    // (`app/own/1851-h0.js:162`). It is a graph/search/property-edit gate, not a
    // tree or index gate — see
    // docs/obsidian-audit/addenda/2026-10-03-vault-hidden-paths-and-userignore.md.
    // Callers build it with ignoreFilterFor().
    static Result buildGlobalGraph(SearchProxy *search, VaultProxy *vault,
                                   const IgnoreFilter &ignore = {});
    static Result buildLocalGraph(SearchProxy *search, VaultProxy *vault,
                                   const QString &centerNotePath, int depth = 2,
                                   const IgnoreFilter &ignore = {});

    /// Build the `userIgnoreFilters` filter from a vault's `app.json`. Returns
    /// an empty filter when the proxy is null, the config is unreadable, or the
    /// key is absent — reads are permission-gated, so a denied read degrades to
    /// "nothing ignored" rather than throwing.
    static IgnoreFilter ignoreFilterFor(VaultProxy *vault);
};

} // namespace Corbomite
