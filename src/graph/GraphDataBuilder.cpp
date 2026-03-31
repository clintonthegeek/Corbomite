// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphDataBuilder.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/core/NoteMeta.h"

#include <QSet>
#include <QQueue>
#include <cmath>

namespace Corbomite {

GraphDataBuilder::Result GraphDataBuilder::buildGlobalGraph(SQLiteIndex *index, VaultModel *vault)
{
    Result result;
    if (!index || !vault) return result;

    // Collect all note paths and their degrees
    QHash<QString, int> degree; // path -> connection count
    auto allNotes = vault->allNotes();
    QSet<QString> existingPaths;

    for (const auto &meta : allNotes) {
        existingPaths.insert(meta.relativePath);
        degree[meta.relativePath] = 0;
    }

    // Get all links and count degrees
    auto allLinks = index->allLinks();
    QSet<QString> unresolvedPaths;

    for (const auto &link : allLinks) {
        if (link.linkType == QStringLiteral("embed")) continue; // Skip embeds for graph

        degree[link.sourcePath]++;
        degree[link.targetPath]++;

        if (!existingPaths.contains(link.targetPath)) {
            unresolvedPaths.insert(link.targetPath);
        }
    }

    // Build nodes
    for (const auto &meta : allNotes) {
        ForceGraph::GraphNode node;
        node.id = meta.relativePath;
        node.label = meta.nameFromPath();

        int deg = degree.value(meta.relativePath, 0);
        node.radius = 4.0 + std::log(1.0 + deg) * 3.0;

        if (deg == 0) {
            node.color = QColor(170, 170, 170); // Orphan — light gray
        } else {
            node.color = QColor(123, 108, 217); // Regular — purple
        }

        result.nodes.append(node);
    }

    // Add unresolved nodes
    for (const auto &path : unresolvedPaths) {
        ForceGraph::GraphNode node;
        node.id = path;
        QString name = path;
        name = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
        node.label = name;
        node.radius = 3.0;
        node.color = QColor(136, 136, 136); // Unresolved — gray
        result.nodes.append(node);
    }

    // Build edges (skip embeds)
    for (const auto &link : allLinks) {
        if (link.linkType == QStringLiteral("embed")) continue;

        ForceGraph::GraphEdge edge;
        edge.sourceId = link.sourcePath;
        edge.targetId = link.targetPath;
        result.edges.append(edge);
    }

    return result;
}

GraphDataBuilder::Result GraphDataBuilder::buildLocalGraph(SQLiteIndex *index, VaultModel *vault,
                                                            const QString &centerNotePath, int depth)
{
    Result result;
    if (!index || !vault || centerNotePath.isEmpty()) return result;

    // BFS from center to collect nodes within N hops
    QSet<QString> collected;
    QQueue<QPair<QString, int>> queue; // (path, current_depth)
    queue.enqueue({centerNotePath, 0});
    collected.insert(centerNotePath);

    while (!queue.isEmpty()) {
        auto [path, d] = queue.dequeue();
        if (d >= depth) continue;

        // Outlinks
        auto outlinks = index->outlinksFor(path);
        for (const auto &link : outlinks) {
            if (link.linkType == QStringLiteral("embed")) continue;
            if (!collected.contains(link.targetPath)) {
                collected.insert(link.targetPath);
                queue.enqueue({link.targetPath, d + 1});
            }
        }

        // Backlinks
        auto backlinks = index->backlinksFor(path);
        for (const auto &link : backlinks) {
            if (!collected.contains(link.sourcePath)) {
                collected.insert(link.sourcePath);
                queue.enqueue({link.sourcePath, d + 1});
            }
        }
    }

    // Build nodes for collected paths
    QHash<QString, int> degree;
    for (const auto &path : collected) degree[path] = 0;

    // Get edges between collected nodes
    auto allLinks = index->allLinks();
    for (const auto &link : allLinks) {
        if (link.linkType == QStringLiteral("embed")) continue;
        if (collected.contains(link.sourcePath) && collected.contains(link.targetPath)) {
            ForceGraph::GraphEdge edge;
            edge.sourceId = link.sourcePath;
            edge.targetId = link.targetPath;
            result.edges.append(edge);

            degree[link.sourcePath]++;
            degree[link.targetPath]++;
        }
    }

    // Create nodes
    for (const auto &path : collected) {
        ForceGraph::GraphNode node;
        node.id = path;

        QString name = path;
        name = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
        node.label = name;

        int deg = degree.value(path, 0);
        node.radius = 4.0 + std::log(1.0 + deg) * 3.0;

        if (path == centerNotePath) {
            node.color = QColor(86, 182, 194);  // Center — teal, slightly larger
            node.radius += 3.0;
        } else if (vault->noteExists(path)) {
            node.color = QColor(123, 108, 217);  // Regular — purple
        } else {
            node.color = QColor(136, 136, 136);  // Unresolved — gray
        }

        result.nodes.append(node);
    }

    return result;
}

} // namespace Corbomite
