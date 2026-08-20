// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QHash>
#include <QVector>
#include <QJsonObject>
#include "CanvasTypes.h"

namespace Canvas {

// V5 angular-sector picker (canvas.md §3 invariant 5 + §8 invariant 10):
// given a node's own dimensions and the vector from its center to the other
// endpoint's center, return the face the edge emerges from. Aspect-aware:
// the boundary between "horizontal face" and "vertical face" is the node's
// own corner diagonal, i.e. atan2(h/2, w/2), not a fixed 45°. Shared between
// CanvasDocument's load-time self-heal and CanvasScene's drop-on-empty edge
// authoring (M3.3).
Side pickSideToward(int thisW, int thisH, double dx, double dy);

class CanvasDocument : public QObject {
    Q_OBJECT

public:
    explicit CanvasDocument(QObject *parent = nullptr);

    // Serialization
    bool loadFromJson(const QJsonObject &json);
    QJsonObject toJson() const;
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
    QHash<QString, CanvasNode> m_nodes;
    QHash<QString, CanvasEdge> m_edges;
    // Insertion/load order of node and edge ids. QHash has no stable
    // iteration order, so nodes()/edges()/toJson() would otherwise reorder
    // the .canvas file's JSON arrays on every save relative to what was
    // originally loaded (or what Obsidian wrote) — pure diff-churn noise.
    // Kept in lockstep with m_nodes/m_edges by add*/remove*/loadFromJson.
    QVector<QString> m_nodeOrder;
    QVector<QString> m_edgeOrder;
    // Unknown top-level JSON keys preserved verbatim across load→save
    // (Obsidian spreads ...unknownData on the root object; we must too,
    // otherwise plugin- or future-Obsidian-written canvases lose data).
    QJsonObject m_extraTopLevel;
    bool m_modified = false;
};

} // namespace Canvas
