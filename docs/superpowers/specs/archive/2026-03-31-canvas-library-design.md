# libcanvas — Canvas Library Design Specification (Phase 4a)

## Overview

A standalone, reusable Qt6/C++ library for interactive canvas editing compatible with the JSON Canvas 1.0 format (jsoncanvas.org). Two layers: a data model with JSON serialization (Qt6::Core only) and a QGraphicsView-based interactive editor (Qt6::Widgets).

Published as a standalone git repository, integrated into Corbomite as a library at `libs/canvas/`. GPLv3 licensed.

## JSON Canvas 1.0 Compatibility

Full read/write support for the published JSON Canvas spec:
- Node types: `text`, `group` (Phase 4a), `file`, `link` (Phase 4b)
- Edge properties: `fromNode`, `toNode`, `fromSide`, `toSide`, `fromEnd`, `toEnd`, `color`, `label`
- Color encoding: preset strings `"1"`-`"6"` and hex `"#RRGGBB"`
- Coordinate system: origin at center, positive x=right, positive y=down

## Layer 1: Data Model + Serialization

### Data Types

```cpp
namespace Canvas {

enum class NodeType { Text, File, Link, Group };
enum class Side { Top, Right, Bottom, Left };
enum class EndType { None, Arrow };

struct CanvasNode {
    QString id;
    NodeType type = NodeType::Text;
    int x = 0, y = 0;
    int width = 250, height = 60;
    QString color;             // "" (default), "1"-"6" (preset), "#RRGGBB" (hex)

    // Type-specific fields
    QString text;              // Text nodes: markdown content
    QString file;              // File nodes: vault path (Phase 4b)
    QString subpath;           // File nodes: #heading or #^block (Phase 4b)
    QString url;               // Link nodes: URL (Phase 4b)
    QString label;             // Group nodes: group label
    QString background;        // Group nodes: background image path
    QString backgroundStyle;   // Group nodes: "cover", "ratio", "repeat"
};

struct CanvasEdge {
    QString id;
    QString fromNode;
    QString toNode;
    Side fromSide = Side::Right;
    Side toSide = Side::Left;
    EndType fromEnd = EndType::None;
    EndType toEnd = EndType::Arrow;
    QString color;
    QString label;
};

} // namespace Canvas
```

### CanvasDocument

```cpp
class CanvasDocument : public QObject {
    Q_OBJECT
public:
    explicit CanvasDocument(QObject *parent = nullptr);

    // Serialization (JSON Canvas 1.0)
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

    // Edge operations
    void addEdge(const CanvasEdge &edge);
    void removeEdge(const QString &id);
    CanvasEdge edge(const QString &id) const;
    QVector<CanvasEdge> edges() const;

    // State
    bool isModified() const;
    void setModified(bool modified);

    // ID generation
    static QString generateId();

signals:
    void nodeAdded(const QString &id);
    void nodeRemoved(const QString &id);
    void nodeChanged(const QString &id);
    void edgeAdded(const QString &id);
    void edgeRemoved(const QString &id);
    void modificationChanged(bool modified);
};
```

### Color Mapping

```cpp
static QColor colorFromCanvasColor(const QString &canvasColor) {
    if (canvasColor == "1") return QColor(233, 49, 71);    // Red
    if (canvasColor == "2") return QColor(236, 117, 0);    // Orange
    if (canvasColor == "3") return QColor(224, 172, 0);    // Yellow
    if (canvasColor == "4") return QColor(8, 185, 78);     // Green
    if (canvasColor == "5") return QColor(0, 191, 188);    // Cyan
    if (canvasColor == "6") return QColor(120, 82, 238);   // Purple
    if (canvasColor.startsWith('#')) return QColor(canvasColor);
    return QColor();  // Default/no color
}
```

## Layer 2: Interactive View

### CanvasView (QGraphicsView)

```cpp
class CanvasView : public QGraphicsView {
    Q_OBJECT
public:
    explicit CanvasView(QWidget *parent = nullptr);

    void setDocument(CanvasDocument *doc);
    CanvasDocument *document() const;
    void zoomToFit();
    void zoomIn();
    void zoomOut();

signals:
    void cardDoubleClicked(const QString &nodeId);
    void selectionChanged(const QStringList &selectedIds);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;
};
```

Grid background drawn in `drawBackground()` — light dotted grid, optional.

### CanvasScene (QGraphicsScene)

Owns all card and edge items. Delegates mouse events to the active tool (PlanStan pattern).

```cpp
class CanvasScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit CanvasScene(QObject *parent = nullptr);

    void setDocument(CanvasDocument *doc);

    // Tool management
    void setActiveTool(CanvasTool *tool);
    CanvasTool *activeTool() const;

    // Item lookup
    TextCardItem *textCardItem(const QString &id) const;
    GroupItem *groupItem(const QString &id) const;
    EdgeItem *edgeItem(const QString &id) const;

    // Undo
    QUndoStack *undoStack();

signals:
    void cardDoubleClicked(const QString &nodeId);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
};
```

### Tool State Machine (PlanStan pattern)

```cpp
class CanvasTool : public QObject {
    Q_OBJECT
public:
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void activate() {}
    virtual void deactivate() {}
};

class SelectMoveTool : public CanvasTool { ... };
// Handles: select, multi-select, drag move, resize (Kdenlive pattern)

class CreateCardTool : public CanvasTool { ... };
// Handles: click to place new text card at position

class CreateEdgeTool : public CanvasTool { ... };
// Handles: drag from card edge dot to target card
```

### TextCardItem (QGraphicsItem)

