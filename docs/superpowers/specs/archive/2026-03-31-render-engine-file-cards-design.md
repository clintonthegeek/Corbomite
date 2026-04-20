# Unified Render Engine & Canvas File Cards — Design Specification

## Overview

Two coupled deliverables:
- **Unified Render Engine:** An abstract rendering interface (`MarkdownRenderEngine`) that all markdown display contexts use — reading mode, canvas cards, hover previews, and future editors. Decouples consumers from any specific rendering implementation.
- **Canvas File Cards:** A new `FileCardItem` in `libcanvas` that renders vault notes (and subpath sections) on the canvas using the render engine, completing Phase 4b file node support.

## Problem

The project has three separate rendering paths:
1. **Reading mode:** `MarkdownRenderer` (regex→HTML) → `QTextBrowser`
2. **Canvas text cards:** Custom inline regexes in `TextCardItem::paint()` → `QTextDocument` → `QPainter`
3. **Editor syntax highlighting:** `MarkdownHighlighter` (separate concern, unchanged by this work)

Path 1 is a regex hack with known edge cases. Path 2 is a separate, even-more-minimal regex hack. Neither is replaceable without touching every consumer. The Corbomite specification (§6.3) envisions a cmark-gfm AST pipeline — when that arrives, every consumer should benefit without rewiring.

Adding file cards to the canvas (Phase 4b) requires rendering arbitrary vault notes inside `QGraphicsScene`. Without a unified interface, this means a third copy of rendering logic. Instead, we build the interface first and make file cards the first proof that it works.

## Render Engine Architecture

### RenderedDocument

Opaque wrapper around the rendering output. Today it contains a `QTextDocument*`; this will be replaced by our own AST/IR when we build a proper markdown parser.

```cpp
namespace Corbomite {

class RenderedDocument {
public:
    ~RenderedDocument();

    // Today's primary output — consumers use this to paint or display
    QTextDocument *toQTextDocument() const;

    // Convenience: wrap in a ready-to-use QTextBrowser widget
    // Caller owns the returned widget
    QWidget *createWidget(QWidget *parent = nullptr) const;

    // Factory — used by engine implementations to construct
    static std::unique_ptr<RenderedDocument> fromQTextDocument(std::unique_ptr<QTextDocument> doc);

private:
    explicit RenderedDocument(std::unique_ptr<QTextDocument> doc);

    std::unique_ptr<QTextDocument> m_document;
};

} // namespace Corbomite
```

**Design intent:** Consumers interact with `RenderedDocument`, not `QTextDocument*` directly. When we replace the internals with our own AST, `toQTextDocument()` either builds one on demand from the AST or we deprecate it entirely and add richer APIs (`paint(QPainter*, QRectF)`, AST node access, etc.).

### RenderProfile

Named configuration bundles that control styling and behavior. Stored as a simple struct — no inheritance.

```cpp
namespace Corbomite {

struct RenderProfile {
    QString name;

    // Styling
    int baseFontSizePt = 14;
    int maxWidthPx = 0;           // 0 = no limit (fill container)
    int marginPx = 16;
    bool showFrontmatter = false;

    // Content
    bool renderImages = true;
    bool renderCodeHighlighting = true;

    // Predefined profiles
    static RenderProfile readingMode();   // Full page: 700px max-width, 16pt
    static RenderProfile canvasCard();    // Compact: no max-width, 11pt, tight margins
    static RenderProfile hoverPreview();  // Small: no max-width, 11pt, no images, truncated
};

} // namespace Corbomite
```

### RenderOptions

Per-call overrides and content parameters. Passed alongside markdown to each render call.

```cpp
namespace Corbomite {

struct RenderOptions {
    // Subpath extraction: render only content under this heading/block
    // Empty = render full document
    // "#heading" = render from that heading to next same-level heading
    // "#^block-id" = render only the paragraph containing that block ID
    QString subpath;

    // Profile overrides (applied on top of the engine's default profile)
    std::optional<int> baseFontSizePt;
    std::optional<int> maxWidthPx;
    std::optional<int> marginPx;

    // Vault context for resolving links and embeds
    QString vaultRoot;
    QString notePath;  // Relative path of the source note (for resolving relative links)
};

} // namespace Corbomite
```

