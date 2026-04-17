// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QVector>
#include <forcegraph/GraphTypes.h>

namespace Corbomite {

class SQLiteIndex;
class Vault;
class SearchProxy;
class VaultProxy;

class GraphDataBuilder {
public:
    struct Result {
        QVector<ForceGraph::GraphNode> nodes;
        QVector<ForceGraph::GraphEdge> edges;
    };

    static Result buildGlobalGraph(SQLiteIndex *index, Vault *vault);
    static Result buildLocalGraph(SQLiteIndex *index, Vault *vault,
                                   const QString &centerNotePath, int depth = 2);

    // Proxy-typed overloads (Cluster N). Semantics identical to the
    // raw-typed versions; used by plugin code that holds only
    // permission-gated proxies rather than raw Vault / SQLiteIndex.
    static Result buildGlobalGraph(SearchProxy *search, VaultProxy *vault);
    static Result buildLocalGraph(SearchProxy *search, VaultProxy *vault,
                                   const QString &centerNotePath, int depth = 2);
};

} // namespace Corbomite