Rounded rectangle with markdown text content:
- `paint()`: draws rounded rect background + color stripe at top + rendered text
- Double-click: creates `QGraphicsProxyWidget` with `QTextEdit` for inline editing, commits on focus loss
- Resize handles: 8 points (4 corners + 4 edge midpoints) using Kdenlive's detection pattern
- `itemChange(ItemPositionHasChanged)`: updates `CanvasDocument` node position, adjusts connected edges
- Color: background tint from the 6-color palette

```cpp
class TextCardItem : public QGraphicsObject {
    Q_OBJECT
public:
    TextCardItem(const CanvasNode &data, CanvasScene *scene);

    void setNodeData(const CanvasNode &data);
    CanvasNode nodeData() const;
    QString nodeId() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // Resize
    enum ResizeMode { NoResize, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };
    ResizeMode resizeModeAtPos(const QPointF &pos) const;

    // Edge connection points
    QPointF connectionPoint(Side side) const;

signals:
    void positionChanged();
    void sizeChanged();
    void editingFinished(const QString &newText);
};
```

### GroupItem (QGraphicsObject)

Semi-transparent colored rectangle with label:
- Background fill with reduced opacity
- Label text at top
- `containedItems()`: finds all card items whose center is within the group rect
- Moving group moves all contained items
- Double-click label to edit (inline QLineEdit via QGraphicsProxyWidget)

```cpp
class GroupItem : public QGraphicsObject {
    Q_OBJECT
public:
    GroupItem(const CanvasNode &data, CanvasScene *scene);

    QVector<QGraphicsItem *> containedItems() const;
    void setNodeData(const CanvasNode &data);
    QString nodeId() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
};
```

### EdgeItem (QGraphicsPathItem)

Line between two cards with optional arrow and label:
- Path computed from source card's `connectionPoint(fromSide)` to target card's `connectionPoint(toSide)`
- Arrow head rendered as small triangle at the `toEnd` (or both ends if bidirectional)
- Label rendered at midpoint of the path
- `adjust()` called when connected cards move
- Hover: thicker line + highlight color
- Double-click: inline edit label

```cpp
class EdgeItem : public QGraphicsPathItem {
public:
    EdgeItem(TextCardItem *fromCard, TextCardItem *toCard, const CanvasEdge &data, QGraphicsItem *parent = nullptr);

    void adjust();          // Recompute path from card positions
    void setEdgeData(const CanvasEdge &data);
    CanvasEdge edgeData() const;
    QString edgeId() const;
};
```

### Undo/Redo (Umbrello pattern)

```cpp
class CmdMoveCards : public QUndoCommand {
    // Captures old positions on construction, new positions on first redo
};

class CmdResizeCard : public QUndoCommand {
    // Captures old rect and new rect
};

class CmdAddCard : public QUndoCommand {
    // Stores CanvasNode data, adds/removes on redo/undo
};

class CmdRemoveCard : public QUndoCommand {
    // Stores CanvasNode + connected edges for full restoration
};

class CmdAddEdge : public QUndoCommand { ... };
class CmdRemoveEdge : public QUndoCommand { ... };
class CmdEditText : public QUndoCommand { ... };
class CmdChangeColor : public QUndoCommand { ... };
```

### Context Menus

**Right-click empty space:**
- New Text Card
- New Group
- Paste (if clipboard has card data)

**Right-click card:**
- Edit (enter edit mode)
- Color → submenu: Red, Orange, Yellow, Green, Cyan, Purple, Remove Color
- Duplicate
- Delete

**Right-click edge:**
- Edit Label
- Reverse Direction
- Delete

## Project Structure

```
libs/canvas/
├── CMakeLists.txt
├── README.md
├── include/canvas/
│   ├── CanvasTypes.h
│   ├── CanvasDocument.h
│   ├── CanvasView.h
│   ├── CanvasScene.h
│   ├── CanvasTool.h
│   ├── TextCardItem.h
│   ├── GroupItem.h
│   └── EdgeItem.h
├── src/
│   ├── CanvasDocument.cpp
│   ├── CanvasView.cpp
│   ├── CanvasScene.cpp
│   ├── CanvasTool.cpp
│   ├── TextCardItem.cpp
│   ├── GroupItem.cpp
│   └── EdgeItem.cpp
└── tests/
    ├── CMakeLists.txt
    ├── tst_canvasdocument.cpp
    └── tst_canvasscene.cpp
```

## Testing

### tst_canvasdocument.cpp (headless)

- Load Obsidian sample `.canvas` JSON → correct node/edge counts
- Text node: correct id, type, x, y, width, height, text
- Group node: correct label
- Edge: correct fromNode, toNode, sides, ends, label, color
- Round-trip: load → toJson → reload → identical data
- Empty document → `{"nodes":[],"edges":[]}`
- Add node via API → nodeAdded signal emitted
- Remove node → nodeRemoved signal, connected edges also removed
- Color encoding: "1" → red, "6" → purple, "#FF0000" → hex
- Generate unique IDs
- Modified state tracking

### tst_canvasscene.cpp (needs QApplication, offscreen)

- Add text card from document → scene item created at correct position
- Move card in scene → document position updated
- Resize card → document width/height updated
- Add edge → EdgeItem connecting two cards
- Delete card → card and connected edges removed
- Group containment: move group → contained cards move too
- Undo move → card returns to original position
- Redo after undo → card at moved position

## What This Does NOT Include

- File cards (Phase 4b — vault integration)
- Link cards (Phase 4b — URL display/preview)
- Minimap overlay (Phase 4c)
- Snap-to-grid UI toggle (Phase 4c — engine supports it, no UI)
- Canvas presentation mode (future)
- Freehand drawing (not in JSON Canvas spec)
- Embedded canvas in notes (future)
- Corbomite integration (separate spec — wiring canvas as editor tab)
