// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QVector>
#include <forcegraph/GraphTypes.h>

namespace Corbomite {

class SQLiteIndex;
class VaultModel;

class GraphDataBuilder {
public:
    struct Result {
        QVector<ForceGraph::GraphNode> nodes;
        QVector<ForceGraph::GraphEdge> edges;
    };

    static Result buildGlobalGraph(SQLiteIndex *index, VaultModel *vault);
    static Result buildLocalGraph(SQLiteIndex *index, VaultModel *vault,
                                   const QString &centerNotePath, int depth = 2);
};

} // namespace Corbomite
