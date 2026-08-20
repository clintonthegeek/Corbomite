// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QUndoCommand>
#include <QVector>

#include "CanvasTypes.h"

namespace Canvas {

class CanvasDocument;

// 1. Move one or more cards
class CmdMoveCards : public QUndoCommand {
public:
    CmdMoveCards(CanvasDocument *doc,
                 const QHash<QString, QPointF> &oldPositions,
                 const QHash<QString, QPointF> &newPositions);
    void redo() override;
    void undo() override;

    // M4.2 — coalesce consecutive nudges (or a nudge right after a drag)
    // of the exact same node set into one undo step, so holding an arrow
    // key doesn't spam the undo stack. id() shares one value across all
    // CmdMoveCards instances (both the drag-end path in
    // CanvasScene::onDragEnded and the new arrow-key nudge path in
    // CanvasScene::keyPressEvent push this same command type);
    // mergeWith() only accepts another CmdMoveCards on the same document
    // whose node-id set is identical, and keeps this command's original
    // (pre-move) positions while adopting the incoming post-move ones.
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

private:
    CanvasDocument *m_doc;
    QHash<QString, QPointF> m_oldPositions;
    QHash<QString, QPointF> m_newPositions;
};

// 2. Resize a card
class CmdResizeCard : public QUndoCommand {
public:
    CmdResizeCard(CanvasDocument *doc, const QString &nodeId,
                  const QRect &oldRect, const QRect &newRect);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    QString m_nodeId;
    QRect m_oldRect;
    QRect m_newRect;
};

// 3. Add a card
class CmdAddCard : public QUndoCommand {
public:
    CmdAddCard(CanvasDocument *doc, const CanvasNode &node, QUndoCommand *parent = nullptr);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    CanvasNode m_node;
};

// 4. Remove a card (and its connected edges)
class CmdRemoveCard : public QUndoCommand {
public:
    CmdRemoveCard(CanvasDocument *doc, const QString &nodeId,
                  QUndoCommand *parent = nullptr);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    QString m_nodeId;
    CanvasNode m_savedNode;
    QVector<CanvasEdge> m_savedEdges;
};

// 5. Add an edge
class CmdAddEdge : public QUndoCommand {
public:
    CmdAddEdge(CanvasDocument *doc, const CanvasEdge &edge, QUndoCommand *parent = nullptr);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    CanvasEdge m_edge;
};

// 6. Remove an edge
class CmdRemoveEdge : public QUndoCommand {
public:
    CmdRemoveEdge(CanvasDocument *doc, const QString &edgeId,
                  QUndoCommand *parent = nullptr);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    QString m_edgeId;
    CanvasEdge m_savedEdge;
};

// 7. Reconnect an edge endpoint to a different node/side (M3.4)
class CmdReconnectEdge : public QUndoCommand {
public:
    CmdReconnectEdge(CanvasDocument *doc, const CanvasEdge &oldEdge, const CanvasEdge &newEdge,
                      QUndoCommand *parent = nullptr);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    CanvasEdge m_oldEdge;
    CanvasEdge m_newEdge;
};

// 7b. Set an edge's fromEnd/toEnd (Direction submenu, M3.5)
class CmdSetEdgeEnds : public QUndoCommand {
public:
    CmdSetEdgeEnds(CanvasDocument *doc, const CanvasEdge &oldEdge, const CanvasEdge &newEdge,
                   QUndoCommand *parent = nullptr);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    CanvasEdge m_oldEdge;
    CanvasEdge m_newEdge;
};

// 7c. Reverse an edge's direction (swaps fromNode/fromSide/fromEnd with
// toNode/toSide/toEnd). Self-inverse: redo()/undo() both re-read the edge
// from the document and swap it again (M3.5).
class CmdReverseEdge : public QUndoCommand {
public:
    CmdReverseEdge(CanvasDocument *doc, const QString &edgeId, QUndoCommand *parent = nullptr);
    void redo() override;
    void undo() override;

private:
    void swapAndApply();

    CanvasDocument *m_doc;
    QString m_edgeId;
};

// 8. Edit text content of a card
class CmdEditText : public QUndoCommand {
public:
    CmdEditText(CanvasDocument *doc, const QString &nodeId,
                const QString &oldText, const QString &newText);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    QString m_nodeId;
    QString m_oldText;
    QString m_newText;
};

// 9. Change card color
class CmdChangeColor : public QUndoCommand {
public:
    CmdChangeColor(CanvasDocument *doc, const QString &nodeId,
                   const QString &oldColor, const QString &newColor);
    void redo() override;
    void undo() override;

private:
    CanvasDocument *m_doc;
    QString m_nodeId;
    QString m_oldColor;
    QString m_newColor;
};

} // namespace Canvas