### MarkdownRenderEngine (Abstract Interface)

The stable contract. Lives in `libs/core`.

```cpp
namespace Corbomite {

class MarkdownRenderEngine {
public:
    virtual ~MarkdownRenderEngine() = default;

    // Core rendering — returns an opaque rendered document
    virtual std::unique_ptr<RenderedDocument> render(
        const QString &markdown,
        const RenderOptions &options = {}) const = 0;

    // Profile management
    void setProfile(const RenderProfile &profile);
    RenderProfile profile() const;

protected:
    // Subpath extraction — shared utility, usable by all implementations
    static QString extractSubpath(const QString &markdown, const QString &subpath);

    RenderProfile m_profile;
};

} // namespace Corbomite
```

**Key decisions:**
- `render()` is the single virtual method — implementations override only this
- `extractSubpath()` is a static utility — heading/block extraction is markdown-structural, not renderer-specific. Implementations call it before rendering if `options.subpath` is non-empty.
- Profile is stored on the engine instance. Per-call overrides in `RenderOptions` are merged by the implementation.

### RegexRenderEngine (Current Implementation)

Wraps the existing `MarkdownRenderer` to fit the new interface. This is the "hack it to fit" adapter — minimal changes to existing code.

```cpp
namespace Corbomite {

class RegexRenderEngine : public MarkdownRenderEngine {
public:
    std::unique_ptr<RenderedDocument> render(
        const QString &markdown,
        const RenderOptions &options = {}) const override;

private:
    // Reuses existing MarkdownRenderer internally
    MarkdownRenderer m_legacyRenderer;
};
```

**Implementation sketch:**
1. If `options.subpath` is set, call `extractSubpath()` to narrow the markdown
2. Call `m_legacyRenderer.renderToHtml(markdown)` to get HTML
3. Apply profile-specific CSS overrides (font size, margins, max-width)
4. Create a `QTextDocument`, call `setHtml()` on it
5. Wrap in `RenderedDocument` and return

The existing `MarkdownRenderer` class stays unchanged — `RegexRenderEngine` composes it. When cmark-gfm arrives, we write `CmarkRenderEngine` and swap it in.

### Where It Lives

```
libs/core/
├── include/corbomite/core/
│   ├── MarkdownRenderEngine.h    // Abstract interface
│   ├── RenderedDocument.h        // Opaque output wrapper
│   ├── RenderProfile.h           // Profile struct
│   ├── RenderOptions.h           // Per-call options struct
│   ├── RegexRenderEngine.h       // Current implementation
│   └── MarkdownRenderer.h        // Existing (unchanged, used internally by RegexRenderEngine)
├── src/
│   ├── MarkdownRenderEngine.cpp  // extractSubpath() + profile accessors
│   ├── RenderedDocument.cpp       // toQTextDocument(), createWidget()
│   ├── RegexRenderEngine.cpp      // Adapter implementation
│   └── MarkdownRenderer.cpp       // Existing (unchanged)
```

All in `libs/core` so `libcanvas` can depend on the interface without depending on app code.

## Subpath Extraction

`extractSubpath()` handles two syntaxes from the JSON Canvas spec:

**Heading subpath (`#heading`):**
- Find the first heading matching the text (case-insensitive)
- Extract from that heading to the next heading of equal or higher level (or end of document)
- Example: `#Introduction` extracts the "Introduction" section and all its subsections

**Block ID subpath (`#^block-id`):**
- Find the line containing `^block-id`
- Extract the containing paragraph (contiguous non-empty lines around the block ID line)
- Strip the `^block-id` marker from the output

Both operate on raw markdown text before rendering — this is a text operation, not a rendering operation.

## Canvas File Cards

### FileCardItem

New graphics item in `libcanvas`, parallel to `TextCardItem`.

