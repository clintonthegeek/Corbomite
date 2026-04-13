# Markoff Architecture

A Qt6/C++ markdown editing and rendering library. Markoff is a standalone
library that does not depend on Corbomite. It owns all Obsidian-flavored
markdown concerns: parsing, rendering, syntax highlighting, inline math,
and interactive block editing. Application-level features (vault navigation,
completion popups, session management) remain in the host application.

---

## Design Principles

1. **Library-first.** Markoff exports a clean widget API. Consumers create
   an `Editor`, call `setPlainText()`, connect signals. No vault paths,
   no note models, no file I/O.

2. **Markdown is the document.** The source of truth is always flat markdown
   text. Scene items, formatted documents, and rendered images are derived
   views that can be rebuilt at any time.

3. **GPL harvest.** We fork Qt's GPL source directly when we need control
   beyond the public API. The TextControl fork gives us full ownership of
   the text editing state machine without private header dependencies.

4. **Heterogeneous blocks.** A markdown document contains text, tables,
   code blocks, math, images, diagrams. These are fundamentally different
   visual objects. The architecture represents them as separate scene items,
   not as regions within a single QTextDocument.

---

## System Diagram

```
                         +---------------------------+
                         |       Host Application    |
                         |  (Corbomite, test-app)    |
                         +---------------------------+
                                     |
                         setPlainText, setTheme, signals
                                     |
                         +---------------------------+
                         |    Editor : QGraphicsView  |  <-- Public API
                         +---------------------------+
                                     |
                    +----------------+----------------+
                    |                                 |
          +---------+----------+          +----------+---------+
          |   SelectionScene   |          |  SceneCoordinator  |
          |  (QGraphicsScene)  |          |     (QObject)      |
          +---------+----------+          +----------+---------+
                    |                                 |
          +---------+----------+        +-------------+----------+
          | SelectionManager   |        |  TreeSitterParser      |
          |  cross-boundary    |        |  MarkdownSplitter      |
          |  mouse/keyboard    |        |  item create/destroy   |
          +--------------------+        |  repositioning         |
                                        +-----------+------------+
                                                    |
                    +-------------------------------+-----+
                    |                 |                    |
          +---------+-------+  +-----+--------+  +-------+--------+
          | MarkdownTextItem|  | TableBlockItem|  | (future items) |
          | (SelectableItem)|  | (SelectableItem) | ImageItem, etc |
          +--------+--------+  +-----+--------+  +----------------+
                   |                  |
          +--------+--------+        |
          |   TextControl   |   custom QPainter
          |  (Qt fork)      |   rendering
          +--------+--------+
                   |
          +--------+--------+
          |  QTextDocument  |
          |  + Highlighter  |
          |  + MathObject   |
          +--------+--------+
                   |
          +--------+--------+
          |  MathRenderer   |
          |  (JKQTMathText) |
          +-----------------+
```

---

## Layers

### Layer 1: Public API

Seven headers in `include/markoff/`:

| Header | Type | Purpose |
|--------|------|---------|
| `Editor.h` | Widget | Main editor widget (QGraphicsView) |
| `ReadingView.h` | Widget | Non-editable rendered view |
| `Document.h` | Model | Parsed markdown with query API |
| `Theme.h` | Config | Colors, fonts, element formats |
| `EditorSettings.h` | Config | Tab size, line numbers, wrapping |
| `RenderSettings.h` | Config | Max width, margins, feature toggles |
| `ResourceProvider.h` | Interface | Image/link/embed resolution |

The public API deliberately hides all scene internals. Consumers never
see SelectionScene, SceneCoordinator, MarkdownTextItem, or any other
implementation class.

### Layer 2: Scene Architecture

**Editor** (QGraphicsView) owns a SelectionScene and a SceneCoordinator.

**SelectionScene** (QGraphicsScene) contains the ordered list of scene items
and owns the SelectionManager. It intercepts mouse/keyboard events and
delegates cross-boundary selection logic to the SelectionManager.

**SceneCoordinator** (QObject) orchestrates the scene:
- Parses markdown with TreeSitterParser
- Splits at block boundaries with MarkdownSplitter
- Creates/destroys MarkdownTextItem and BlockItem instances
- Positions items vertically with spacing
- Debounces reparse on text changes (150ms timer)
- Propagates theme, font, and width changes to all items

