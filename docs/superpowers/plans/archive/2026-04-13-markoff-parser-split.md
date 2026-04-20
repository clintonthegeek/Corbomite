# Markoff Parser/Editor Split Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split monolithic markoff into MarkoffParser (standalone tree-sitter parser library) and MarkoffEditor (widget), removing MD4C, ReadingView, Renderer, RenderSettings, and Mode enum.

**Architecture:** MarkoffParser wraps tree-sitter-markdown with Obsidian extensions; depends only on Qt6::Core + tree-sitter. MarkoffEditor depends on MarkoffParser + Qt6::Widgets + KF6 + JKQTMathText. Document query API rebuilt on tree-sitter CST traversal. Read-only mode replaces ReadingView.

**Tech Stack:** C++20, Qt6, tree-sitter, KDE Frameworks 6 (SyntaxHighlighting), JKQTMathText, CMake

**Spec:** `docs/superpowers/specs/2026-04-13-markoff-parser-split-design.md`

**Important context:**
- Build from repo root: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build`
- Run markoff tests: `cd build && ctest -R tst_markoff --output-on-failure`
- Do NOT create feature branches — work directly on master
- The existing `tst_document_queries` tests are the critical validation gate for the parser migration

---

## File Map

### New Files (MarkoffParser library)

| File | Responsibility |
|------|---------------|
| `libs/markoff-parser/CMakeLists.txt` | Static library target `MarkoffParser::MarkoffParser` |
| `libs/markoff-parser/include/markoff-parser/Document.h` | Moved from markoff; public API unchanged |
| `libs/markoff-parser/include/markoff-parser/TreeSitterParser.h` | Promoted from private to public |
| `libs/markoff-parser/include/markoff-parser/SourceSpan.h` | Promoted; struct + `buildUtf8ToCharMap()` only |
| `libs/markoff-parser/include/markoff-parser/MarkdownSplitter.h` | Moved from markoff src/ to public |
| `libs/markoff-parser/include/markoff-parser/TableHandler.h` | Moved from markoff src/ to public |
| `libs/markoff-parser/src/Document.cpp` | Rewritten: tree-sitter CST instead of MD4C |
| `libs/markoff-parser/src/TreeSitterParser.cpp` | Moved; gains `buildDocumentQueries()` |
| `libs/markoff-parser/src/SourceSpan.cpp` | Moved; MD4C `buildSpanMap()` deleted |
| `libs/markoff-parser/src/MarkdownSplitter.cpp` | Moved from markoff |
| `libs/markoff-parser/src/TableHandler.cpp` | Moved from markoff |
| `libs/markoff-parser/src/vendor/tree-sitter-markdown/` | Moved from markoff |
| `libs/markoff-parser/tests/CMakeLists.txt` | Tests for parser library |
| `libs/markoff-parser/tests/tst_document.cpp` | Moved from markoff |
| `libs/markoff-parser/tests/tst_document_queries.cpp` | Moved from markoff |
| `libs/markoff-parser/tests/tst_splitter.cpp` | Moved from markoff |
| `libs/markoff-parser/tests/tst_table.cpp` | Moved from markoff |

### Deleted Files

| File | Reason |
|------|--------|
| `libs/markoff/include/markoff/Document.h` | Moved to markoff-parser |
| `libs/markoff/include/markoff/ReadingView.h` | Widget deleted |
| `libs/markoff/include/markoff/RenderSettings.h` | Only used by deleted Renderer |
| `libs/markoff/src/ReadingView.cpp` | Widget deleted |
| `libs/markoff/src/Renderer.h` | Rendering pipeline deleted |
| `libs/markoff/src/Renderer.cpp` | Rendering pipeline deleted |
| `libs/markoff/src/DocumentBuilder.cpp` | MD4C wrapper replaced |
| `libs/markoff/src/DocumentBuilder_p.h` | MD4C types no longer needed |
| `libs/markoff/src/SourceSpan.h` | Moved to markoff-parser |
| `libs/markoff/src/SourceSpan.cpp` | Moved to markoff-parser |
| `libs/markoff/src/TreeSitterParser.h` | Moved to markoff-parser |
| `libs/markoff/src/TreeSitterParser.cpp` | Moved to markoff-parser |
| `libs/markoff/src/MarkdownSplitter.h` | Moved to markoff-parser |
| `libs/markoff/src/MarkdownSplitter.cpp` | Moved to markoff-parser |
| `libs/markoff/src/TableHandler.h` | Moved to markoff-parser |
| `libs/markoff/src/TableHandler.cpp` | Moved to markoff-parser |
| `libs/markoff/src/vendor/tree-sitter-markdown/` | Moved to markoff-parser |
| `libs/markoff/tests/tst_renderer.cpp` | Renderer deleted |
| `libs/markoff/tests/tst_document.cpp` | Moved to markoff-parser |
| `libs/markoff/tests/tst_document_queries.cpp` | Moved to markoff-parser |
| `libs/markoff/tests/tst_splitter.cpp` | Moved to markoff-parser |
| `libs/markoff/tests/tst_table.cpp` | Moved to markoff-parser |

### Modified Files

| File | Changes |
|------|---------|
| `libs/markoff/CMakeLists.txt` | Remove MD4C/tree-sitter deps, removed sources; add MarkoffParser dep |
| `libs/markoff/include/markoff/Editor.h` | Remove Mode enum, RenderSettings; add setReadOnly/isReadOnly |
| `libs/markoff/src/Editor.cpp` | Remove mode logic; add setReadOnly impl |
| `libs/markoff/src/SceneCoordinator.h` | Remove loadSource(), remove MarkdownHighlighter::Mode param |
| `libs/markoff/src/SceneCoordinator.cpp` | Remove loadSource(), update include paths, always LivePreview |
| `libs/markoff/src/MarkdownHighlighter.h` | Remove Mode enum, setMode(), m_mode |
| `libs/markoff/src/MarkdownHighlighter.cpp` | Remove mode branches |
| `libs/markoff/src/MarkdownTextItem.cpp` | Update SourceSpan include path |
| `libs/markoff/src/TableBlockItem.h` | Add read-only column-width TODO comment |
| `libs/markoff/src/TableBlockItem.cpp` | Update TableHandler include path |
| `libs/markoff/tests/CMakeLists.txt` | Remove moved/deleted test targets |
| `libs/core/src/MarkoffRenderEngine.cpp` | Stub: return raw markdown plain text |
| `libs/core/CMakeLists.txt` | Remove private markoff/src include path |
| `src/editor/NoteEditorWidget.h` | Remove ReadingView, collapse ViewMode |
| `src/editor/NoteEditorWidget.cpp` | Remove ReadingView, use setReadOnly for Reading |
| `CMakeLists.txt` (root) | Add `add_subdirectory(libs/markoff-parser)` before markoff |

---

### Task 1: Create MarkoffParser library skeleton with moved files

This task creates the new library directory, moves parser files physically, sets up CMake, and verifies the parser library builds independently.

**Files:**
- Create: `libs/markoff-parser/CMakeLists.txt`
- Create: `libs/markoff-parser/include/markoff-parser/` (directory)
- Create: `libs/markoff-parser/src/` (directory)
- Create: `libs/markoff-parser/tests/CMakeLists.txt`
- Move: parser sources from `libs/markoff/src/` to `libs/markoff-parser/`
- Modify: `CMakeLists.txt` (repo root)

- [ ] **Step 1: Create directory structure**

```bash
mkdir -p libs/markoff-parser/include/markoff-parser
mkdir -p libs/markoff-parser/src
mkdir -p libs/markoff-parser/tests
```

- [ ] **Step 2: Move parser source files**

```bash
# Headers → public include dir
cp libs/markoff/include/markoff/Document.h libs/markoff-parser/include/markoff-parser/Document.h
cp libs/markoff/src/TreeSitterParser.h libs/markoff-parser/include/markoff-parser/TreeSitterParser.h
cp libs/markoff/src/SourceSpan.h libs/markoff-parser/include/markoff-parser/SourceSpan.h
cp libs/markoff/src/MarkdownSplitter.h libs/markoff-parser/include/markoff-parser/MarkdownSplitter.h
cp libs/markoff/src/TableHandler.h libs/markoff-parser/include/markoff-parser/TableHandler.h

