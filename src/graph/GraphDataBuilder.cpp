// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphDataBuilder.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/core/NoteMeta.h"

#include <QApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QSet>
#include <QQueue>
#include <cmath>

namespace Corbomite {

static constexpr int HUB_DEGREE_THRESHOLD = 6;

// Daily note pattern: filename is YYYY-MM-DD (with optional suffix)
static bool isDailyNote(const QString &label)
{
    static const QRegularExpression re(QStringLiteral(R"(^\d{4}-\d{2}-\d{2})"));
    return re.match(label).hasMatch();
}

// Derive graph colors from the current palette for theme awareness.
// These are intentionally muted/distinct from the accent color to avoid
// clashing with selection highlights.
static QColor colorForType(ForceGraph::NodeType type)
{
    const QPalette &pal = QApplication::palette();
    bool dark = pal.color(QPalette::Window).lightness() < 128;

    switch (type) {
    case ForceGraph::NodeType::Hub:
        // Brighter purple for hubs — stands out from regular
        return dark ? QColor(155, 140, 233) : QColor(100, 80, 200);
    case ForceGraph::NodeType::Regular:
        return dark ? QColor(123, 108, 217) : QColor(110, 90, 190);
    case ForceGraph::NodeType::Orphan:
        return dark ? QColor(140, 140, 140) : QColor(170, 170, 170);
    case ForceGraph::NodeType::Unresolved:
        return dark ? QColor(100, 100, 100) : QColor(160, 160, 160);
    case ForceGraph::NodeType::DailyNote:
        return dark ? QColor(86, 182, 194) : QColor(50, 150, 165);
    }
    return pal.color(QPalette::Text);
}

static void classifyNode(ForceGraph::GraphNode &node, int deg, bool exists)
{
    node.degree = deg;
    node.radius = 4.0 + std::log(1.0 + deg) * 3.0;

    if (!exists) {
        node.type = ForceGraph::NodeType::Unresolved;
        node.radius = 3.0;
    } else if (deg == 0) {
        node.type = ForceGraph::NodeType::Orphan;
    } else if (isDailyNote(node.label)) {
        node.type = ForceGraph::NodeType::DailyNote;
    } else if (deg >= HUB_DEGREE_THRESHOLD) {
        node.type = ForceGraph::NodeType::Hub;
    } else {
        node.type = ForceGraph::NodeType::Regular;
    }

    node.color = colorForType(node.type);
}

GraphDataBuilder::Result GraphDataBuilder::buildGlobalGraph(SQLiteIndex *index, VaultModel *vault)
{
    Result result;
    if (!index || !vault) return result;

    // Collect all note paths and their degrees
    QHash<QString, int> degree;
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
        if (link.linkType == QStringLiteral("embed")) continue;

        degree[link.sourcePath]++;
        degree[link.targetPath]++;

        if (!existingPaths.contains(link.targetPath)) {
            unresolvedPaths.insert(link.targetPath);
        }
    }

    // Build tag → notes reverse map for tooltips. Phase 8: CachedMetadata
    // stores tags with the leading '#' (Obsidian shape); strip it for display.
    QHash<QString, QStringList> noteTags; // path → [tag1, tag2, ...]
    const auto tags = index->allTags();
    for (const auto &tag : tags) {
        const auto paths = index->notesWithTag(tag);
        QString displayTag = tag;
        if (displayTag.startsWith(QLatin1Char('#'))) displayTag.remove(0, 1);
        for (const auto &p : paths) {
            noteTags[p].append(displayTag);
        }
    }

    // Build nodes
    for (const auto &meta : allNotes) {
        ForceGraph::GraphNode node;
        node.id = meta.relativePath;
        node.label = meta.nameFromPath();
        classifyNode(node, degree.value(meta.relativePath, 0), true);

        // Rich tooltip
        int deg = node.degree;
        QString tip = QStringLiteral("<b>%1</b>&nbsp;&nbsp;%2 link%3")
            .arg(node.label.toHtmlEscaped())
            .arg(deg)
            .arg(deg == 1 ? QString() : QStringLiteral("s"));
        const auto &nodeTags = noteTags.value(meta.relativePath);
        if (!nodeTags.isEmpty()) {
            tip += QStringLiteral("<br><i>%1</i>").arg(nodeTags.join(QStringLiteral(", ")));
        }
        node.tooltip = tip;

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
        classifyNode(node, degree.value(path, 0), false);
        node.tooltip = QStringLiteral("<b>%1</b><br><i>unresolved link</i>")
            .arg(node.label.toHtmlEscaped());
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

        bool exists = vault->noteExists(path);
        classifyNode(node, degree.value(path, 0), exists);

        // Center note gets special treatment: teal + larger
        if (path == centerNotePath) {
            node.color = colorForType(ForceGraph::NodeType::DailyNote);
            node.radius += 3.0;
        }

        result.nodes.append(node);
    }

    return result;
}

} // namespace Corbomite