**SelectionManager** (QObject) handles selection across item boundaries:
- Three modes: None, WithinItem, CrossBoundary
- Uses `ungrabMouse()` to break Qt's implicit grab on boundary crossing
- Applies selection via the SelectableItem interface
- Serializes selected markdown for clipboard

### Layer 3: Scene Items

All scene items implement **SelectableItem** — a pure interface for
cross-boundary selection. Text items provide `hitTest`, `setSelection`,
`selectedMarkdown`. Non-text items provide `setFullySelected`, `toMarkdown`.

**MarkdownTextItem** (QGraphicsObject + SelectableItem):
- Wraps TextControl (forked QWidgetTextControl) + QTextDocument
- Each item has its own MarkdownHighlighter instance
- Registers MathTextObject for inline LaTeX glyph rendering
- Detects and paints DecoratedRanges (code block backgrounds, callout
  borders, blockquote indicators)
- Emits `cursorAtBoundary(Qt::Edge)` for focus transfer between items

**BlockItem** (QGraphicsObject + SelectableItem) base class:
- Paints selection overlay when fully selected
- Subclasses: TableBlockItem (read-only table rendering)

### Layer 4: Text Editing

**TextControl** — forked from Qt's QWidgetTextControl. Provides the
complete text editing state machine: cursor, selection, undo/redo,
clipboard, IME, drag-and-drop. Operates on a standard QTextDocument.

The fork replaced Qt's pimpl macros with plain nested structs, broke
the QWidgetPrivate inheritance chain, and uses only QAbstractScrollArea's
public API. No private Qt headers required.

### Layer 5: Parsing

**TreeSitterParser** — wraps the tree-sitter C API with vendored
tree-sitter-markdown grammars (with wiki-link and tag extensions). Produces:
- Flat span map (QList<SourceSpan>) for the highlighter
- Block boundaries for the splitter (tables, fenced code blocks)
- UTF-8/char offset mapping

**DocumentBuilder** — MD4C-based SAX parser that builds the Document AST.
Currently powers the reading view (Renderer) and Document query API.
*Planned for removal* once Renderer migrates to tree-sitter.

**MarkdownSplitter** — uses TreeSitterParser to find block boundaries and
split markdown into segments (Text, Table, FencedCodeBlock). Each segment
becomes a scene item.

### Layer 6: Rendering

**MarkdownHighlighter** (QSyntaxHighlighter) — driven by the tree-sitter
span map. Applies Theme formats to text ranges. Handles:
- All inline formatting (bold, italic, code, math, links, etc.)
- Block-level state (code blocks, frontmatter, blockquotes)
- Cursor-aware delimiter visibility (hides `**`, `*`, etc. away from cursor)
- Code block syntax highlighting via KSyntaxHighlighting

**MathTextObject** (QTextObjectInterface) — registered with QTextDocument
to render inline math as custom glyphs (U+FFFC replacement character).

**MathRenderer** — stateless utility that renders LaTeX to QImage via
JKQTMathText. Process-wide thread-safe cache keyed by
(latex, displayMode, fontSize, devicePixelRatio). Images supersampled at
3x DPI.

**Renderer** — walks the Document AST and produces a QTextDocument with
HTML content for the ReadingView. Currently uses MD4C-based Document AST.

---

## Data Flow: Keystroke to Pixel

```
Keystroke
  |
  v
Editor::keyPressEvent
  |
  v
QGraphicsView -> SelectionScene -> MarkdownTextItem::keyPressEvent
  |
  v
TextControl::processEvent -> QTextDocument mutation
  |
  +---> MarkdownTextItem::onCursorPositionChanged
  |       |
  |       +---> snapCursorPastDelimiters (skip hidden syntax)
  |       +---> updateMathReveal (expand/collapse math regions)
  |       +---> MarkdownHighlighter::setCursorPosition
  |
  +---> SceneCoordinator::onItemTextChanged
          |
          +---> [150ms debounce]
          |
          v
        SceneCoordinator::reparse
          |
          +---> TreeSitterParser::parse (full AST rebuild)
          +---> MarkdownSplitter::split (find new block boundaries)
          +---> Rebuild scene if boundaries changed
          +---> MarkdownHighlighter::setSpanMap (new formatting data)
          +---> MarkdownTextItem::refreshMathSubstitution
                  |
                  +---> stripMathSubstitution (remove U+FFFC glyphs)
                  +---> applyMathSubstitution (insert fresh glyphs)
```