# Implementation files
cp libs/markoff/src/TreeSitterParser.cpp libs/markoff-parser/src/TreeSitterParser.cpp
cp libs/markoff/src/SourceSpan.cpp libs/markoff-parser/src/SourceSpan.cpp
cp libs/markoff/src/MarkdownSplitter.cpp libs/markoff-parser/src/MarkdownSplitter.cpp
cp libs/markoff/src/TableHandler.cpp libs/markoff-parser/src/TableHandler.cpp
cp libs/markoff/src/Document.cpp libs/markoff-parser/src/Document.cpp

# Vendored grammar
cp -r libs/markoff/src/vendor/tree-sitter-markdown libs/markoff-parser/src/vendor/tree-sitter-markdown

# Tests
cp libs/markoff/tests/tst_document.cpp libs/markoff-parser/tests/tst_document.cpp
cp libs/markoff/tests/tst_document_queries.cpp libs/markoff-parser/tests/tst_document_queries.cpp
cp libs/markoff/tests/tst_splitter.cpp libs/markoff-parser/tests/tst_splitter.cpp
cp libs/markoff/tests/tst_table.cpp libs/markoff-parser/tests/tst_table.cpp
```

Note: We copy first, then delete originals in a later task after the new library builds. This prevents breaking the build mid-refactor.

- [ ] **Step 3: Write MarkoffParser CMakeLists.txt**

Create `libs/markoff-parser/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.19)
project(markoff-parser VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TREESITTER REQUIRED IMPORTED_TARGET tree-sitter)

# Tree-sitter markdown grammar (vendored)
set(TS_MD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/tree-sitter-markdown)
add_library(ts-markdown-parser STATIC
    ${TS_MD_DIR}/tree-sitter-markdown/src/parser.c
    ${TS_MD_DIR}/tree-sitter-markdown/src/scanner.c
    ${TS_MD_DIR}/tree-sitter-markdown-inline/src/parser.c
    ${TS_MD_DIR}/tree-sitter-markdown-inline/src/scanner.c
)
target_include_directories(ts-markdown-parser PRIVATE
    ${TS_MD_DIR}/tree-sitter-markdown/src
    ${TS_MD_DIR}/tree-sitter-markdown-inline/src
)
target_compile_definitions(ts-markdown-parser PRIVATE
    EXTENSION_WIKI_LINK
    EXTENSION_TAGS
)
set_target_properties(ts-markdown-parser PROPERTIES POSITION_INDEPENDENT_CODE ON)

