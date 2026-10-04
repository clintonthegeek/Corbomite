// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QHash>
#include <QVector>
#include <QJsonObject>
#include "CanvasTypes.h"
#include "CanvasJsonPrinter.h"

namespace Canvas {

// Angular-sector picker, upstream `$7` (app/own/2023-$7.js:2): given a node's
// own dimensions and the vector from its center to the other endpoint's
// center, return the face the edge emerges from. Aspect-aware — the boundary
// between "horizontal face" and "vertical face" is the node's own corner
// diagonal, i.e. atan2(h/2, w/2), not a fixed 45°. Shared between
// CanvasDocument's load-time self-heal and CanvasScene's drop-on-empty edge
// authoring.
Side pickSideToward(int thisW, int thisH, double dx, double dy);

// Obsidian normalises "r,g,b" to "#rrggbb" on read, with no clamping
// (Z8/$8/qb/zb — app/own/1998-Z8.js:2, 1997-$8.js:2, 0810-qb.js:2,
// 0809-zb.js:2). Preset indices and hex pass through untouched.
QString normalizeCanvasColor(const QString &raw);

class CanvasDocument : public QObject {
    Q_OBJECT

public:
    explicit CanvasDocument(QObject *parent = nullptr);

    // Serialization
    bool loadFromJson(const QJsonObject &json);
    QJsonObject toJson() const;
    // The on-disk bytes: Obsidian's `$d` printer, so a canvas Corbomite saves
    // diffs cleanly against one Obsidian saved. toJson() is the same document
    // as an unordered QJsonObject, for callers that want to inspect it.
    QByteArray serializeObsidianStyle() const;
    bool loadFromFile(const QString &filePath);
    bool saveToFile(const QString &filePath);

    // Node operations
    void addNode(const CanvasNode &node);
    void removeNode(const QString &id);
    void updateNode(const CanvasNode &node);
    CanvasNode node(const QString &id) const;
    QVector<CanvasNode> nodes() const;
    bool hasNode(const QString &id) const;

    // Edge operations
    void addEdge(const CanvasEdge &edge);
    void removeEdge(const QString &id);
    void updateEdge(const CanvasEdge &edge);
    CanvasEdge edge(const QString &id) const;
    QVector<CanvasEdge> edges() const;
    QVector<CanvasEdge> edgesForNode(const QString &nodeId) const;

    // State
    bool isModified() const;
    void setModified(bool modified);

    // ID generation
    static QString generateId();

Q_SIGNALS:
    void nodeAdded(const QString &id);
    void nodeRemoved(const QString &id);
    void nodeChanged(const QString &id);
    void edgeAdded(const QString &id);
    void edgeRemoved(const QString &id);
    void edgeChanged(const QString &id);
    void modificationChanged(bool modified);

private:
    OrderedJson toOrderedJson() const;
    void adoptEdge(const QJsonObject &obj, CanvasEdge &edge) const;
    void refreshUnresolvedEdges();

    QHash<QString, CanvasNode> m_nodes;
    QHash<QString, CanvasEdge> m_edges;
    // Insertion/load order of node and edge ids. QHash has no stable
    // iteration order, so nodes()/edges()/toJson() would otherwise reorder
    // the .canvas file's JSON arrays on every save relative to what was
    // originally loaded (or what Obsidian wrote) — pure diff-churn noise.
    // Kept in lockstep with m_nodes/m_edges by add*/remove*/loadFromJson.
    QVector<QString> m_nodeOrder;
    QVector<QString> m_edgeOrder;
    // Paint order. Upstream re-sorts `nodes` by zIndex on every save
    // (app/own/2018-k7.js:362) and does not sort `edges` (:367-370), so array
    // position *is* z-order for nodes and insertion order for edges. zIndex
    // is not on disk: upstream reconstructs it as a running maximum over file
    // order (:429), which is exactly m_nodeOrder, and that is what m_nodeZIndex
    // records. No reordering API exists yet, so the two are equal by
    // construction — this map is the seam where "raise to front" will land.
    QHash<QString, int> m_nodeZIndex;
    // Unknown top-level JSON keys preserved verbatim across load→save
    // (Obsidian spreads ...unknownData on the root object; we must too,
    // otherwise plugin- or future-Obsidian-written canvases lose data).
    QJsonObject m_extraTopLevel;
    bool m_modified = false;
};

} // namespace Canvas