Between full reparses, `MarkdownHighlighter::adjustSpanOffsets()`
incrementally shifts span byte offsets as the user types, keeping
highlighting approximately correct without waiting for the debounce.

---

## Math Rendering Pipeline

Inline math (`$x^2$`) and display math (`$$\int_0^1 f(x) dx$$`) follow
the same pipeline:

1. **Detection**: TreeSitterParser marks spans with `math` or `mathDisplay`
2. **Substitution**: `applyMathSubstitution()` replaces each math source
   range with a single U+FFFC character carrying format properties:
   - `SourceProperty`: the LaTeX source (`x^2`)
   - `DisplayProperty`: inline vs display mode
   - `RawProperty`: original delimited form (`$x^2$`)
3. **Rendering**: Qt's layout engine calls `MathTextObject::intrinsicSize()`
   and `drawObject()` for each U+FFFC. These delegate to
   `MathRenderer::render()`, which compiles the LaTeX and returns a cached
   QImage.
4. **Cursor reveal**: When the cursor enters a math region,
   `updateMathReveal()` expands the U+FFFC back to raw LaTeX so the user
   can edit it. On cursor exit, it re-substitutes the glyph.

**Open question**: The reveal/collapse mechanism is the most complex
subsystem in the codebase (~300 lines with reentrancy guards and deferred
flag clearing). Should we explore a simpler model — e.g., always showing
source in the focused text item and only substituting in unfocused items?
This would trade per-glyph reveal for per-item reveal, dramatically
simplifying the state machine at the cost of slightly different UX from
Obsidian.

---

## Theme System

The Theme struct contains a `QHash<Element, QTextCharFormat>` with 57
element types covering all markdown constructs, plus base text and code
fonts.

Themes propagate through: `Editor::setTheme()` -> `SceneCoordinator::setTheme()`
-> each item's `MarkdownHighlighter::setTheme()` -> immediate rehighlight.

Factory methods: `Theme::defaultLight()`, `Theme::defaultDark()`,
`Theme::fromSchemeFile()` (QOwnNotes INI format).

**Open question**: Should the Theme system absorb the hardcoded visual
constants currently scattered across rendering code (table grid colors,
code block backgrounds, selection overlay, callout type colors)? This
would make themes fully control all visual output but adds ~20 new
Element values or a separate decoration color map.

**Open question**: Should `Theme::fromSchemeFile()` be extended to read
KDE color schemes (Breeze, etc.) for native desktop integration?

---

## Mode Switching

**Source mode**: SceneCoordinator creates a single MarkdownTextItem
containing the entire markdown document. No splitting, no block items.
Raw markdown visible with syntax highlighting.

**LivePreview mode**: SceneCoordinator splits at block boundaries. Each
text segment becomes a MarkdownTextItem with its own highlighter. Tables
become TableBlockItems. Syntax delimiters (`**`, `*`, `#`, etc.) are
hidden away from the cursor.

Mode switching serializes all items back to flat markdown, then rebuilds
the scene in the new mode.

---

## Current Block Item Types

| Type | Status | Capabilities |
|------|--------|-------------|
| MarkdownTextItem | Shipped | Full editing, highlighting, math, decorated ranges |
| TableBlockItem | Shipped (read-only) | Custom-painted table grid, column alignment |

## Planned Block Item Types

| Type | Purpose | Key Questions |
|------|---------|---------------|
| Editable table | Cell editing, row/column ops | Per-cell QTextDocument + QTextLayout, or simpler model? Context menu vs hover handles? |
| ImageItem | Rendered image with resize | How to handle missing images? Lazy loading for large vaults? |
| CodeBlockItem | Syntax-highlighted code editing | Separate item or just a decorated MarkdownTextItem with enhanced rendering? |
| MathItem | Display math as rendered block | Separate item or handled by existing MathTextObject in text items? |
| MermaidItem | SVG diagram rendering | External renderer dependency? Async rendering? |
| EmbedItem | Embedded note preview | Recursive rendering depth limit? |

**Open question**: Not all of these necessarily need to be separate
BlockItem types. Code blocks and display math currently work well as
decorated regions within MarkdownTextItem. The split-into-separate-item
approach is necessary when the visual representation has fundamentally
different geometry from the source text (tables, images). For code blocks,
the source text IS the visual text — just with different styling. Should
we only split items for geometrically-incompatible blocks?