```cpp
namespace Canvas {

class FileCardItem : public QGraphicsObject {
    Q_OBJECT

public:
    FileCardItem(const CanvasNode &data, QGraphicsItem *parent = nullptr);

    void setNodeData(const CanvasNode &data);
    CanvasNode nodeData() const;
    QString nodeId() const;

    // Set the rendered content (called by scene when content is available)
    void setRenderedDocument(std::unique_ptr<Corbomite::RenderedDocument> doc);

    // Notify that the referenced file's content has changed
    void refresh();

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // Edge connection (same API as TextCardItem)
    QPointF connectionPoint(Side side) const;

    // Resize detection (same pattern as TextCardItem)
    enum ResizeMode { NoResize = 0, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };
    ResizeMode resizeModeAtPos(const QPointF &localPos) const;

Q_SIGNALS:
    void positionChanged();
    void sizeChanged();
    void editRequested();     // Double-click → open for editing
    void refreshRequested();  // Content needs re-rendering (file changed, resize, etc.)

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    CanvasNode m_data;
    std::unique_ptr<Corbomite::RenderedDocument> m_renderedDoc;
};

} // namespace Canvas
```

**Rendering approach:**
- `paint()` calls `m_renderedDoc->toQTextDocument()->drawContents(painter, clipRect)` — same pattern TextCardItem already uses for its markdown
- The card does NOT render markdown itself — it receives a pre-rendered `RenderedDocument`
- When the card is resized, it emits `refreshRequested()` so the scene/app can re-render at the new size

**Visual appearance:**
- Same rounded rectangle chrome as TextCardItem (same resize handles, selection border, color stripe)
- Title bar shows the filename (extracted from `m_data.file`, e.g. "My Note" from "path/to/My Note.md")
- If subpath is set, title shows "My Note > Heading"
- Body shows the rendered markdown content, clipped to card bounds
- A small file icon in the title bar distinguishes file cards from text cards

### Render Engine Injection

`libcanvas` depends on the abstract `MarkdownRenderEngine` interface (in `libs/core`), not on any concrete implementation. The app injects the engine.

**CanvasScene additions:**

```cpp
class CanvasScene : public QGraphicsScene {
    // ... existing API ...

    // Render engine for file cards
    void setRenderEngine(Corbomite::MarkdownRenderEngine *engine);
    Corbomite::MarkdownRenderEngine *renderEngine() const;

    // File content resolver — called when a file card needs content
    using FileResolver = std::function<QString(const QString &filePath)>;
    void setFileResolver(FileResolver resolver);

    // Item management additions
    FileCardItem *addFileCardItem(const CanvasNode &node);
    void removeFileCardItem(const QString &id);
    FileCardItem *fileCardItem(const QString &id) const;
};
```

**FileResolver:** The canvas doesn't know about vaults. When it needs to render a file card, it calls `m_fileResolver(node.file)` to get the markdown content, then passes it through the render engine. The app provides this callback:

```cpp
// In CanvasViewTab setup:
canvasScene->setFileResolver([vault](const QString &path) -> QString {
    auto *doc = vault->documentForPath(path);
    return doc ? doc->markdown() : QString();
});
```

**Rendering flow:**
1. `CanvasScene::onNodeAdded()` sees a `NodeType::File` node
2. Creates `FileCardItem`, stores in `m_fileCardItems`
3. Calls `m_fileResolver(node.file)` to get markdown content
4. Calls `m_renderEngine->render(markdown, options)` with subpath and `CanvasCard` profile
5. Calls `fileCard->setRenderedDocument(std::move(rendered))`
6. Card paints itself using the rendered document

### Inline Editing for File Cards

File cards support inline editing just like text cards — double-click to edit. The difference: edits write back to the source file, not to the canvas JSON.

**Edit flow:**
1. Double-click file card → `editRequested()` signal
2. Scene creates `QGraphicsProxyWidget` with `QTextEdit`, loads the file's markdown
3. On focus loss → `finishInlineEdit()`
4. Scene calls `m_fileResolver` callback in reverse — a `FileSaver` callback writes content back to the vault file
5. Scene re-renders the card

**Addition to CanvasScene:**

```cpp
using FileSaver = std::function<void(const QString &filePath, const QString &content)>;
void setFileSaver(FileSaver saver);
```

### CanvasScene Changes Summary

The `onNodeAdded` slot needs to handle `NodeType::File` in addition to `NodeType::Text` and `NodeType::Group`:

```cpp
void CanvasScene::onNodeAdded(const QString &id) {
    auto node = m_document->node(id);
    switch (node.type) {
    case NodeType::Text:
        addTextCardItem(node);
        break;
    case NodeType::File:
        addFileCardItem(node);
        break;
    case NodeType::Group:
        addGroupItemToScene(node);
        break;
    case NodeType::Link:
        // Phase 4b future
        break;
    }
}
```

