# Markoff Parser/Editor Split and MD4C Removal

## Overview

Split the monolithic markoff library into two independent libraries and
remove the MD4C parser, ReadingView widget, Renderer, and Source/LivePreview
mode distinction.

**MarkoffParser** — a standalone Qt/C++ Obsidian-flavored markdown parser
wrapping tree-sitter-markdown. Depends only on Qt6::Core and tree-sitter.
Provides a typed query API for headings, links, wikilinks, tags, footnotes,
and word count. Useful independently of the editor for vault scanning,
backlink resolution, and metadata extraction.

**MarkoffEditor** — the editor widget. Depends on MarkoffParser plus
Qt6::Widgets, KF6SyntaxHighlighting, and JKQTMathText. Always operates in
live preview mode (the only mode). Read-only display is the same widget
with editing disabled.

**Deleted:** MD4C parser, DocumentBuilder, Renderer, ReadingView,
RenderSettings, Source/LivePreview Mode enum.

---

## Motivation

1. **Dual parser redundancy.** MD4C and tree-sitter coexist, serving
   overlapping roles. Every markdown feature must be implemented twice.
   Tree-sitter is strictly more capable (incremental parsing, explicit
   delimiter nodes, Obsidian extension support).

2. **Unnecessary complexity.** ReadingView is a separate widget with its
   own Renderer that walks a separate AST to produce HTML. The editor
   already renders markdown beautifully in live preview mode. A read-only
   display is just the editor with editing disabled.

3. **Mode switching is overhead.** Source mode (single text item, raw
   markdown) vs LivePreview mode (split items, formatted) forces
   SceneCoordinator to maintain two code paths and Editor to carry a
   Mode enum, Q_PROPERTY, and mode-switching logic. There's one good
   mode: live preview.

4. **Library reuse.** The tree-sitter parser with Obsidian extensions and
   a Qt-native query API is independently valuable. No equivalent exists
   in the Qt/C++ ecosystem.

---

## MarkoffParser Library

### Location

`libs/markoff-parser/`

### Target

`MarkoffParser::MarkoffParser` (static library, position-independent)

### Dependencies

- Qt6::Core (QString, QList, QByteArray, QRegularExpression)
- tree-sitter (system, via pkg-config)
- tree-sitter-markdown (vendored, with EXTENSION_WIKI_LINK, EXTENSION_TAGS)

No Qt Widgets, no Qt Gui, no KDE Frameworks. Pure data library.

Both libraries share `namespace Markoff` — they are the same family,
split for dependency reasons.

### Public Headers

```
include/markoff-parser/
+-- Document.h          # Parsed document + query API
+-- TreeSitterParser.h  # Parser with span map and block boundary detection
+-- SourceSpan.h        # Span data structure + UTF-8 offset utilities
+-- MarkdownSplitter.h  # Block boundary splitting (used by editor's SceneCoordinator)
+-- TableHandler.h      # Pipe table parsing/serialization (used by editor's TableBlockItem)
```

MarkdownSplitter and TableHandler are public headers because the editor
library is a legitimate consumer. They could also serve standalone use
cases (splitting markdown for batch processing, table extraction).

### Internal Sources

```
src/
+-- Document.cpp
+-- TreeSitterParser.cpp
+-- SourceSpan.cpp          # buildUtf8ToCharMap() only; old MD4C buildSpanMap() deleted
+-- MarkdownSplitter.cpp
+-- TableHandler.cpp
+-- vendor/tree-sitter-markdown/
```

### Document.h Public API

The public interface is unchanged from the current markoff Document.h:

```cpp
namespace Markoff {

struct HeadingInfo { int level; QString text; int sourceOffset; };
struct LinkInfo {
    enum Type { Standard, Wiki, Image, Embed };
    Type type; QString target; QString displayText; int sourceOffset;
};
struct TagInfo { QString name; int sourceOffset; };
struct FootnoteInfo { int number; QString label; QString content; };

class Document {
public:
    ~Document();
    static std::unique_ptr<Document> fromMarkdown(const QString &source);

    QString sourceText() const;
    bool isEmpty() const;
    QString extractSubpath(const QString &subpath) const;
    QString frontmatter() const;
    QString markdownContent() const;

    QList<HeadingInfo> headings() const;
    QList<LinkInfo> links() const;
    QList<LinkInfo> wikiLinks() const;
    QList<TagInfo> tags() const;
    QList<FootnoteInfo> footnotes() const;
    int footnoteCount() const;
    QString footnoteContent(int number) const;
    int wordCount() const;
    int characterCount() const;

private:
    Document();
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace Markoff
```

### Document Internal Rewrite

The internal storage changes from the MD4C Block tree to a TreeSitterParser
instance that owns the CST:

```cpp
struct Document::Private {
    QString source;
    QString frontmatter;          // regex-extracted, unchanged
    TreeSitterParser parser;      // owns the tree-sitter CST
    QList<Footnote> footnotes;    // regex-extracted, unchanged
};
```

`Document::fromMarkdown()` changes:
1. Frontmatter extraction — unchanged (regex on raw text)
2. Footnote extraction — unchanged (regex on raw text)
3. ~~DocumentBuilder::parse()~~ → `parser.parse(markdown)`
4. ~~DocumentBuilder::postProcess()~~ → deleted (tree-sitter handles natively)

Query methods are rewritten to walk the tree-sitter CST directly via a
new `buildDocumentQueries()` traversal on TreeSitterParser, rather than
reusing the flat span map (which is optimized for the highlighter). The
query traversal extracts structured data (heading text, link targets,
tag names) that would be awkward to reconstruct from flat spans.

#### Query Method Mapping

| Method | Current (MD4C Block tree) | New (tree-sitter CST) |
|--------|--------------------------|----------------------|
| `headings()` | Walk Blocks where `type == MD_BLOCK_H` | Find `atx_heading` nodes, extract level from `atx_h*_marker` child, collect inline text |
| `links()` | Check InlineRun.linkHref, wikiTarget, imageSrc | Find `inline_link`, `shortcut_link`, `image`, `wiki_link` nodes. Extract target from destination child. |
| `wikiLinks()` | Filter links() by type | Filter links() for Wiki and Embed types |
| `tags()` | Check InlineRun.isTag | Find `tag` nodes, strip leading `#` |
| `footnotes()` | Regex-extracted from source text | Unchanged (parser-independent regex) |
| `wordCount()` | Split markdownContent() on whitespace | Unchanged (parser-independent) |
| `characterCount()` | Length of markdownContent() | Unchanged (parser-independent) |
| `extractSubpath()` | Line-based text search | Unchanged (parser-independent) |
| `frontmatter()` | Regex-extracted from source text | Unchanged (parser-independent) |

Five of the nine query methods operate on raw source text and need zero
changes. The three AST-dependent methods (headings, links, tags) get
rewritten. `wikiLinks()` is a filter over `links()`.

### TreeSitterParser Additions

TreeSitterParser gains a query-focused traversal method:

```cpp
struct DocumentQueryResult {
    QList<HeadingInfo> headings;
    QList<LinkInfo> links;
    QList<TagInfo> tags;
};

DocumentQueryResult buildDocumentQueries() const;
```

This walks the block and inline trees once, collecting structured data.
Separate from `buildSpanMap()` which produces flat formatting ranges for
the highlighter.

### SourceSpan Changes

The `SourceSpan` struct is unchanged. The free function
`buildSpanMap(const QList<Block>&, const QByteArray&)` is deleted — it
walks the MD4C Block tree and is fully replaced by
`TreeSitterParser::buildSpanMap()`. The utility function
`buildUtf8ToCharMap()` stays (used by TreeSitterParser internally, but
also useful standalone).

### Tests (moved from markoff)