---

## Parsing: Dual Parser Situation

Markoff currently has two parsers:

| Parser | Drives | Used For |
|--------|--------|----------|
| tree-sitter-markdown | Editor pipeline | Span map, block boundaries, delimiter positions |
| MD4C (via DocumentBuilder) | Reading view | Document AST, query API (headings, links, tags) |

This means every markdown feature must be implemented twice. The path
forward is to migrate Renderer to walk the tree-sitter CST, then delete
MD4C, DocumentBuilder, DocumentBuilder_p.h, and SourceSpan (the gap-based
span builder).

**Open question**: The Document query API (headings, links, wikilinks,
tags, footnotes, word count) currently depends on the MD4C-based AST. Can
tree-sitter provide equivalent queries? Tree-sitter's CST has explicit
nodes for headings, links, etc., but the query ergonomics differ from
walking a typed AST. Should Document internally use tree-sitter queries,
or should it build its own lightweight AST from the CST?

**Open question**: tree-sitter supports incremental parsing via
`ts_tree_edit()`. The current implementation does a full reparse on every
debounced keystroke. At what document size does this become a bottleneck?
Should incremental parsing be prioritized, or is 150ms debounce + full
reparse fast enough for typical note sizes (< 10,000 lines)?

---

## Obsidian Grammar Extensions

The vendored tree-sitter-markdown grammar supports wiki links and tags
via compile-time flags. Several Obsidian-flavored constructs are NOT
yet in the grammar:

- `==highlighted text==` — currently detected by DocumentBuilder's Layer 2
  post-processing, which will be deleted when MD4C is removed
- `%%comment text%%` — same situation
- `![[embed]]` — embed prefix on wikilinks
- `^block-id` — block references
- `> [!type]` — callout blocks (partially handled by DecoratedRange
  detection in MarkdownTextItem)

**Open question**: Fork the vendored grammar to add these nodes, or
handle them as a post-processing pass over the tree-sitter CST? Forking
gives clean AST nodes but requires maintaining a grammar fork and
regenerating with the tree-sitter CLI (Node.js toolchain). Post-processing
is simpler but repeats the pattern we're trying to eliminate.

---

## Cross-Boundary Selection

The SelectionManager handles selection spanning multiple scene items:

1. **Mouse press** records anchor item + char position
2. **Mouse drag within item** is handled natively by Qt (WithinItem mode)
3. **Mouse drag across boundary** breaks the grab, enters CrossBoundary mode
4. **applySelection()** walks ordered items, setting partial selection on
   edge items and full selection on middle items
5. **Ctrl+C** serializes selected markdown from each item in order
6. **Ctrl+A** selects all items
7. **Escape** clears cross-boundary selection

Keyboard selection (Shift+Arrow at item boundary) extends cross-boundary
selection by detecting `cursorAtBoundary` signals and programmatically
extending into adjacent items.

---

## What's Not Built Yet

Roughly ordered by dependency and value:

1. **Remove MD4C** — migrate Renderer to tree-sitter, collapse dual parser
2. **Incremental tree-sitter parsing** — `ts_tree_edit()` for keystroke-level
   performance on large documents
3. **Incremental rehighlight** — only rehighlight blocks whose spans changed
4. **Theme-driven visual constants** — move hardcoded colors/layout values
   into Theme/EditorSettings
5. **Editable tables** — the biggest missing interactive feature
6. **In-editor image rendering** — ImageItem with ResourceProvider
7. **Horizontal rules as graphical lines** — not just styled `---`
8. **Task list checkbox widgets** — clickable toggle, not unicode chars
9. **Blockquote left border** — visual indicator beyond color
10. **Obsidian grammar fork** — native AST nodes for highlights, comments,
    embeds, block refs, callouts
11. **Highlighter test suite** — tst_highlighter.cpp
12. **Performance benchmarks** — large document stress tests
13. **Accessibility** — screen reader, keyboard-only navigation

---

## Dependencies

| Dependency | Version | Purpose | Notes |
|-----------|---------|---------|-------|
| Qt6 | 6.8+ | GUI framework | Core, Gui, Widgets |
| KF6SyntaxHighlighting | KF6 | Code block syntax coloring | |
| tree-sitter | system | Incremental parser framework | via pkg-config |
| tree-sitter-markdown | vendored | Markdown grammar + inline grammar | with EXTENSION_WIKI_LINK, EXTENSION_TAGS |
| MD4C | system | SAX markdown parser | *Planned for removal* |
| JKQTMathText | sibling lib | LaTeX math rendering | |