Edge connections work with file cards — `EdgeItem` needs to accept `FileCardItem` as a source/target in addition to `TextCardItem`. This likely means extracting a shared interface or using `QGraphicsObject*` with `connectionPoint()`.

### Edge Connection Refactor

Currently `EdgeItem` takes `TextCardItem*` specifically. With file cards as edge targets, we need a common interface:

```cpp
namespace Canvas {

// Mixin interface for items that edges can connect to
class ConnectableItem {
public:
    virtual ~ConnectableItem() = default;
    virtual QPointF connectionPoint(Side side) const = 0;
    virtual QString nodeId() const = 0;
    virtual QGraphicsObject *asGraphicsObject() = 0;
};

} // namespace Canvas
```

`TextCardItem` and `FileCardItem` both implement `ConnectableItem`. `EdgeItem` stores `ConnectableItem*` instead of `TextCardItem*`.

## Migrating Existing Consumers

### NotePreviewWidget (Reading Mode)

Before:
```cpp
// Direct use of MarkdownRenderer
MarkdownRenderer m_renderer;
void renderDocument(NoteDocument *doc) {
    setHtml(m_renderer.renderToHtml(doc->markdown()));
}
```

After:
```cpp
// Uses the render engine
MarkdownRenderEngine *m_engine;  // Injected
void renderDocument(NoteDocument *doc) {
    auto rendered = m_engine->render(doc->markdown());
    setHtml(rendered->toQTextDocument()->toHtml());
    // Or in future: use createWidget() to replace QTextBrowser entirely
}
```

### Canvas TextCardItem

Before: Custom inline regex→HTML in `paint()`.

After: Uses the render engine with `CanvasCard` profile, same as file cards. The scene renders text card content through the engine and calls `setRenderedDocument()`. This unifies text and file card rendering.

### Future: Hover Preview

```cpp
auto rendered = m_engine->render(markdown, {.subpath = "#heading"});
m_popup->setWidget(rendered->createWidget(this));
```

## What This Does NOT Include

- **Link cards** (`NodeType::Link`) — URL preview cards, separate Phase 4b work
- **Live preview / WYSIWYG editor** — future work that will provide a new `MarkdownRenderEngine` implementation
- **cmark-gfm integration** — future replacement for `RegexRenderEngine`, drops in behind the same interface
- **Image rendering in canvas cards** — requires vault-relative image resolution, can be added to render options later
- **Canvas minimap** (Phase 4c)
- **Embedded canvas in notes** (future)

## Testing

### tst_renderengine.cpp (headless, in libs/core)

- `RegexRenderEngine::render()` returns non-null `RenderedDocument`
- `RenderedDocument::toQTextDocument()` returns valid `QTextDocument` with content
- `RenderedDocument::createWidget()` returns a `QWidget` (requires `QApplication`)
- Profile affects output: `CanvasCard` profile produces different styling than `ReadingMode`
- Render options override profile values
- Subpath extraction: heading subpath extracts correct section
- Subpath extraction: block ID subpath extracts correct paragraph
- Subpath extraction: nonexistent subpath returns empty string
- Empty markdown → empty but valid `RenderedDocument`

### tst_canvasscene.cpp additions

- Load canvas with file node → `FileCardItem` created in scene
- File card renders content from resolver
- File card with subpath → only subpath section rendered
- File card double-click → `editRequested()` emitted
- Edge between text card and file card → `EdgeItem` connects correctly
- Delete file card → card and connected edges removed
- File resolver returns empty → card shows "File not found" placeholder
- Undo add file card → card removed from scene

### tst_subpath.cpp (headless, in libs/core)

Dedicated tests for `extractSubpath()`:
- `#Heading` extracts from heading to next same-level heading
- `#Heading` with subsections includes them
- `#Heading` at end of document extracts to EOF
- `#^block-id` extracts containing paragraph
- `#^block-id` strips the marker from output
- Case-insensitive heading match
- Nonexistent heading → empty string
- Nonexistent block ID → empty string
- Empty subpath → returns full document unchanged
