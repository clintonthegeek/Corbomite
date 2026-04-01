// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QHash>
#include <QVector>
#include <QJsonObject>
#include "CanvasTypes.h"

namespace Canvas {

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
    bool m_modified = false;
};

} // namespace Canvas