---

## File Map

```
libs/markoff/
+-- include/markoff/           # Public API (7 headers)
|   +-- Editor.h               # QGraphicsView widget
|   +-- Document.h             # Parsed document + query API
|   +-- ReadingView.h          # Non-editable rendered view
|   +-- Theme.h                # Colors, fonts, element formats
|   +-- EditorSettings.h       # Editor behavior config
|   +-- RenderSettings.h       # Rendering config
|   +-- ResourceProvider.h     # Image/link/embed resolution interface
|
+-- src/                       # Implementation (~11,000 lines)
|   +-- Editor.cpp             # QGraphicsView widget implementation
|   +-- SceneCoordinator.h/cpp # Scene item management, reparse
|   +-- SelectionScene.h/cpp   # QGraphicsScene + SelectionManager delegation
|   +-- SelectionManager.h/cpp # Cross-boundary selection state machine
|   +-- SelectableItem.h       # Item interface for selection
|   +-- MarkdownTextItem.h/cpp # Editable text region (TextControl + QTextDocument)
|   +-- BlockItem.h/cpp        # Non-text item base class
|   +-- TableBlockItem.h/cpp   # Read-only table rendering
|   +-- StubBlockItem.h/cpp    # Minimal block for testing
|   +-- TextControl.h/cpp      # Forked from Qt's QWidgetTextControl
|   +-- TextControl_p.h        # Private implementation details
|   +-- MarkdownHighlighter.h/cpp  # AST-driven syntax highlighting
|   +-- TreeSitterParser.h/cpp # tree-sitter C API wrapper
|   +-- MarkdownSplitter.h/cpp # Split markdown at block boundaries
|   +-- SourceSpan.h/cpp       # Span map from AST (planned for removal with MD4C)
|   +-- Document.cpp           # Document model implementation
|   +-- DocumentBuilder.cpp    # MD4C SAX builder (planned for removal)
|   +-- DocumentBuilder_p.h    # Internal types (planned for removal)
|   +-- Renderer.cpp           # Document AST -> QTextDocument HTML
|   +-- ReadingView.cpp        # ReadingView widget
|   +-- Theme.cpp              # Theme factory methods and defaults
|   +-- MathRenderer.h/cpp     # LaTeX -> QImage via JKQTMathText
|   +-- MathTextObject.h/cpp   # QTextObjectInterface for inline math
|   +-- DecoratedRange.h/cpp   # Code block/callout/blockquote ranges
|   +-- TableHandler.h/cpp     # Pipe table parsing and serialization
|   +-- ResourceProvider.cpp   # Filesystem resource provider
|   +-- MarkoffBlockData.h     # Per-block user data
|   +-- vendor/
|       +-- tree-sitter-markdown/  # Vendored grammar (will be forked)
|
+-- tests/                     # 12 test executables
+-- app/                       # Test application + scene demo
+-- docs/                      # This file, specs, plans, research
    +-- archive/               # Superseded specs and plans
```

---

## Documentation Map

| Document | Purpose |
|----------|---------|
| `architecture.md` | This file — current system description |
| `TODO.md` | Active backlog — the most up-to-date task list |
| `2026-04-13-codebase-audit.md` | Comprehensive code quality audit |
| `01-problem-definition.md` | Why Markoff exists |
| `02-parser-survey.md` | Parser evaluation (MD4C, cmark, tree-sitter) |
| `03-editor-architecture-survey.md` | Editor widget approaches |
| `04-reference-codebase-analysis.md` | Analysis of reference projects |
| `05-options-and-tradeoffs.md` | Design decision rationale |
| `06-qt-source-harvest.md` | Fork strategy for Qt widgets |
| `obsidian-editor-internals.md` | Obsidian's CodeMirror architecture |
| `obsidian-editor-ux-reference.md` | Obsidian UX behavior catalog |
| `specs/` | Design specs (implemented ones marked at top) |
| `plans/` | Implementation plans (implemented ones marked at top) |
| `archive/` | Superseded specs from earlier architectural approaches |
| `appendix-*.md` | External research (vaults, repos, APIs) |