| Test | What it validates |
|------|-------------------|
| `tst_document.cpp` | Document::fromMarkdown(), sourceText(), extractSubpath() |
| `tst_document_queries.cpp` | headings(), links(), wikiLinks(), tags(), wordCount(), characterCount() |
| `tst_splitter.cpp` | MarkdownSplitter block boundary detection |
| `tst_table.cpp` | TableHandler parsing and serialization |

The document and document_queries tests are the critical validation gate.
If they pass after the tree-sitter rewrite, the parser migration is
correct.

---

## MarkoffEditor Simplification

### Location

`libs/markoff/` (stays in place)

### Target

`Markoff::Markoff` (static library, position-independent)

### Dependencies Change

```
Before:
  Qt6::Core, Qt6::Gui, Qt6::Widgets
  PkgConfig::TREESITTER, PkgConfig::MD4C
  ts-markdown, KF6::SyntaxHighlighting, jkqtmathtext

After:
  Qt6::Core, Qt6::Gui, Qt6::Widgets
  MarkoffParser::MarkoffParser
  KF6::SyntaxHighlighting, jkqtmathtext
```

tree-sitter and ts-markdown are transitive through MarkoffParser. MD4C is
gone entirely.

### Editor.h Changes

Removed:
- `enum class Mode { Source, LivePreview }` and `Q_ENUM(Mode)`
- `Q_PROPERTY(Mode mode ...)`
- `void setMode(Mode mode)` / `Mode mode() const`
- `void setRenderSettings(const RenderSettings &settings)` / `RenderSettings renderSettings() const`
- Signal: `void modeChanged(Markoff::Editor::Mode mode)`
- Members: `Mode m_mode`, `RenderSettings m_renderSettings`
- Include: `<markoff/RenderSettings.h>`

Added:
```cpp
/// Enable or disable editing. When read-only, the editor displays
/// live-preview-formatted markdown but does not accept input.
///
/// NOTE: Read-only mode does NOT prevent all user interaction with
/// non-text block items. Specifically, TableBlockItem (and future
/// interactive block items) may allow non-destructive display
/// adjustments — such as column width resizing — that affect only
/// the visual presentation and do not modify the underlying markdown.
/// These are ephemeral viewport affordances for readability, not
/// editing operations, and are not persisted or serialized.
void setReadOnly(bool readOnly);
bool isReadOnly() const;
```

### Editor.cpp Changes

- `rebuildScene()` always calls `m_coordinator->loadMarkdown(m_sourceText)`.
  The `if (m_mode == Mode::Source)` branch calling `loadSource()` is deleted.
- `toPlainText()` no longer needs a mode check — always serializes via
  `m_coordinator->toMarkdown()`.
- `setReadOnly()` stores a flag and propagates to all current and future
  MarkdownTextItem instances (via `setTextInteractionFlags()`).
  Non-text items (TableBlockItem) receive a separate flag that disables
  content editing but preserves display-adjustment interactions.

### SceneCoordinator Changes

- `loadSource()` method deleted. Only `loadMarkdown()` remains.
- No other changes.

### MarkdownHighlighter Changes

- `enum class Mode { Source, LivePreview }` deleted.
- `void setMode(Mode mode)` deleted.
- The highlighter always runs in live-preview behavior: delimiters are
  hidden when the cursor is not within their parent formatting range.
- Internal `m_mode` member and any `if (m_mode == Mode::Source)` branches
  are removed.

### Include Path Changes

All internal sources that included private markoff headers for parser types
now include from `markoff-parser/`:
- `#include "TreeSitterParser.h"` → `#include <markoff-parser/TreeSitterParser.h>`
- `#include "SourceSpan.h"` → `#include <markoff-parser/SourceSpan.h>`
- `#include "MarkdownSplitter.h"` → include from markoff-parser
- `#include "TableHandler.h"` → include from markoff-parser

### TableBlockItem Read-Only Semantics

TableBlockItem gets a TODO comment documenting the column-width-adjustment
exception:

```cpp
// TODO: When setReadOnly(true) is active, TableBlockItem should still
// allow column width adjustment via drag handles. This is an ephemeral
// display affordance for readability — it does not modify the underlying
// pipe-delimited markdown and is not persisted. The interaction model is:
//   - Drag column border to resize
//   - Widths are volatile (reset on reparse or document reload)
//   - No undo/redo entry is created
//   - toMarkdown() output is unaffected
// This applies to all future interactive block items that offer similar
// non-destructive display adjustments.
```

### Deleted Public Headers

| Header | Reason |
|--------|--------|
| `include/markoff/ReadingView.h` | No separate reading view widget |
| `include/markoff/RenderSettings.h` | Only used by deleted Renderer/ReadingView |
| `include/markoff/Document.h` | Moved to markoff-parser |

### Deleted Internal Sources

| File | Reason |
|------|--------|
| `src/ReadingView.cpp` | Widget deleted |
| `src/Renderer.h` | Rendering pipeline deleted |
| `src/Renderer.cpp` | Rendering pipeline deleted |
| `src/DocumentBuilder.cpp` | MD4C wrapper replaced by tree-sitter |
| `src/DocumentBuilder_p.h` | MD4C types (Block, InlineRun, DocumentBlockAccessor) no longer needed |

Note: `DocumentBlockAccessor` (the friend struct that gave Renderer and
SourceSpan access to Document's internal block list) is deleted along
with DocumentBuilder_p.h. Its only consumers were Renderer and the old
MD4C-based `buildSpanMap()`, both of which are gone.

### Deleted Tests

| Test | Reason |
|------|--------|
| `tst_renderer.cpp` | Renderer deleted |

### Remaining Editor Tests

| Test | What it validates |
|------|-------------------|
| `tst_inline_math.cpp` | MathTextObject substitution and reveal |
| `tst_editor_formatting.cpp` | Bold, italic, heading, code toggle actions |
| `tst_selection.cpp` | Cross-boundary SelectionManager |
| `tst_theme.cpp` | Theme construction and element formats |
| `tst_resourceprovider.cpp` | Resource path resolution |

---

## Corbomite Host Changes

### MarkoffRenderEngine (stub)

`libs/core/src/MarkoffRenderEngine.cpp` is replaced with a deprecated
stub that returns raw markdown as plain text:

```cpp
// DEPRECATED: Stub implementation. Returns raw markdown as a plain
// QTextDocument. The Markoff rendering pipeline has been removed.
// Canvas card rendering needs a new approach (e.g., offscreen Editor
// widget or a dedicated card renderer).
std::unique_ptr<RenderedDocument> MarkoffRenderEngine::render(
    const QString &markdown, const RenderOptions &options) const
{
    QString md = markdown;
    if (!options.subpath.isEmpty())
        md = extractSubpath(markdown, options.subpath);

    auto doc = std::make_unique<QTextDocument>();
    doc->setPlainText(md);
    return RenderedDocument::fromQTextDocument(std::move(doc));
}
```

All markoff includes are removed except `<QTextDocument>`. The
`libs/core/CMakeLists.txt` private include path into `markoff/src/`
(used to reach `Renderer.h`) is removed. If `libs/core` still needs
`Document.h` for `extractSubpath()`, it links `MarkoffParser::MarkoffParser`
instead. Otherwise that call can be inlined as a simple text operation.

### NoteEditorWidget

`src/editor/NoteEditorWidget.h` changes:
- Remove `Markoff::ReadingView` forward declaration
- Collapse `ViewMode` enum: `Source` and `LivePreview` merge into a single
  editing state. `Reading` calls `m_editor->setReadOnly(true)`.
- Remove `m_readingView` member, `m_modeStack` (QStackedWidget).
- The editor widget is the only child.

`src/editor/NoteEditorWidget.cpp` changes:
- Remove `#include <markoff/ReadingView.h>`
- Remove ReadingView construction, signal connections, setMarkdown calls,
  setResourceProvider forwarding to ReadingView
- `setViewMode(Reading)` → `m_editor->setReadOnly(true)`
- `setViewMode(other)` → `m_editor->setReadOnly(false)`
- Remove QStackedWidget — layout directly contains `m_editor`

---

## File Structure After Refactoring

```
libs/markoff-parser/
+-- CMakeLists.txt
+-- include/markoff-parser/
|   +-- Document.h
|   +-- TreeSitterParser.h
|   +-- SourceSpan.h
|   +-- MarkdownSplitter.h
|   +-- TableHandler.h
+-- src/
|   +-- Document.cpp
|   +-- TreeSitterParser.cpp
|   +-- SourceSpan.cpp
|   +-- MarkdownSplitter.cpp
|   +-- TableHandler.cpp
|   +-- vendor/tree-sitter-markdown/
+-- tests/
    +-- CMakeLists.txt
    +-- tst_document.cpp
    +-- tst_document_queries.cpp
    +-- tst_splitter.cpp
    +-- tst_table.cpp

libs/markoff/
+-- CMakeLists.txt
+-- include/markoff/
|   +-- Editor.h
|   +-- Theme.h
|   +-- EditorSettings.h
|   +-- ResourceProvider.h
+-- src/
|   +-- Editor.cpp
|   +-- SceneCoordinator.h/cpp
|   +-- SelectionScene.h/cpp
|   +-- SelectionManager.h/cpp
|   +-- SelectableItem.h
|   +-- MarkdownTextItem.h/cpp
|   +-- BlockItem.h/cpp
|   +-- TableBlockItem.h/cpp
|   +-- StubBlockItem.h/cpp
|   +-- TextControl.h/cpp
|   +-- TextControl_p.h
|   +-- MarkdownHighlighter.h/cpp
|   +-- MathRenderer.h/cpp
|   +-- MathTextObject.h/cpp
|   +-- DecoratedRange.h/cpp
|   +-- ResourceProvider.cpp
|   +-- Theme.cpp
|   +-- MarkoffBlockData.h
+-- tests/
|   +-- CMakeLists.txt
|   +-- tst_inline_math.cpp
|   +-- tst_editor_formatting.cpp
|   +-- tst_selection.cpp
|   +-- tst_theme.cpp
|   +-- tst_resourceprovider.cpp
|   +-- showcase.md
+-- app/
    +-- main.cpp
    +-- MainWindow.h/cpp
    +-- scene-demo/
```

---

## Risk Assessment

### Document query regression

The document_queries test suite is the safety net. If `headings()`,
`links()`, `tags()`, `wordCount()` return identical results from
tree-sitter as from MD4C, the migration is correct. Run these tests
early and often during implementation.

### Tree-sitter node type coverage

Tree-sitter-markdown may not have explicit nodes for every Obsidian
construct (footnote definitions, block IDs). Footnotes are already
handled by regex preprocessing (parser-independent). Block IDs in
`extractSubpath()` are handled by line-based text search
(parser-independent). No tree-sitter coverage gap for current queries.

### Include path churn

Every file in markoff that imports parser types needs its include paths
updated. This is mechanical but touches many files. Do it in one pass.

### NoteEditorWidget breakage

The ViewMode simplification touches signal connections and widget
construction. Test that vault opening, note switching, and cursor
position reporting still work after the changes.

---

## Implementation Order

1. Create `libs/markoff-parser/` with CMake target and move parser files
2. Implement `TreeSitterParser::buildDocumentQueries()`
3. Rewrite `Document.cpp` to use TreeSitterParser instead of DocumentBuilder
4. Validate: `tst_document` and `tst_document_queries` pass
5. Update markoff CMakeLists.txt to depend on MarkoffParser, remove MD4C
6. Delete DocumentBuilder, Renderer, ReadingView, RenderSettings from markoff
7. Remove Mode enum from Editor, MarkdownHighlighter, SceneCoordinator
8. Add `setReadOnly()`/`isReadOnly()` to Editor
9. Add TableBlockItem read-only column-width TODO comment
10. Update all include paths in markoff sources
11. Stub MarkoffRenderEngine in Corbomite
12. Simplify NoteEditorWidget (remove ReadingView, collapse ViewMode)
13. Build and run all tests
