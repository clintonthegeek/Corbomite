// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QVector>
#include <forcegraph/GraphTypes.h>

namespace Corbomite {

class SearchProxy;
class VaultProxy;

class GraphDataBuilder {
public:
    struct Result {
        QVector<ForceGraph::GraphNode> nodes;
        QVector<ForceGraph::GraphEdge> edges;
    };

    // Proxy-typed only (Cluster N, Task 2.7). The raw-typed
    // (SQLiteIndex*, Vault*) overloads were removed once the last
    // production caller — the graph-view and local-graph plugins —
    // migrated onto SearchProxy / VaultProxy. Tests drive the builder
    // through proxies as well.
    static Result buildGlobalGraph(SearchProxy *search, VaultProxy *vault);
    static Result buildLocalGraph(SearchProxy *search, VaultProxy *vault,
                                   const QString &centerNotePath, int depth = 2);
};

} // namespace Corbomite