# MarkoffParser library
add_library(markoff-parser STATIC
    include/markoff-parser/Document.h
    include/markoff-parser/TreeSitterParser.h
    include/markoff-parser/SourceSpan.h
    include/markoff-parser/MarkdownSplitter.h
    include/markoff-parser/TableHandler.h
    src/Document.cpp
    src/TreeSitterParser.cpp
    src/SourceSpan.cpp
    src/MarkdownSplitter.cpp
    src/TableHandler.cpp
)
set_target_properties(markoff-parser PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(MarkoffParser::MarkoffParser ALIAS markoff-parser)

target_include_directories(markoff-parser
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${TS_MD_DIR}/tree-sitter-markdown/bindings/c
            ${TS_MD_DIR}/tree-sitter-markdown-inline/bindings/c
)

target_link_libraries(markoff-parser
    PUBLIC Qt6::Core
    PRIVATE PkgConfig::TREESITTER ts-markdown-parser
)

enable_testing()
add_subdirectory(tests)
```

- [ ] **Step 4: Write MarkoffParser tests CMakeLists.txt**

Create `libs/markoff-parser/tests/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.19)
project(markoff_parser_tests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Gui Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_markoffparser_document tst_document.cpp)
add_test(NAME tst_markoffparser_document COMMAND tst_markoffparser_document)
target_link_libraries(tst_markoffparser_document PRIVATE Qt6::Test markoff-parser)
set_tests_properties(tst_markoffparser_document PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_markoffparser_document_queries tst_document_queries.cpp)
add_test(NAME tst_markoffparser_document_queries COMMAND tst_markoffparser_document_queries)
target_link_libraries(tst_markoffparser_document_queries PRIVATE Qt6::Test markoff-parser)
set_tests_properties(tst_markoffparser_document_queries PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_markoffparser_splitter tst_splitter.cpp)
add_test(NAME tst_markoffparser_splitter COMMAND tst_markoffparser_splitter)
target_link_libraries(tst_markoffparser_splitter PRIVATE Qt6::Test markoff-parser)
set_tests_properties(tst_markoffparser_splitter PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_markoffparser_table tst_table.cpp)
add_test(NAME tst_markoffparser_table COMMAND tst_markoffparser_table)
target_link_libraries(tst_markoffparser_table PRIVATE Qt6::Test Qt6::Gui Qt6::Widgets markoff-parser)
set_tests_properties(tst_markoffparser_table PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 5: Update include paths in moved files**

In every moved source file, update includes to use the new public header paths:

`libs/markoff-parser/src/TreeSitterParser.cpp`:
- `#include "TreeSitterParser.h"` → `#include <markoff-parser/TreeSitterParser.h>`
- `#include "SourceSpan.h"` → `#include <markoff-parser/SourceSpan.h>`

`libs/markoff-parser/src/SourceSpan.cpp`:
- `#include "SourceSpan.h"` → `#include <markoff-parser/SourceSpan.h>`
- Delete `#include "DocumentBuilder_p.h"` entirely

`libs/markoff-parser/src/MarkdownSplitter.cpp`:
- `#include "MarkdownSplitter.h"` → `#include <markoff-parser/MarkdownSplitter.h>`
- `#include "TreeSitterParser.h"` → `#include <markoff-parser/TreeSitterParser.h>`

`libs/markoff-parser/src/TableHandler.cpp`:
- `#include "TableHandler.h"` → `#include <markoff-parser/TableHandler.h>`

`libs/markoff-parser/src/Document.cpp`:
- `#include "markoff/Document.h"` → `#include <markoff-parser/Document.h>`
- Delete `#include "DocumentBuilder_p.h"` (will be replaced in Task 2)

Test files — update Document.h include:
- `#include "markoff/Document.h"` → `#include <markoff-parser/Document.h>`

Splitter test — update include if it references private headers.
Table test — update TableHandler include.

- [ ] **Step 6: Delete MD4C-dependent code from SourceSpan.cpp**

The moved `libs/markoff-parser/src/SourceSpan.cpp` currently contains the old `buildSpanMap(const QList<Block>&, const QByteArray&)` function that walks the MD4C Block tree. Delete everything in this file except `buildUtf8ToCharMap()`:

Keep only:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <markoff-parser/SourceSpan.h>

namespace Markoff {

QList<int> buildUtf8ToCharMap(const QByteArray &utf8)
{
    // [existing implementation unchanged]
}

} // namespace Markoff
```

Delete: `makeDelimiterSpan()`, `makeContentSpan()`, `buildSpansForBlock()`, and the `buildSpanMap(const QList<Block>&, ...)` function. Also remove the `struct Block;` forward declaration from `SourceSpan.h` and the `buildSpanMap()` free function declaration.

- [ ] **Step 7: Add `add_subdirectory(libs/markoff-parser)` to root CMakeLists.txt**

In the repo root `CMakeLists.txt`, add `add_subdirectory(libs/markoff-parser)` **before** the line that adds `libs/markoff`. The parser library must be available when the editor library's CMake runs.

- [ ] **Step 8: Build the parser library and run its tests**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON 2>&1 | tail -20
cmake --build build --target markoff-parser 2>&1 | tail -20
```

Note: The Document.cpp in markoff-parser still references DocumentBuilder so it won't compile yet. That's expected — Task 2 rewrites it. For now verify that TreeSitterParser, SourceSpan, MarkdownSplitter, and TableHandler compile.

If compilation fails, fix include path issues before proceeding.

- [ ] **Step 9: Commit**

```bash
git add libs/markoff-parser/
git add CMakeLists.txt  # only if root CMakeLists was modified
git commit -m "feat(markoff-parser): create parser library skeleton with moved files

Move TreeSitterParser, SourceSpan, MarkdownSplitter, TableHandler, and
Document to new libs/markoff-parser/ library. Document.cpp still references
MD4C (rewritten in next task). Old files in libs/markoff/ not yet deleted."
```

---

### Task 2: Implement buildDocumentQueries() on TreeSitterParser

Add a tree-sitter CST traversal that extracts headings, links, and tags as structured data. This is the core of the parser migration.

**Files:**
- Modify: `libs/markoff-parser/include/markoff-parser/TreeSitterParser.h`
- Modify: `libs/markoff-parser/src/TreeSitterParser.cpp`
- Modify: `libs/markoff-parser/include/markoff-parser/Document.h` (add DocumentQueryResult forward decl or import)

- [ ] **Step 1: Add DocumentQueryResult struct and method declaration to TreeSitterParser.h**

Add to `libs/markoff-parser/include/markoff-parser/TreeSitterParser.h`, inside `namespace Markoff`, before the `TreeSitterParser` class:

```cpp
// Forward declarations from Document.h
struct HeadingInfo;
struct LinkInfo;
struct TagInfo;

/// Result of a document-query traversal of the tree-sitter CST.
struct DocumentQueryResult {
    QList<HeadingInfo> headings;
    QList<LinkInfo> links;
    QList<TagInfo> tags;
};
```

Add inside the `TreeSitterParser` class, in the public section:

```cpp
    /// Walk the CST and extract structured document metadata.
    /// Separate from buildSpanMap() which produces flat formatting ranges.
    DocumentQueryResult buildDocumentQueries() const;
```

- [ ] **Step 2: Implement buildDocumentQueries() in TreeSitterParser.cpp**

Add to `libs/markoff-parser/src/TreeSitterParser.cpp`. This needs to include `<markoff-parser/Document.h>` for the HeadingInfo/LinkInfo/TagInfo types.

Add include at top:
```cpp
#include <markoff-parser/Document.h>
```

Add the implementation. The traversal walks block-level nodes for headings, then walks inline trees for links, tags, and wiki-links:

```cpp
DocumentQueryResult TreeSitterParser::buildDocumentQueries() const
{
    DocumentQueryResult result;
    if (!m_blockTree)
        return result;

    TSNode root = ts_tree_root_node(m_blockTree);

    // Helper: extract text between two byte offsets from m_utf8
    auto textBetween = [this](int startByte, int endByte) -> QString {
        if (startByte < 0 || endByte <= startByte || endByte > m_utf8.size())
            return {};
        return QString::fromUtf8(m_utf8.mid(startByte, endByte - startByte));
    };

    // --- Headings: walk block tree for atx_heading nodes ---
    std::function<void(TSNode)> collectHeadings = [&](TSNode node) {
        const char *type = ts_node_type(node);
        if (strcmp(type, "atx_heading") == 0) {
            HeadingInfo h;
            h.level = 1;
            int contentStart = -1, contentEnd = -1;
            for (uint32_t i = 0; i < ts_node_child_count(node); ++i) {
                TSNode child = ts_node_child(node, i);
                const char *ct = ts_node_type(child);
                if (strncmp(ct, "atx_h", 5) == 0 && strstr(ct, "_marker")) {
                    h.level = ct[5] - '0';
                } else if (strcmp(ct, "inline") == 0) {
                    contentStart = ts_node_start_byte(child);
                    contentEnd = ts_node_end_byte(child);
                }
            }
            if (contentStart >= 0)
                h.text = textBetween(contentStart, contentEnd).trimmed();
            h.sourceOffset = ts_node_start_byte(node);
            result.headings.append(h);
            return; // don't recurse into heading children
        }
        for (uint32_t i = 0; i < ts_node_child_count(node); ++i)
            collectHeadings(ts_node_child(node, i));
    };
    collectHeadings(root);

    // --- Links and tags: walk inline trees ---
    for (const auto *inlineTree : m_inlineTrees) {
        if (!inlineTree) continue;
        TSNode inlineRoot = ts_tree_root_node(inlineTree);

        std::function<void(TSNode)> collectInline = [&](TSNode node) {
            const char *type = ts_node_type(node);

            if (strcmp(type, "wiki_link") == 0) {
                LinkInfo li;
                int start = ts_node_start_byte(node);
                int end = ts_node_end_byte(node);
                QString raw = textBetween(start, end);
                // Wiki links: [[target]] or [[target|display]]
                // Strip [[ and ]]
                if (raw.startsWith(QStringLiteral("[[")))
                    raw = raw.mid(2);
                if (raw.endsWith(QStringLiteral("]]")))
                    raw.chop(2);
                int pipe = raw.indexOf(QLatin1Char('|'));
                if (pipe >= 0) {
                    li.target = raw.left(pipe);
                    li.displayText = raw.mid(pipe + 1);
                } else {
                    li.target = raw;
                    li.displayText = raw;
                }
                // Check for embed prefix (! before [[)
                if (start > 0 && m_utf8[start - 1] == '!')
                    li.type = LinkInfo::Embed;
                else
                    li.type = LinkInfo::Wiki;
                li.sourceOffset = start;
                result.links.append(li);
                return;
            }

            if (strcmp(type, "inline_link") == 0 || strcmp(type, "shortcut_link") == 0 ||
                strcmp(type, "full_reference_link") == 0 || strcmp(type, "collapsed_reference_link") == 0) {
                LinkInfo li;
                li.type = LinkInfo::Standard;
                li.sourceOffset = ts_node_start_byte(node);
                // Extract link text and destination from children
                for (uint32_t i = 0; i < ts_node_child_count(node); ++i) {
                    TSNode child = ts_node_child(node, i);
                    const char *ct = ts_node_type(child);
                    if (strcmp(ct, "link_text") == 0) {
                        li.displayText = textBetween(ts_node_start_byte(child),
                                                      ts_node_end_byte(child));
                        // Strip [ and ] from link_text
                        if (li.displayText.startsWith(QLatin1Char('[')))
                            li.displayText = li.displayText.mid(1);
                        if (li.displayText.endsWith(QLatin1Char(']')))
                            li.displayText.chop(1);
                    } else if (strcmp(ct, "link_destination") == 0) {
                        li.target = textBetween(ts_node_start_byte(child),
                                                 ts_node_end_byte(child));
                    }
                }
                result.links.append(li);
                return;
            }

            if (strcmp(type, "image") == 0) {
                LinkInfo li;
                li.type = LinkInfo::Image;
                li.sourceOffset = ts_node_start_byte(node);
                for (uint32_t i = 0; i < ts_node_child_count(node); ++i) {
                    TSNode child = ts_node_child(node, i);
                    const char *ct = ts_node_type(child);
                    if (strcmp(ct, "image_description") == 0 || strcmp(ct, "link_text") == 0) {
                        li.displayText = textBetween(ts_node_start_byte(child),
                                                      ts_node_end_byte(child));
                    } else if (strcmp(ct, "link_destination") == 0) {
                        li.target = textBetween(ts_node_start_byte(child),
                                                 ts_node_end_byte(child));
                    }
                }
                result.links.append(li);
                return;
            }

            if (strcmp(type, "tag") == 0) {
                TagInfo ti;
                ti.sourceOffset = ts_node_start_byte(node);
                ti.name = textBetween(ts_node_start_byte(node), ts_node_end_byte(node));
                if (ti.name.startsWith(QLatin1Char('#')))
                    ti.name = ti.name.mid(1);
                result.tags.append(ti);
                return;
            }

            // Recurse into children
            for (uint32_t i = 0; i < ts_node_child_count(node); ++i)
                collectInline(ts_node_child(node, i));
        };
        collectInline(inlineRoot);
    }

    return result;
}
```

- [ ] **Step 3: Build and verify compilation**

```bash
cmake --build build --target markoff-parser 2>&1 | tail -20
```

Fix any compilation errors (missing includes, type mismatches).

- [ ] **Step 4: Commit**

```bash
git add libs/markoff-parser/
git commit -m "feat(markoff-parser): implement buildDocumentQueries() on TreeSitterParser

Walk tree-sitter CST to extract headings, links, wikilinks, images, and
tags as structured DocumentQueryResult data. Separate from buildSpanMap()
which produces flat formatting ranges for the highlighter."
```

---

### Task 3: Rewrite Document.cpp to use TreeSitterParser

Replace MD4C-based parsing with tree-sitter. The public API is unchanged; only the internals change.

**Files:**
- Modify: `libs/markoff-parser/src/Document.cpp`

- [ ] **Step 1: Rewrite Document.cpp**

Replace the entire `libs/markoff-parser/src/Document.cpp` with:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <markoff-parser/Document.h>
#include <markoff-parser/TreeSitterParser.h>

#include <QStringList>
#include <QRegularExpression>

namespace Markoff {

struct Footnote {
    QString label;
    QString content;
    int number = 0;
};

struct Document::Private {
    QString source;
    QString frontmatter;
    TreeSitterParser parser;
    QList<Footnote> footnotes;
};

Document::Document()
    : d(std::make_unique<Private>())
{
}

Document::~Document() = default;

std::unique_ptr<Document> Document::fromMarkdown(const QString &source)
{
    auto doc = std::unique_ptr<Document>(new Document());
    doc->d->source = source;

    // Extract frontmatter before parsing
    QString markdown = source;
    if (source.startsWith(QStringLiteral("---\n")) || source.startsWith(QStringLiteral("---\r\n"))) {
        int endPos = source.indexOf(QStringLiteral("\n---"), 3);
        if (endPos >= 0) {
            int fmStart = source.indexOf(QLatin1Char('\n')) + 1;
            doc->d->frontmatter = source.mid(fmStart, endPos - fmStart);
            int afterFm = endPos + 4;
            if (afterFm < source.size() && source[afterFm] == QLatin1Char('\n'))
                ++afterFm;
            markdown = source.mid(afterFm);
        }
    }

    // Extract footnote definitions [^label]: content
    static const QRegularExpression footnoteDef(
        QStringLiteral(R"(^\[\^([^\]]+)\]:\s*(.+)$)"),
        QRegularExpression::MultilineOption);

    QHash<QString, Footnote> footnoteMap;
    auto it = footnoteDef.globalMatch(markdown);
    while (it.hasNext()) {
        auto match = it.next();
        Footnote fn;
        fn.label = match.captured(1);
        fn.content = match.captured(2);
        footnoteMap.insert(fn.label, fn);
    }

    if (!footnoteMap.isEmpty())
        markdown.remove(footnoteDef);

    // Number footnotes in order of first reference
    int nextNum = 1;
    static const QRegularExpression footnoteRef(QStringLiteral(R"(\[\^([^\]]+)\])"));
    auto refIt = footnoteRef.globalMatch(markdown);
    while (refIt.hasNext()) {
        auto match = refIt.next();
        const QString label = match.captured(1);
        if (footnoteMap.contains(label) && footnoteMap[label].number == 0) {
            footnoteMap[label].number = nextNum++;
        }
    }

    // Replace [^label] references with superscript numbers
    if (!footnoteMap.isEmpty()) {
        QString processed;
        int pos = 0;
        auto refIt2 = footnoteRef.globalMatch(markdown);
        while (refIt2.hasNext()) {
            auto match = refIt2.next();
            processed += markdown.mid(pos, match.capturedStart() - pos);
            const QString label = match.captured(1);
            if (footnoteMap.contains(label)) {
                int num = footnoteMap[label].number;
                processed += QStringLiteral("<sup>%1</sup>").arg(num);
            } else {
                processed += match.captured(0);
            }
            pos = match.capturedEnd();
        }
        processed += markdown.mid(pos);
        markdown = processed;
    }

    // Store sorted footnotes
    QList<Footnote> sorted;
    for (auto &fn : footnoteMap) {
        if (fn.number > 0)
            sorted.append(fn);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const Footnote &a, const Footnote &b) { return a.number < b.number; });
    doc->d->footnotes = sorted;

    // Parse with tree-sitter (replaces MD4C DocumentBuilder)
    doc->d->parser.parse(markdown);

    return doc;
}

QString Document::sourceText() const { return d->source; }
bool Document::isEmpty() const { return d->source.isEmpty(); }

QString Document::frontmatter() const { return d->frontmatter; }

QString Document::markdownContent() const
{
    if (d->frontmatter.isEmpty())
        return d->source;
    int endPos = d->source.indexOf(QStringLiteral("\n---"), 3);
    if (endPos < 0)
        return d->source;
    int afterFm = endPos + 4;
    if (afterFm < d->source.size() && d->source[afterFm] == QLatin1Char('\n'))
        ++afterFm;
    return d->source.mid(afterFm);
}

int Document::footnoteCount() const { return d->footnotes.size(); }

QString Document::footnoteContent(int number) const
{
    for (const auto &fn : d->footnotes) {
        if (fn.number == number)
            return fn.content;
    }
    return {};
}

// --- Query API (now tree-sitter-based) ---

QList<HeadingInfo> Document::headings() const
{
    auto qr = d->parser.buildDocumentQueries();
    return qr.headings;
}

QList<LinkInfo> Document::links() const
{
    auto qr = d->parser.buildDocumentQueries();
    return qr.links;
}

QList<LinkInfo> Document::wikiLinks() const
{
    QList<LinkInfo> all = links();
    QList<LinkInfo> result;
    for (const auto &l : all) {
        if (l.type == LinkInfo::Wiki || l.type == LinkInfo::Embed)
            result.append(l);
    }
    return result;
}

QList<TagInfo> Document::tags() const
{
    auto qr = d->parser.buildDocumentQueries();
    return qr.tags;
}

QList<FootnoteInfo> Document::footnotes() const
{
    QList<FootnoteInfo> result;
    for (const auto &fn : d->footnotes) {
        FootnoteInfo info;
        info.number = fn.number;
        info.label = fn.label;
        info.content = fn.content;
        result.append(info);
    }
    return result;
}

int Document::wordCount() const
{
    const QString content = markdownContent().trimmed();
    if (content.isEmpty())
        return 0;
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    return content.split(whitespace, Qt::SkipEmptyParts).size();
}

int Document::characterCount() const
{
    return markdownContent().length();
}

// --- extractSubpath (unchanged — operates on source text) ---
```

Then append the `extractSubpath()` implementation. This method is parser-independent (line-based text search), so copy it verbatim from the current `libs/markoff/src/Document.cpp` lines 304-412. Read that file and copy the `extractSubpath` method exactly.

Also remove the `DocumentBlockAccessor::blocks()` function that was at the end of the old Document.cpp — it no longer exists.

- [ ] **Step 2: Remove DocumentBlockAccessor from Document.h**

In `libs/markoff-parser/include/markoff-parser/Document.h`, remove:
- The `friend struct DocumentBlockAccessor;` line from the Document class
- The `struct DocumentBlockAccessor` declaration if present

- [ ] **Step 3: Build the parser library**

```bash
cmake --build build --target markoff-parser 2>&1 | tail -20
```

Fix any compilation errors.

- [ ] **Step 4: Run parser tests**

```bash
cd build && ctest -R tst_markoffparser --output-on-failure
```

The `tst_markoffparser_document` and `tst_markoffparser_document_queries` tests are the critical validation. They must pass — same public API, different internal parser.

If tests fail, debug the `buildDocumentQueries()` implementation. Common issues:
- Heading text extraction may include trailing whitespace
- Wiki link target parsing may differ in edge cases
- Tag name may or may not include the `#` prefix

- [ ] **Step 5: Commit**

```bash
git add libs/markoff-parser/
git commit -m "feat(markoff-parser): rewrite Document on tree-sitter, delete MD4C dependency

Document::fromMarkdown() now uses TreeSitterParser internally. Query
methods (headings, links, tags) use buildDocumentQueries() CST traversal.
Footnotes and frontmatter extraction unchanged (regex-based).
DocumentBlockAccessor deleted."
```

---

### Task 4: Update markoff editor to depend on MarkoffParser and delete old files

Wire the editor library to use MarkoffParser, delete the original parser files from markoff, and delete Renderer/ReadingView/DocumentBuilder/RenderSettings.

**Files:**
- Modify: `libs/markoff/CMakeLists.txt`
- Delete: all files listed in the "Deleted Files" section of the file map
- Modify: `libs/markoff/tests/CMakeLists.txt`
- Modify: `libs/markoff/src/SceneCoordinator.cpp` (include paths)
- Modify: `libs/markoff/src/MarkdownHighlighter.h` (include path)
- Modify: `libs/markoff/src/MarkdownTextItem.cpp` (include path)
- Modify: `libs/markoff/src/TableBlockItem.cpp` (include path)

- [ ] **Step 1: Delete old files from markoff**

```bash
# Parser files (now in markoff-parser)
rm libs/markoff/src/TreeSitterParser.h libs/markoff/src/TreeSitterParser.cpp
rm libs/markoff/src/SourceSpan.h libs/markoff/src/SourceSpan.cpp
rm libs/markoff/src/MarkdownSplitter.h libs/markoff/src/MarkdownSplitter.cpp
rm libs/markoff/src/TableHandler.h libs/markoff/src/TableHandler.cpp
rm -rf libs/markoff/src/vendor/tree-sitter-markdown

# MD4C / Renderer / ReadingView
rm libs/markoff/src/DocumentBuilder.cpp libs/markoff/src/DocumentBuilder_p.h
rm libs/markoff/src/Renderer.h libs/markoff/src/Renderer.cpp
rm libs/markoff/src/ReadingView.cpp
rm libs/markoff/include/markoff/ReadingView.h
rm libs/markoff/include/markoff/RenderSettings.h
rm libs/markoff/include/markoff/Document.h

# Tests that moved or are deleted
rm libs/markoff/tests/tst_renderer.cpp
rm libs/markoff/tests/tst_document.cpp
rm libs/markoff/tests/tst_document_queries.cpp
rm libs/markoff/tests/tst_splitter.cpp
rm libs/markoff/tests/tst_table.cpp
```

- [ ] **Step 2: Rewrite markoff CMakeLists.txt**

Replace `libs/markoff/CMakeLists.txt` with:

```cmake
cmake_minimum_required(VERSION 3.19)
project(markoff VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6SyntaxHighlighting REQUIRED)

if(NOT TARGET jkqtmathtext)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../jkqtmathtext ${CMAKE_CURRENT_BINARY_DIR}/jkqtmathtext)
endif()

add_library(markoff STATIC
    include/markoff/Editor.h
    src/SelectableItem.h
    src/BlockItem.h
    src/BlockItem.cpp
    src/StubBlockItem.h
    src/StubBlockItem.cpp
    src/TableBlockItem.h
    src/TableBlockItem.cpp
    src/MarkdownTextItem.h
    src/MarkdownTextItem.cpp
    src/MathRenderer.h
    src/MathRenderer.cpp
    src/MathTextObject.h
    src/MathTextObject.cpp
    src/SelectionManager.h
    src/SelectionManager.cpp
    src/SelectionScene.h
    src/SelectionScene.cpp
    src/SceneCoordinator.h
    src/SceneCoordinator.cpp
    src/Theme.cpp
    src/Editor.cpp
    src/MarkdownHighlighter.cpp
    src/DecoratedRange.cpp
    src/ResourceProvider.cpp
    src/TextControl.cpp
)
set_target_properties(markoff PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Markoff::Markoff ALIAS markoff)

target_include_directories(markoff
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(markoff
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        MarkoffParser::MarkoffParser
    PRIVATE
        KF6::SyntaxHighlighting
        jkqtmathtext
)

enable_testing()
add_subdirectory(tests)
add_subdirectory(app)
```

- [ ] **Step 3: Update markoff tests CMakeLists.txt**

Replace `libs/markoff/tests/CMakeLists.txt` — remove deleted/moved test targets:

```cmake
cmake_minimum_required(VERSION 3.19)
project(markoff_tests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_markoff_selection tst_selection.cpp)
add_test(NAME tst_markoff_selection COMMAND tst_markoff_selection)
target_link_libraries(tst_markoff_selection PRIVATE Qt6::Test markoff)
target_include_directories(tst_markoff_selection PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set_tests_properties(tst_markoff_selection PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_markoff_theme tst_theme.cpp)
add_test(NAME tst_markoff_theme COMMAND tst_markoff_theme)
target_link_libraries(tst_markoff_theme PRIVATE Qt6::Test markoff)
set_tests_properties(tst_markoff_theme PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_markoff_resourceprovider tst_resourceprovider.cpp)
add_test(NAME tst_markoff_resourceprovider COMMAND tst_markoff_resourceprovider)
target_link_libraries(tst_markoff_resourceprovider PRIVATE Qt6::Test markoff)
set_tests_properties(tst_markoff_resourceprovider PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_markoff_inline_math tst_inline_math.cpp)
add_test(NAME tst_markoff_inline_math COMMAND tst_markoff_inline_math)
target_link_libraries(tst_markoff_inline_math PRIVATE Qt6::Test Qt6::Widgets markoff KF6::SyntaxHighlighting)
target_include_directories(tst_markoff_inline_math PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set_tests_properties(tst_markoff_inline_math PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_markoff_editor_formatting tst_editor_formatting.cpp)
add_test(NAME tst_markoff_editor_formatting COMMAND tst_markoff_editor_formatting)
target_link_libraries(tst_markoff_editor_formatting PRIVATE Qt6::Test Qt6::Widgets markoff)
set_tests_properties(tst_markoff_editor_formatting PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 4: Update include paths in remaining markoff sources**

`libs/markoff/src/SceneCoordinator.cpp`:
- `#include "MarkdownSplitter.h"` → `#include <markoff-parser/MarkdownSplitter.h>`
- `#include "TreeSitterParser.h"` → `#include <markoff-parser/TreeSitterParser.h>`

`libs/markoff/src/SceneCoordinator.h`:
- `#include "MarkdownHighlighter.h"` — keep (local to markoff)
- If it forward-declares `TreeSitterParser`, update to include or keep forward decl

`libs/markoff/src/MarkdownHighlighter.h`:
- `#include "SourceSpan.h"` → `#include <markoff-parser/SourceSpan.h>`

`libs/markoff/src/MarkdownTextItem.cpp`:
- `#include "SourceSpan.h"` → `#include <markoff-parser/SourceSpan.h>`

`libs/markoff/src/TableBlockItem.cpp`:
- `#include "TableHandler.h"` → `#include <markoff-parser/TableHandler.h>`

`libs/markoff/src/Editor.cpp`:
- If it includes `markoff/Document.h`, change to `<markoff-parser/Document.h>`
- If it includes `markoff/RenderSettings.h`, delete that include

- [ ] **Step 5: Build everything**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -40
```

Fix compilation errors. The most likely issues are:
- Missing includes for moved headers
- References to deleted types (Block, InlineRun, DocumentBlockAccessor)
- MD4C type constants (MD_BLOCK_H, etc.) in files that should no longer use them

- [ ] **Step 6: Run all tests**

```bash
cd build && ctest --output-on-failure 2>&1 | tail -40
```

Both markoff-parser and markoff test suites must pass.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "refactor(markoff): wire editor to MarkoffParser, delete MD4C/Renderer/ReadingView

Editor library now depends on MarkoffParser::MarkoffParser. Deleted:
DocumentBuilder, Renderer, ReadingView, RenderSettings, MD4C dependency.
Updated all include paths to use markoff-parser/ public headers."
```

---

### Task 5: Remove Mode enum from Editor, MarkdownHighlighter, and SceneCoordinator

Simplify the editor to always operate in live preview mode.

**Files:**
- Modify: `libs/markoff/include/markoff/Editor.h`
- Modify: `libs/markoff/src/Editor.cpp`
- Modify: `libs/markoff/src/MarkdownHighlighter.h`
- Modify: `libs/markoff/src/MarkdownHighlighter.cpp`
- Modify: `libs/markoff/src/SceneCoordinator.h`
- Modify: `libs/markoff/src/SceneCoordinator.cpp`

- [ ] **Step 1: Remove Mode from Editor.h**

In `libs/markoff/include/markoff/Editor.h`:
- Delete `enum class Mode { Source, LivePreview };` and `Q_ENUM(Mode)`
- Delete `Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)`
- Delete `void setMode(Mode mode);` and `Mode mode() const;`
- Delete `void setRenderSettings(const RenderSettings &settings);` and `RenderSettings renderSettings() const;`
- Delete signal `void modeChanged(Markoff::Editor::Mode mode);`
- Delete members `Mode m_mode = Mode::Source;` and `RenderSettings m_renderSettings;`
- Delete `#include <markoff/RenderSettings.h>` if present
- Add to public section:

```cpp
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;
```

- Add member: `bool m_readOnly = false;`

- [ ] **Step 2: Simplify Editor.cpp**

In `libs/markoff/src/Editor.cpp`:

Replace `toPlainText()`:
```cpp
QString Editor::toPlainText() const
{
    return m_coordinator->toMarkdown();
}
```

Delete the `setMode()` method entirely.

In `rebuildScene()` (or wherever mode switching happens), ensure it always calls `m_coordinator->loadMarkdown(m_sourceText)` — no conditional on mode.

Add `setReadOnly()` / `isReadOnly()`:
```cpp
void Editor::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    for (auto *item : m_coordinator->items()) {
        if (item->isTextItem()) {
            auto *textItem = static_cast<MarkdownTextItem *>(item->asGraphicsItem());
            textItem->textControl()->setTextInteractionFlags(
                readOnly ? Qt::TextBrowserInteraction : Qt::TextEditorInteraction);
        }
    }
}

bool Editor::isReadOnly() const
{
    return m_readOnly;
}
```

Also guard keyboard input in `keyPressEvent()` — if read-only, only allow navigation keys, not editing keys. The simplest approach: if `m_readOnly`, skip the base class call for non-navigation events. But `Qt::TextBrowserInteraction` on the text items already handles this — just verify.

- [ ] **Step 3: Remove Mode from MarkdownHighlighter**

In `libs/markoff/src/MarkdownHighlighter.h`:
- Delete `enum class Mode { Source, LivePreview };`
- Delete `void setMode(Mode mode);`
- Delete `Mode mode() const { return m_mode; }`
- Delete member `Mode m_mode = Mode::Source;`

In `libs/markoff/src/MarkdownHighlighter.cpp`:
- Delete the `setMode()` method
- In `highlightBlock()` and `applySpanFormat()`, remove any `if (m_mode == Mode::Source)` branches. Always run the live-preview path (hide delimiters when cursor is not adjacent).

- [ ] **Step 4: Remove loadSource() from SceneCoordinator**

In `libs/markoff/src/SceneCoordinator.h`:
- Delete `void loadSource(const QString &markdown);`
- In `createTextItem()`, remove the `MarkdownHighlighter::Mode` parameter — always create in live-preview mode

In `libs/markoff/src/SceneCoordinator.cpp`:
- Delete the `loadSource()` method body
- In `createTextItem()`, remove the `hlMode` parameter. Delete `highlighter->setMode(hlMode)`. Always call `item->refreshMathSubstitution()` (no mode check).
- In `loadMarkdown()`, call `createTextItem(seg.text)` without the mode argument.

- [ ] **Step 5: Build and test**

```bash
cmake --build build 2>&1 | tail -30
cd build && ctest -R tst_markoff --output-on-failure
```

Fix any compilation errors from removed types.

- [ ] **Step 6: Commit**

```bash
git add libs/markoff/
git commit -m "refactor(markoff): remove Mode enum, always live preview

Editor no longer has Source/LivePreview modes. SceneCoordinator::loadSource()
deleted. MarkdownHighlighter always hides delimiters away from cursor.
Added Editor::setReadOnly()/isReadOnly() for read-only display."
```

---

### Task 6: Add TableBlockItem read-only TODO and update docs

**Files:**
- Modify: `libs/markoff/src/TableBlockItem.h`

- [ ] **Step 1: Add TODO comment to TableBlockItem.h**

In `libs/markoff/src/TableBlockItem.h`, add before the class declaration:

```cpp
// TODO: When Editor::setReadOnly(true) is active, TableBlockItem should still
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

- [ ] **Step 2: Commit**

```bash
git add libs/markoff/src/TableBlockItem.h
git commit -m "docs(markoff): add read-only column-width TODO to TableBlockItem"
```

---

### Task 7: Stub MarkoffRenderEngine and simplify NoteEditorWidget

Update Corbomite host code to work with the simplified markoff API.

**Files:**
- Modify: `libs/core/src/MarkoffRenderEngine.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Modify: `src/editor/NoteEditorWidget.h`
- Modify: `src/editor/NoteEditorWidget.cpp`

- [ ] **Step 1: Stub MarkoffRenderEngine**

Replace `libs/core/src/MarkoffRenderEngine.cpp` with:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/MarkoffRenderEngine.h"
#include "corbomite/core/RenderedDocument.h"

#include <QTextDocument>

namespace Corbomite {

// DEPRECATED: Stub implementation. Returns raw markdown as a plain
// QTextDocument. The Markoff rendering pipeline has been removed.
// Canvas card rendering needs a new approach (e.g., offscreen Editor
// widget or a dedicated card renderer).
std::unique_ptr<RenderedDocument> MarkoffRenderEngine::render(
    const QString &markdown,
    const RenderOptions &options) const
{
    QString md = markdown;
    if (!options.subpath.isEmpty())
        md = extractSubpath(markdown, options.subpath);

    auto doc = std::make_unique<QTextDocument>();
    doc->setPlainText(md);
    return RenderedDocument::fromQTextDocument(std::move(doc));
}

} // namespace Corbomite
```

- [ ] **Step 2: Update libs/core CMakeLists.txt**

Remove the private include path into markoff's src/ directory (the line like `PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../markoff/src`). Remove any link dependency on markoff if MarkoffRenderEngine was the only consumer. If other code in libs/core still needs markoff, keep the link.

Remove `#include <markoff/RenderSettings.h>` references if any remain. Remove `#include "Renderer.h"`.

- [ ] **Step 3: Simplify NoteEditorWidget.h**

Replace `libs/markoff/src/editor/NoteEditorWidget.h` — but the actual path is `src/editor/NoteEditorWidget.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace Markoff {
class Editor;
}

namespace Corbomite {

class NoteDocument;
class VaultModel;
class VaultResourceProvider;
class CompletionPopup;

class NoteEditorWidget : public QWidget {
    Q_OBJECT

public:
    enum class ViewMode { Editing, Reading };
    Q_ENUM(ViewMode)

    explicit NoteEditorWidget(QWidget *parent = nullptr);

    void setNoteDocument(NoteDocument *doc);
    NoteDocument *noteDocument() const;
    void setVaultModel(VaultModel *vault);

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;

    Markoff::Editor *editor() const;

    int currentLine() const;
    int currentColumn() const;

Q_SIGNALS:
    void cursorInfoChanged(int line, int column, int wordCount);
    void linkActivated(const QString &targetPath);
    void viewModeChanged(ViewMode mode);

private:
    bool eventFilter(QObject *obj, QEvent *event) override;

    void onTextChanged();
    void onCursorPositionChanged(int line, int column);
    void syncFromDocument();

    // Completion
    void triggerWikiLinkCompletion();
    void triggerTagCompletion();
    void dismissCompletion();
    void onCompletionAccepted(const QString &text, const QString &data);

    // Link resolution
    QString resolveTarget(const QString &target) const;

    Markoff::Editor *m_editor = nullptr;
    ViewMode m_viewMode = ViewMode::Editing;

    NoteDocument *m_doc = nullptr;
    VaultModel *m_vault = nullptr;
    VaultResourceProvider *m_resourceProvider = nullptr;
    bool m_updatingFromDoc = false;
    int m_cachedWordCount = 0;

    // Completion state
    CompletionPopup *m_completionPopup = nullptr;
    int m_completionTriggerPos = -1;
    enum class CompletionMode { None, WikiLink, Tag };
    CompletionMode m_completionMode = CompletionMode::None;
};

} // namespace Corbomite
```

Key changes: removed `QStackedWidget` include, removed `ReadingView` forward decl, collapsed `ViewMode` to `{ Editing, Reading }`, removed `m_readingView` and `m_modeStack` members.

- [ ] **Step 4: Simplify NoteEditorWidget.cpp**

In `src/editor/NoteEditorWidget.cpp`:

- Remove `#include <markoff/ReadingView.h>`
- Remove `#include <QStackedWidget>`

Rewrite constructor:
```cpp
NoteEditorWidget::NoteEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_editor(new Markoff::Editor(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_editor);

    connect(m_editor, &Markoff::Editor::textChanged,
            this, &NoteEditorWidget::onTextChanged);
    connect(m_editor, &Markoff::Editor::cursorPositionChanged,
            this, &NoteEditorWidget::onCursorPositionChanged);
    connect(m_editor, &Markoff::Editor::wordCountChanged,
            this, [this](int count) { m_cachedWordCount = count; });
    connect(m_editor, &Markoff::Editor::linkClicked,
            this, [this](const QString &target) {
        Q_EMIT linkActivated(resolveTarget(target));
    });
    connect(m_editor, &Markoff::Editor::wikiLinkTrigger,
            this, [this](int pos) {
        m_completionTriggerPos = pos;
        triggerWikiLinkCompletion();
    });
    connect(m_editor, &Markoff::Editor::tagTrigger,
            this, [this](int pos) {
        m_completionTriggerPos = pos;
        triggerTagCompletion();
    });
    connect(m_editor, &Markoff::Editor::completionDismissHint,
            this, &NoteEditorWidget::dismissCompletion);

    m_editor->installEventFilter(this);
}
```

Rewrite `setNoteDocument()` — remove all ReadingView references:
```cpp
void NoteEditorWidget::setNoteDocument(NoteDocument *doc)
{
    m_doc = doc;
    if (m_doc) {
        m_editor->setResourceProvider(nullptr);
        delete m_resourceProvider;
        m_resourceProvider = nullptr;
        if (m_vault) {
            m_resourceProvider = new VaultResourceProvider(m_vault, m_doc->relativePath());
            m_editor->setResourceProvider(m_resourceProvider);
        }
        syncFromDocument();
    } else {
        m_editor->clear();
    }
}
```

Rewrite `setVaultModel()`:
```cpp
void NoteEditorWidget::setVaultModel(VaultModel *vault)
{
    m_vault = vault;
    if (m_doc && m_vault) {
        m_editor->setResourceProvider(nullptr);
        delete m_resourceProvider;
        m_resourceProvider = new VaultResourceProvider(m_vault, m_doc->relativePath());
        m_editor->setResourceProvider(m_resourceProvider);
    }
}
```

Rewrite `setViewMode()`:
```cpp
void NoteEditorWidget::setViewMode(ViewMode mode)
{
    if (m_viewMode == mode) return;
    m_viewMode = mode;
    m_editor->setReadOnly(mode == ViewMode::Reading);
    Q_EMIT viewModeChanged(mode);
}
```

- [ ] **Step 5: Update any code that references the old ViewMode values**

Search for `ViewMode::Source`, `ViewMode::LivePreview` in the Corbomite codebase and replace:
- `ViewMode::Source` → `ViewMode::Editing`
- `ViewMode::LivePreview` → `ViewMode::Editing`
- `ViewMode::Reading` → `ViewMode::Reading` (unchanged)

Check `src/app/MainWindow.cpp`, `src/editor/EditorViewManager.cpp`, and any other files that reference these enum values.

- [ ] **Step 6: Build entire project**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -40
```

- [ ] **Step 7: Run all tests**

```bash
cd build && ctest --output-on-failure 2>&1 | tail -40
```

All markoff-parser tests and markoff editor tests must pass.

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "refactor: stub MarkoffRenderEngine, simplify NoteEditorWidget

MarkoffRenderEngine now returns raw markdown (deprecated stub). 
NoteEditorWidget removes ReadingView and QStackedWidget; ViewMode
collapsed to Editing/Reading. Reading mode uses Editor::setReadOnly()."
```

---

### Task 8: Final cleanup and documentation update

**Files:**
- Modify: `libs/markoff/docs/architecture.md`
- Modify: `libs/markoff/docs/TODO.md`

- [ ] **Step 1: Update architecture.md**

In `libs/markoff/docs/architecture.md`:
- Update the system diagram to show the two-library split
- Remove references to ReadingView, Renderer, RenderSettings, Mode enum
- Update the "Dependencies" section
- Update the "File Map" section
- Remove "Parsing: Dual Parser Situation" section — there's only one parser now
- Update "What's Not Built Yet" — remove "Remove MD4C" (done)

- [ ] **Step 2: Update TODO.md**

In `libs/markoff/docs/TODO.md`:
- Remove the "Parser / Grammar" section items about removing MD4C and migrating Renderer (done)
- Update any references to Source mode or ReadingView

- [ ] **Step 3: Run full test suite one final time**

```bash
cd build && ctest --output-on-failure
```

All tests pass.

- [ ] **Step 4: Commit**

```bash
git add libs/markoff/docs/
git commit -m "docs(markoff): update architecture and TODO for parser split

Remove references to MD4C, ReadingView, Renderer, Mode enum.
Document the two-library structure (MarkoffParser + MarkoffEditor)."
```
