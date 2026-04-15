# Code-Block Folding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans`. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add fenced-code-block folding to the markoff editor, sharing the heading-folding infrastructure via a unified `FoldableRegion` data model.

**Architecture:** Extend `TreeSitterParser` to emit `CodeBlockInfo` alongside headings. Refactor `FoldingModel` to store a type-tagged `QList<FoldableRegion>` instead of a heading-only list — v1 `headings()` becomes a filter view for compatibility. `TableBlockItem` (which renders fenced code) gets a folded paint mode showing `` ```lang (N lines)``. `FoldGutter` iterates regions (not headings), painting arrows for both types; code-block Ctrl+Click toggles siblings in the same enclosing heading section.

**Tech Stack:** Qt6 (Core, Gui, Widgets, Test) ≥ 6.8; C++20; tree-sitter-markdown via `MarkoffParser::MarkoffParser`; `QT_QPA_PLATFORM=offscreen` for tests.

**Spec:** [`../specs/2026-04-15-code-block-folding-design.md`](../specs/2026-04-15-code-block-folding-design.md). Read before starting.

**v1 reference:** [`../specs/2026-04-15-heading-folding-design.md`](../specs/2026-04-15-heading-folding-design.md) and [`2026-04-15-heading-folding.md`](./2026-04-15-heading-folding.md).

---

## File structure

### New files

- `libs/markoff/tests/tst_code_block_folding.cpp` — region detection, path encoding, bulk ops, reconcile.
- `libs/markoff/tests/tst_code_block_paint.cpp` — TableBlockItem folded paint mode.

### Modified files

- `libs/markoff-parser/include/markoff-parser/Document.h` — add `CodeBlockInfo` struct.
- `libs/markoff-parser/include/markoff-parser/TreeSitterParser.h` — extend `DocumentQueryResult`.
- `libs/markoff-parser/src/TreeSitterParser.cpp` — walk fenced-code nodes.
- `libs/markoff/src/FoldingModel.h` / `.cpp` — unified `FoldableRegion` model.
- `libs/markoff/include/markoff/FoldingTypes.h` / `src/FoldingTypes.cpp` — `FoldableRegion` struct + path helper.
- `libs/markoff/src/SceneCoordinator.h` / `.cpp` — region detection, rename `headingSceneY`→`regionSceneY` etc., applyFoldVisibility handles CodeBlock.
- `libs/markoff/src/TableBlockItem.h` / `.cpp` — folded mode.
- `libs/markoff/src/FoldGutter.cpp` — iterate regions.
- `libs/markoff/src/FoldArrowColumn.cpp` — code-block Ctrl+Click.
- `libs/markoff/include/markoff/Editor.h` / `src/Editor.cpp` — new API, findText auto-unfold for code blocks.
- `libs/markoff/tests/CMakeLists.txt` — register new test binaries.

---

## Task 1: TreeSitterParser exposes `CodeBlockInfo`

**Files:**
- Modify: `libs/markoff-parser/include/markoff-parser/Document.h`
- Modify: `libs/markoff-parser/include/markoff-parser/TreeSitterParser.h`
- Modify: `libs/markoff-parser/src/TreeSitterParser.cpp`
- Test: `libs/markoff-parser/tests/tst_tree_sitter_parser.cpp` (or similar — check existing test file naming for this lib)

- [ ] **Step 1: Define `CodeBlockInfo` struct**

In `libs/markoff-parser/include/markoff-parser/Document.h`, after the existing `HeadingInfo`, `LinkInfo`, `TagInfo` structs, add:

```cpp
struct CodeBlockInfo {
    int sourceOffset;    ///< UTF-8 byte offset of opening fence in parsed source.
    QString language;    ///< Info-string language tag ("" if none). Trimmed, first word only.
    int lineCount;       ///< Lines strictly inside the fences (excludes opening + closing).
};
```

- [ ] **Step 2: Add field to `DocumentQueryResult`**

In `libs/markoff-parser/include/markoff-parser/TreeSitterParser.h`, modify:

```cpp
struct DocumentQueryResult {
    QList<HeadingInfo> headings;
    QList<LinkInfo> links;
    QList<TagInfo> tags;
    QList<CodeBlockInfo> codeBlocks;
};
```

- [ ] **Step 3: Write the failing test**

Find the parser's existing test file (likely `libs/markoff-parser/tests/tst_tree_sitter_parser.cpp`). Append a new `QTest` slot:

```cpp
void TstTreeSitterParser::documentQueries_fencedCodeBlock_populatedWithLanguageAndLineCount()
{
    TreeSitterParser parser;
    const QString src = QStringLiteral(
        "# A\n\n"
        "```cpp\n"
        "int main() { return 0; }\n"
        "puts(\"hi\");\n"
        "```\n");
    QVERIFY(parser.parse(src));
    const auto q = parser.buildDocumentQueries();
    QCOMPARE(q.codeBlocks.size(), 1);
    QCOMPARE(q.codeBlocks[0].language, QStringLiteral("cpp"));
    QCOMPARE(q.codeBlocks[0].lineCount, 2);
    QVERIFY(q.codeBlocks[0].sourceOffset > 0);
}

void TstTreeSitterParser::documentQueries_fencedCodeBlock_emptyLanguage()
{
    TreeSitterParser parser;
    QVERIFY(parser.parse(QStringLiteral("```\nfoo\n```\n")));
    const auto q = parser.buildDocumentQueries();
    QCOMPARE(q.codeBlocks.size(), 1);
    QCOMPARE(q.codeBlocks[0].language, QString());
    QCOMPARE(q.codeBlocks[0].lineCount, 1);
}

void TstTreeSitterParser::documentQueries_fencedCodeBlock_multipleBlocks()
{
    TreeSitterParser parser;
    const QString src = QStringLiteral(
        "```py\n"
        "x = 1\n"
        "```\n\n"
        "text\n\n"
        "```rust\n"
        "fn main() {}\n"
        "fn other() {}\n"
        "```\n");
    QVERIFY(parser.parse(src));
    const auto q = parser.buildDocumentQueries();
    QCOMPARE(q.codeBlocks.size(), 2);
    QCOMPARE(q.codeBlocks[0].language, QStringLiteral("py"));
    QCOMPARE(q.codeBlocks[0].lineCount, 1);
    QCOMPARE(q.codeBlocks[1].language, QStringLiteral("rust"));
    QCOMPARE(q.codeBlocks[1].lineCount, 2);
    QVERIFY(q.codeBlocks[1].sourceOffset > q.codeBlocks[0].sourceOffset);
}
```

- [ ] **Step 4: Run tests — expect fail**

```
cd /home/clinton/dev/Corbomite/.worktrees/markoff-fold-v2
cmake --build build --target tst_markoff_parser_<name>  # substitute existing binary name
ctest --test-dir build -R tst_markoff_parser --output-on-failure
```

Expected: new slots fail (`codeBlocks` field empty).

- [ ] **Step 5: Implement the CST walker**

In `libs/markoff-parser/src/TreeSitterParser.cpp`, add a helper next to `collectHeadingsForQuery`:

```cpp
static void collectCodeBlocksForQuery(TSNode node, const QByteArray &utf8,
                                       QList<CodeBlockInfo> &codeBlocks)
{
    const char *nodeType = ts_node_type(node);
    if (strcmp(nodeType, "fenced_code_block") == 0) {
        CodeBlockInfo info;
        info.sourceOffset = static_cast<int>(ts_node_start_byte(node));

        // Find child nodes: info_string (optional), code_fence_content.
        const uint32_t childCount = ts_node_child_count(node);
        for (uint32_t i = 0; i < childCount; ++i) {
            TSNode child = ts_node_child(node, i);
            const char *ct = ts_node_type(child);
            if (strcmp(ct, "info_string") == 0) {
                const int s = static_cast<int>(ts_node_start_byte(child));
                const int e = static_cast<int>(ts_node_end_byte(child));
                const QString text = QString::fromUtf8(utf8.constData() + s, e - s).trimmed();
                // First whitespace-delimited word is the language.
                const int sp = text.indexOf(QRegularExpression(QStringLiteral(R"(\s)")));
                info.language = (sp < 0) ? text : text.left(sp);
            } else if (strcmp(ct, "code_fence_content") == 0) {
                const int s = static_cast<int>(ts_node_start_byte(child));
                const int e = static_cast<int>(ts_node_end_byte(child));
                const QByteArray slice = utf8.mid(s, e - s);
                info.lineCount = static_cast<int>(slice.count('\n'));
                // Trailing newline of the last content line is counted;
                // if the content ends right before the closing fence, the
                // count is exactly the number of content lines.
            }
        }
        codeBlocks.append(info);
        return; // Do not recurse into fence children; we've handled them.
    }
    const uint32_t n = ts_node_child_count(node);
    for (uint32_t i = 0; i < n; ++i)
        collectCodeBlocksForQuery(ts_node_child(node, i), utf8, codeBlocks);
}
```

Add `#include <QRegularExpression>` at the top of the file if not already present.

- [ ] **Step 6: Invoke the walker from `buildDocumentQueries`**

Find the existing `buildDocumentQueries()` implementation (around line 896 of `TreeSitterParser.cpp`). After the call to `collectHeadingsForQuery`, add:

```cpp
collectCodeBlocksForQuery(blockRoot, m_utf8, result.codeBlocks);
```

- [ ] **Step 7: Run tests — expect pass**

```
cmake --build build --target tst_markoff_parser_<name>
ctest --test-dir build -R tst_markoff_parser --output-on-failure
```

Expected: all 3 new slots pass. Existing slots still pass.

- [ ] **Step 8: Commit**

```
git add libs/markoff-parser/include/markoff-parser/Document.h \
         libs/markoff-parser/include/markoff-parser/TreeSitterParser.h \
         libs/markoff-parser/src/TreeSitterParser.cpp \
         libs/markoff-parser/tests/tst_tree_sitter_parser.cpp
git commit -m "feat(markoff-parser): emit CodeBlockInfo from document queries"
```

---

## Task 2: `FoldableRegion` struct + helper

**Files:**
- Modify: `libs/markoff/include/markoff/FoldingTypes.h`
- Modify: `libs/markoff/src/FoldingTypes.cpp`
- Modify: `libs/markoff/tests/tst_folding_model.cpp`

- [ ] **Step 1: Declare `FoldableRegion`**

In `libs/markoff/include/markoff/FoldingTypes.h`, after the `FoldRegionKey` alias:

```cpp
#include <markoff-parser/Document.h>

namespace Markoff {

struct FoldableRegion {
    enum Type { Heading, CodeBlock };
    Type type = Heading;
    FoldRegionKey path;
    int sourceOffset = 0;
    int level = 0;       // 1..6 for Heading; 0 for CodeBlock

    // Heading-specific (valid only when type == Heading):
    HeadingInfo info;

    // Code-block-specific (valid only when type == CodeBlock):
    QString language;
    int lineCount = 0;
};

/// Compute `code:N` path segments by walking regions in document
/// order, resetting the ordinal whenever a heading boundary is crossed.
/// Called after `computeHeadingPaths` has populated heading paths in
/// the input list; code-block entries have their `path` initialized
/// to just the enclosing heading path (no "code:N" yet) and this
/// function appends the ordinal segment.
void assignCodeBlockOrdinals(QList<FoldableRegion> &regions);

} // namespace Markoff
```

- [ ] **Step 2: Write the failing tests**

Append to `libs/markoff/tests/tst_folding_model.cpp` (in a new class before the `main` runner):

```cpp
class TstCodeBlockOrdinals : public QObject {
    Q_OBJECT
private slots:
    void assignOrdinals_singleBlockNoHeading_getsCodeZero();
    void assignOrdinals_twoBlocksSameSection_incrementOrdinal();
    void assignOrdinals_headingBetweenBlocks_resetsOrdinal();
    void assignOrdinals_preambleBlocks_haveNoHeadingPrefix();
};

static FoldableRegion mkHeading(QStringList path, int level) {
    FoldableRegion r;
    r.type = FoldableRegion::Heading;
    r.path = path;
    r.level = level;
    r.info = HeadingInfo{level, path.last(), 0};
    return r;
}

static FoldableRegion mkCode(QStringList parentPath) {
    FoldableRegion r;
    r.type = FoldableRegion::CodeBlock;
    r.path = parentPath; // assignCodeBlockOrdinals will append "code:N"
    return r;
}

void TstCodeBlockOrdinals::assignOrdinals_singleBlockNoHeading_getsCodeZero() {
    QList<FoldableRegion> regions{ mkCode({}) };
    assignCodeBlockOrdinals(regions);
    QCOMPARE(regions[0].path, (QStringList{"code:0"}));
}

void TstCodeBlockOrdinals::assignOrdinals_twoBlocksSameSection_incrementOrdinal() {
    QList<FoldableRegion> regions{
        mkHeading({"A"}, 1),
        mkCode({"A"}),
        mkCode({"A"}),
    };
    assignCodeBlockOrdinals(regions);
    QCOMPARE(regions[1].path, (QStringList{"A","code:0"}));
    QCOMPARE(regions[2].path, (QStringList{"A","code:1"}));
}

void TstCodeBlockOrdinals::assignOrdinals_headingBetweenBlocks_resetsOrdinal() {
    QList<FoldableRegion> regions{
        mkHeading({"A"}, 1),
        mkCode({"A"}),
        mkHeading({"A","B"}, 2),
        mkCode({"A","B"}),
    };
    assignCodeBlockOrdinals(regions);
    QCOMPARE(regions[1].path, (QStringList{"A","code:0"}));
    QCOMPARE(regions[3].path, (QStringList{"A","B","code:0"}));
}

void TstCodeBlockOrdinals::assignOrdinals_preambleBlocks_haveNoHeadingPrefix() {
    QList<FoldableRegion> regions{
        mkCode({}),
        mkCode({}),
        mkHeading({"A"}, 1),
        mkCode({"A"}),
    };
    assignCodeBlockOrdinals(regions);
    QCOMPARE(regions[0].path, (QStringList{"code:0"}));
    QCOMPARE(regions[1].path, (QStringList{"code:1"}));
    QCOMPARE(regions[3].path, (QStringList{"A","code:0"}));
}
```

Register the class in the custom `main` runner at the bottom of the file:

```cpp
{ TstCodeBlockOrdinals t; status |= QTest::qExec(&t, argc, argv); }
```

- [ ] **Step 3: Run test — expect fail**

```
cmake --build build --target tst_markoff_folding_model
ctest --test-dir build -R tst_markoff_folding_model --output-on-failure
```

Expected: new slots fail with "undefined reference to assignCodeBlockOrdinals".

- [ ] **Step 4: Implement `assignCodeBlockOrdinals`**

In `libs/markoff/src/FoldingTypes.cpp`, append:

```cpp
void assignCodeBlockOrdinals(QList<FoldableRegion> &regions)
{
    int ordinal = 0;
    QStringList currentHeadingPath;
    for (int i = 0; i < regions.size(); ++i) {
        FoldableRegion &r = regions[i];
        if (r.type == FoldableRegion::Heading) {
            currentHeadingPath = r.path;
            ordinal = 0;
            continue;
        }
        // Code block: append the ordinal segment.
        QStringList path = currentHeadingPath;
        path.append(QStringLiteral("code:%1").arg(ordinal));
        r.path = path;
        ++ordinal;
    }
}
```

- [ ] **Step 5: Run tests — expect pass**

```
cmake --build build --target tst_markoff_folding_model
ctest --test-dir build -R tst_markoff_folding_model --output-on-failure
```

Expected: 4 new slots pass; all existing slots still pass.

- [ ] **Step 6: Commit**

```
git add libs/markoff/include/markoff/FoldingTypes.h \
         libs/markoff/src/FoldingTypes.cpp \
         libs/markoff/tests/tst_folding_model.cpp
git commit -m "feat(markoff): FoldableRegion + code-block path ordinal assignment"
```

---

## Task 3: Refactor `FoldingModel` to unified `m_regions`

**Files:**
- Modify: `libs/markoff/src/FoldingModel.h`
- Modify: `libs/markoff/src/FoldingModel.cpp`
- Modify: `libs/markoff/tests/tst_folding_model.cpp`

Replace the heading-only cache with a region list. Keep `headings()` as a filter view so existing tests continue to pass unchanged.

- [ ] **Step 1: Update the header**

In `libs/markoff/src/FoldingModel.h`, replace the `m_headings` member and the `HeadingEntry` struct usage:

```cpp
class FoldingModel : public QObject {
    Q_OBJECT
public:
    // Existing HeadingEntry wrapper kept for v1 compat.
    struct HeadingEntry {
        FoldRegionKey path;
        HeadingInfo info;
    };

    explicit FoldingModel(QObject *parent = nullptr);

    bool isFolded(const FoldRegionKey &path) const;
    QList<FoldRegionKey> foldedPaths() const;
    QList<FoldRegionKey> allPaths() const;

    // --- v1-compatible views over m_regions ---
    QList<HeadingEntry> headings() const;
    QList<FoldableRegion> codeBlockRegions() const;
    const QList<FoldableRegion> &regions() const { return m_regions; }

    bool isHiddenByFold(const FoldRegionKey &path) const;

    // Existing individual mutations — unchanged, paths are opaque.
    void fold(const FoldRegionKey &path);
    void unfold(const FoldRegionKey &path);
    void toggle(const FoldRegionKey &path);

    // Bulk ops — operate on regions[] with type-aware predicates.
    void foldAll();
    void unfoldAll();
    void foldAllAtLevel(int level);        // headings only
    void unfoldAllAtLevel(int level);      // headings only
    void foldLevel(int n);                 // headings only
    void unfoldLevel(int n);               // headings only

    // New code-block bulk ops:
    void foldAllCodeBlocks();
    void unfoldAllCodeBlocks();
    void foldAllCodeBlocksInSection(const FoldRegionKey &headingPath);
    void unfoldAllCodeBlocksInSection(const FoldRegionKey &headingPath);

    QJsonObject serialize() const;
    void restore(const QJsonObject &);

    // reconcile NOW takes a precomputed region list (not a QList<HeadingInfo>).
    void reconcile(const QList<FoldableRegion> &newRegions);

    QList<FoldRegionKey> unfoldAncestors(const FoldRegionKey &path);

    // Test-only.
    void setRegionsForTesting(QList<FoldableRegion> r) { m_regions = std::move(r); }
    // Kept for v1-compat tests that predate the refactor.
    void setHeadingsForTesting(QList<HeadingEntry> h);

Q_SIGNALS:
    void foldStateChanged();

private:
    QSet<FoldRegionKey> m_folded;
    QList<FoldableRegion> m_regions;
};
```

- [ ] **Step 2: Update `FoldingModel.cpp`**

Replace the impl file to use `m_regions`. Key changes:

```cpp
QList<FoldingModel::HeadingEntry> FoldingModel::headings() const {
    QList<HeadingEntry> out;
    for (const auto &r : m_regions)
        if (r.type == FoldableRegion::Heading)
            out.append({ r.path, r.info });
    return out;
}

QList<FoldableRegion> FoldingModel::codeBlockRegions() const {
    QList<FoldableRegion> out;
    for (const auto &r : m_regions)
        if (r.type == FoldableRegion::CodeBlock)
            out.append(r);
    return out;
}

void FoldingModel::foldAll() {
    bool changed = false;
    for (const auto &r : m_regions) {
        if (!m_folded.contains(r.path)) { m_folded.insert(r.path); changed = true; }
    }
    if (changed) emit foldStateChanged();
}

void FoldingModel::unfoldAll() {
    if (m_folded.isEmpty()) return;
    m_folded.clear();
    emit foldStateChanged();
}

void FoldingModel::foldAllAtLevel(int level) {
    bool changed = false;
    for (const auto &r : m_regions) {
        if (r.type == FoldableRegion::Heading && r.level == level
            && !m_folded.contains(r.path)) {
            m_folded.insert(r.path); changed = true;
        }
    }
    if (changed) emit foldStateChanged();
}

void FoldingModel::unfoldAllAtLevel(int level) {
    bool changed = false;
    for (const auto &r : m_regions) {
        if (r.type == FoldableRegion::Heading && r.level == level
            && m_folded.contains(r.path)) {
            m_folded.remove(r.path); changed = true;
        }
    }
    if (changed) emit foldStateChanged();
}

void FoldingModel::foldLevel(int n) {
    bool changed = false;
    for (const auto &r : m_regions) {
        if (r.type == FoldableRegion::Heading && r.level >= n
            && !m_folded.contains(r.path)) {
            m_folded.insert(r.path); changed = true;
        }
    }
    if (changed) emit foldStateChanged();
}

void FoldingModel::unfoldLevel(int n) {
    bool changed = false;
    for (const auto &r : m_regions) {
        if (r.type == FoldableRegion::Heading && r.level >= n
            && m_folded.contains(r.path)) {
            m_folded.remove(r.path); changed = true;
        }
    }
    if (changed) emit foldStateChanged();
}

void FoldingModel::foldAllCodeBlocks() {
    bool changed = false;
    for (const auto &r : m_regions) {
        if (r.type == FoldableRegion::CodeBlock && !m_folded.contains(r.path)) {
            m_folded.insert(r.path); changed = true;
        }
    }
    if (changed) emit foldStateChanged();
}

void FoldingModel::unfoldAllCodeBlocks() {
    bool changed = false;
    for (const auto &r : m_regions) {
        if (r.type == FoldableRegion::CodeBlock && m_folded.contains(r.path)) {
            m_folded.remove(r.path); changed = true;
        }
    }
    if (changed) emit foldStateChanged();
}

void FoldingModel::foldAllCodeBlocksInSection(const FoldRegionKey &sectionPath) {
    bool changed = false;
    for (const auto &r : m_regions) {
        if (r.type != FoldableRegion::CodeBlock) continue;
        // A code block is "in section" iff its path-prefix (all but the last
        // segment) equals sectionPath.
        if (r.path.size() <= 1) continue;
        const QStringList prefix = r.path.mid(0, r.path.size() - 1);
        if (prefix == sectionPath && !m_folded.contains(r.path)) {
            m_folded.insert(r.path); changed = true;
        }
    }
    if (changed) emit foldStateChanged();
}

void FoldingModel::unfoldAllCodeBlocksInSection(const FoldRegionKey &sectionPath) {
    bool changed = false;
    for (const auto &r : m_regions) {
        if (r.type != FoldableRegion::CodeBlock) continue;
        if (r.path.size() <= 1) continue;
        const QStringList prefix = r.path.mid(0, r.path.size() - 1);
        if (prefix == sectionPath && m_folded.contains(r.path)) {
            m_folded.remove(r.path); changed = true;
        }
    }
    if (changed) emit foldStateChanged();
}

void FoldingModel::reconcile(const QList<FoldableRegion> &newRegions) {
    m_regions = newRegions;
    QSet<FoldRegionKey> newPathSet;
    for (const auto &r : m_regions) newPathSet.insert(r.path);

    const auto prev = m_folded;
    auto it = m_folded.begin();
    while (it != m_folded.end()) {
        if (!newPathSet.contains(*it)) it = m_folded.erase(it);
        else ++it;
    }
    if (prev != m_folded) emit foldStateChanged();
}

void FoldingModel::setHeadingsForTesting(QList<HeadingEntry> h) {
    QList<FoldableRegion> regions;
    regions.reserve(h.size());
    for (const auto &entry : h) {
        FoldableRegion r;
        r.type = FoldableRegion::Heading;
        r.path = entry.path;
        r.info = entry.info;
        r.level = entry.info.level;
        r.sourceOffset = entry.info.sourceOffset;
        regions.append(r);
    }
    m_regions = std::move(regions);
}
```

`allPaths()` becomes:

```cpp
QList<FoldRegionKey> FoldingModel::allPaths() const {
    QList<FoldRegionKey> r;
    r.reserve(m_regions.size());
    for (const auto &reg : m_regions) r << reg.path;
    return r;
}
```

- [ ] **Step 3: Build & run existing tests (v1 compat check)**

```
cmake --build build --target tst_markoff_folding_model tst_markoff_folding_reconcile tst_markoff_fold_persistence
ctest --test-dir build -R "tst_markoff_(folding_|fold_persistence)" --output-on-failure
```

Expected: all pre-existing slots still pass. The `TstFoldingReconcile` class uses `m.reconcile(QList<HeadingInfo>)` which no longer compiles — this test class will be updated in Task 4.

If v1 reconcile tests fail to compile, that's expected. Mark them failing; Task 4 fixes them.

- [ ] **Step 4: Commit (tests may be red for reconcile — explicitly noted)**

```
git add libs/markoff/src/FoldingModel.h libs/markoff/src/FoldingModel.cpp
git commit -m "refactor(markoff): FoldingModel stores unified FoldableRegion list

reconcile() signature now takes QList<FoldableRegion>. tst_folding_reconcile
will be updated in the next commit."
```

---

## Task 4: Update `tst_folding_reconcile` for new signature

**Files:**
- Modify: `libs/markoff/tests/tst_folding_reconcile.cpp`

The reconcile tests passed `QList<HeadingInfo>` which is gone. Port them.

- [ ] **Step 1: Add a small adapter helper at the top of the test file**

```cpp
static QList<FoldableRegion> headingsToRegions(const QList<HeadingInfo> &hs)
{
    // Mirror the production path computation for headings.
    const QList<FoldRegionKey> paths = computeHeadingPaths(hs);
    QList<FoldableRegion> regions;
    for (int i = 0; i < hs.size(); ++i) {
        FoldableRegion r;
        r.type = FoldableRegion::Heading;
        r.path = paths[i];
        r.level = hs[i].level;
        r.sourceOffset = hs[i].sourceOffset;
        r.info = hs[i];
        regions.append(r);
    }
    return regions;
}
```

- [ ] **Step 2: Replace every `m.reconcile(...)` call**

Wherever the existing test does `m.reconcile({h(1, "A"), h(2, "B")})`, change to
`m.reconcile(headingsToRegions({h(1, "A"), h(2, "B")}))`. Same for every slot in this file.

- [ ] **Step 3: Build and run**

```
cmake --build build --target tst_markoff_folding_reconcile
ctest --test-dir build -R tst_markoff_folding_reconcile --output-on-failure
```

Expected: all 10 slots pass.

- [ ] **Step 4: Commit**

```
git add libs/markoff/tests/tst_folding_reconcile.cpp
git commit -m "test(markoff): port reconcile tests to FoldableRegion signature"
```

---

## Task 5: SceneCoordinator computes regions with code blocks

**Files:**
- Modify: `libs/markoff/src/SceneCoordinator.cpp`

The v1 `ensureHeadingMap` already does a fresh parse with `TreeSitterParser` on `toMarkdown()`. Extend it to produce `FoldableRegion`s for both headings and code blocks in document order.

- [ ] **Step 1: Add a new helper `computeRegions()` on SceneCoordinator**

In `libs/markoff/src/SceneCoordinator.h`, add to `private:`:

```cpp
QList<FoldableRegion> computeRegions() const;
```

And `#include <markoff/FoldingTypes.h>` at the top.

In `libs/markoff/src/SceneCoordinator.cpp`, implement:

```cpp
QList<FoldableRegion> SceneCoordinator::computeRegions() const
{
    const QString md = toMarkdown();
    TreeSitterParser rawParser;
    rawParser.parse(md);
    const auto q = rawParser.buildDocumentQueries();

    // Merge headings and code blocks into a single list ordered by sourceOffset.
    const auto headingPaths = computeHeadingPaths(q.headings);
    QList<FoldableRegion> regions;
    regions.reserve(q.headings.size() + q.codeBlocks.size());
    for (int i = 0; i < q.headings.size(); ++i) {
        FoldableRegion r;
        r.type = FoldableRegion::Heading;
        r.path = headingPaths[i];
        r.level = q.headings[i].level;
        r.sourceOffset = q.headings[i].sourceOffset;
        r.info = q.headings[i];
        regions.append(r);
    }
    for (const auto &cb : q.codeBlocks) {
        FoldableRegion r;
        r.type = FoldableRegion::CodeBlock;
        r.path = {}; // assignCodeBlockOrdinals fills this
        r.level = 0;
        r.sourceOffset = cb.sourceOffset;
        r.language = cb.language;
        r.lineCount = cb.lineCount;
        regions.append(r);
    }
    std::sort(regions.begin(), regions.end(),
              [](const FoldableRegion &a, const FoldableRegion &b) {
                  return a.sourceOffset < b.sourceOffset;
              });
    assignCodeBlockOrdinals(regions);
    return regions;
}
```

- [ ] **Step 2: Replace the heading-list usage inside `ensureHeadingMap`**

The current `ensureHeadingMap` parses `toMarkdown()` to get `rawHeadings` and then walks items building a `(itemIdx, blockNumber) → headingIdx` map keyed to the heading sourceOffsets.

Update it: parse once, get the full `QList<FoldableRegion>` via `computeRegions()`, and build `(itemIdx, blockNumber) → regionIdx` keyed to each region's sourceOffset. The map type changes from
`QHash<QPair<int,int>, int> m_blockToHeadingIdx`
to
`QHash<QPair<int,int>, int> m_blockToRegionIdx`.

Rename accordingly in `SceneCoordinator.h`:

```cpp
mutable QHash<QPair<int,int>, int> m_blockToRegionIdx;
int regionAtBlock(int itemIdx, int blockNumber) const;
```

And update `ensureHeadingMap`'s body to use the unified regions:

```cpp
void SceneCoordinator::ensureHeadingMap() const
{
    if (!m_headingMapDirty) return;
    m_blockToRegionIdx.clear();
    m_headingMapDirty = false;
    if (!m_foldingModel) return;

    const QList<FoldableRegion> regions = computeRegions();
    if (regions.isEmpty()) return;

    const QByteArray utf8 = toMarkdown().toUtf8();
    QHash<int, int> lineToRegionIdx;
    lineToRegionIdx.reserve(regions.size());
    for (int i = 0; i < regions.size(); ++i) {
        const int line = sourceLineAt(utf8, regions[i].sourceOffset);
        lineToRegionIdx.insert(line, i);
    }

    // Walk items as before, but match block lines against lineToRegionIdx.
    int srcLine = 0;
    for (int itemIdx = 0; itemIdx < m_items.size(); ++itemIdx) {
        if (itemIdx > 0) {
            const bool prevBlock = !m_items[itemIdx - 1]->isTextItem();
            const bool currBlock = !m_items[itemIdx]->isTextItem();
            srcLine += (prevBlock || currBlock) ? 2 : 1;
        }
        auto *mti = dynamic_cast<MarkdownTextItem *>(m_items[itemIdx]);
        if (mti) {
            int blockLine = srcLine;
            for (QTextBlock block = mti->document()->begin();
                 block.isValid(); block = block.next()) {
                auto it = lineToRegionIdx.constFind(blockLine);
                if (it != lineToRegionIdx.constEnd())
                    m_blockToRegionIdx.insert({itemIdx, block.blockNumber()}, *it);
                int blockNewlines = /* existing ORC-expanding newline count */ 0;
                // ... existing per-block newline computation unchanged ...
                blockLine += blockNewlines;
                if (block.next().isValid()) blockLine += 1;
            }
            srcLine = blockLine;
        } else {
            // Non-MTI items may themselves be regions (code blocks via
            // TableBlockItem or images via ImageBlockItem). If a region's
            // line equals srcLine, map the ITEM-level key (itemIdx, 0).
            auto it = lineToRegionIdx.constFind(srcLine);
            if (it != lineToRegionIdx.constEnd())
                m_blockToRegionIdx.insert({itemIdx, 0}, *it);
            srcLine += int(m_items[itemIdx]->toMarkdown().count(QLatin1Char('\n')));
        }
    }
}

int SceneCoordinator::regionAtBlock(int itemIdx, int blockNumber) const
{
    ensureHeadingMap();
    return m_blockToRegionIdx.value({itemIdx, blockNumber}, -1);
}
```

Keep the old `headingAtBlock` as a thin wrapper for gradual migration:

```cpp
int SceneCoordinator::headingAtBlock(int itemIdx, int blockNumber) const
{
    ensureHeadingMap();
    const int r = m_blockToRegionIdx.value({itemIdx, blockNumber}, -1);
    if (r < 0) return -1;
    if (!m_foldingModel) return -1;
    const auto &regs = m_foldingModel->regions();
    if (r >= regs.size()) return -1;
    if (regs[r].type != FoldableRegion::Heading) return -1;
    // Convert region index → heading index (count headings at or before r).
    int headingIdx = 0;
    for (int i = 0; i < r; ++i)
        if (regs[i].type == FoldableRegion::Heading) ++headingIdx;
    return headingIdx;
}
```

- [ ] **Step 3: Build (no tests yet for this task)**

```
cmake --build build --target markoff 2>&1 | tail -10
```

Expected: clean build (test binaries may still fail to link if they use headingAtBlock semantics that depend on m_regions being populated via Editor; full test run after Task 6).

- [ ] **Step 4: Commit**

```
git add libs/markoff/src/SceneCoordinator.h libs/markoff/src/SceneCoordinator.cpp
git commit -m "feat(markoff): SceneCoordinator computes FoldableRegion list from CST"
```

---

## Task 6: Editor emits regions (not headings) to FoldingModel

**Files:**
- Modify: `libs/markoff/include/markoff/Editor.h`
- Modify: `libs/markoff/src/Editor.cpp`

The v1 Editor wires `headingsChanged → FoldingModel::reconcile(QList<HeadingInfo>)`. Switch the reconcile source to `SceneCoordinator::computeRegions()`.

- [ ] **Step 1: Expose `computeRegions()` publicly on SceneCoordinator**

In `libs/markoff/src/SceneCoordinator.h`, move the declaration from `private:` to `public:`.

- [ ] **Step 2: Update Editor's constructor wiring**

In `libs/markoff/src/Editor.cpp`, find the v1 connect:

```cpp
connect(this, &Editor::headingsChanged,
        m_foldingModel, &FoldingModel::reconcile);
```

Replace with:

```cpp
connect(this, &Editor::headingsChanged, m_foldingModel, [this]() {
    m_foldingModel->reconcile(m_coordinator->computeRegions());
});
```

The existing `headingsChanged` signal still fires (for Corbomite consumers that care about just headings); we simply use it as a trigger and compute regions freshly.

- [ ] **Step 3: Run fold tests**

```
cmake --build build --target tst_markoff_folding_integration tst_markoff_fold_gutter tst_markoff_folding_model tst_markoff_folding_reconcile tst_markoff_fold_persistence
ctest --test-dir build -R "tst_markoff_(fold|folding)" --output-on-failure
```

Expected: all 5 suites pass (v1 behaviour preserved).

- [ ] **Step 4: Commit**

```
git add libs/markoff/include/markoff/Editor.h libs/markoff/src/Editor.cpp libs/markoff/src/SceneCoordinator.h
git commit -m "feat(markoff): Editor feeds FoldableRegion list to FoldingModel"
```

---

## Task 7: TableBlockItem folded paint mode

**Files:**
- Modify: `libs/markoff/src/TableBlockItem.h`
- Modify: `libs/markoff/src/TableBlockItem.cpp`
- Create: `libs/markoff/tests/tst_code_block_paint.cpp`
- Modify: `libs/markoff/tests/CMakeLists.txt`

- [ ] **Step 1: Declare the folded state**

In `libs/markoff/src/TableBlockItem.h`, add to the public interface:

```cpp
void setFolded(bool folded, const QString &language = QString(), int lineCount = 0);
bool isFolded() const { return m_folded; }
```

And to private members:

```cpp
bool m_folded = false;
QString m_foldedLanguage;
int m_foldedLineCount = 0;
```

- [ ] **Step 2: Write the failing paint tests**

Create `libs/markoff/tests/tst_code_block_paint.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include "TableBlockItem.h"

using namespace Markoff;

class TstCodeBlockPaint : public QObject {
    Q_OBJECT
private slots:
    void setFolded_true_reducesBoundingRect();
    void setFolded_false_restoresBoundingRect();
    void paint_folded_rendersAnyPixels();
    void paint_foldedSingular_formatsOneLine();
};

static TableBlockItem *makeItem(const QString &fence) {
    auto *it = new TableBlockItem(fence, 400.0);
    return it;
}

void TstCodeBlockPaint::setFolded_true_reducesBoundingRect()
{
    auto *it = makeItem(QStringLiteral("```cpp\nint a = 1;\nint b = 2;\n```"));
    const qreal unfoldedHeight = it->boundingRect().height();
    it->setFolded(true, QStringLiteral("cpp"), 2);
    QVERIFY(it->boundingRect().height() < unfoldedHeight);
    delete it;
}

void TstCodeBlockPaint::setFolded_false_restoresBoundingRect()
{
    auto *it = makeItem(QStringLiteral("```cpp\nint a = 1;\nint b = 2;\n```"));
    const qreal unfoldedHeight = it->boundingRect().height();
    it->setFolded(true, QStringLiteral("cpp"), 2);
    it->setFolded(false);
    QCOMPARE(it->boundingRect().height(), unfoldedHeight);
    delete it;
}

void TstCodeBlockPaint::paint_folded_rendersAnyPixels()
{
    auto *it = makeItem(QStringLiteral("```cpp\nint a = 1;\nint b = 2;\n```"));
    it->setFolded(true, QStringLiteral("cpp"), 2);
    QImage img(int(it->boundingRect().width()),
               int(it->boundingRect().height()),
               QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    QStyleOptionGraphicsItem opt;
    it->paint(&p, &opt, nullptr);
    p.end();
    bool hasAny = false;
    for (int y = 0; y < img.height() && !hasAny; ++y)
        for (int x = 0; x < img.width(); ++x)
            if (qAlpha(img.pixel(x, y)) > 0) { hasAny = true; break; }
    QVERIFY(hasAny);
    delete it;
}

void TstCodeBlockPaint::paint_foldedSingular_formatsOneLine()
{
    // Not a paint test per se — verifies the summary-string formatting
    // helper renders "(1 line)" not "(1 lines)".
    auto *it = makeItem(QStringLiteral("```cpp\nint a;\n```"));
    it->setFolded(true, QStringLiteral("cpp"), 1);
    // Expose a summaryForTesting() method on TableBlockItem that returns
    // the summary string (see impl below).
    QCOMPARE(it->summaryForTesting(),
             QStringLiteral("```cpp (1 line)"));
    delete it;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    TstCodeBlockPaint t;
    return QTest::qExec(&t, argc, argv);
}
#include "tst_code_block_paint.moc"
```

Append to `libs/markoff/tests/CMakeLists.txt`:

```cmake
add_executable(tst_markoff_code_block_paint tst_code_block_paint.cpp)
add_test(NAME tst_markoff_code_block_paint COMMAND tst_markoff_code_block_paint)
target_link_libraries(tst_markoff_code_block_paint PRIVATE Qt6::Test Qt6::Widgets markoff)
target_include_directories(tst_markoff_code_block_paint PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set_tests_properties(tst_markoff_code_block_paint PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Run — expect fail**

```
cmake --build build --target tst_markoff_code_block_paint 2>&1 | tail -10
```

Expected: build fails — `setFolded`/`summaryForTesting` undefined.

- [ ] **Step 4: Implement `setFolded` + folded paint**

In `libs/markoff/src/TableBlockItem.cpp`:

```cpp
void TableBlockItem::setFolded(bool folded, const QString &language, int lineCount)
{
    if (m_folded == folded
        && m_foldedLanguage == language
        && m_foldedLineCount == lineCount) return;
    prepareGeometryChange();
    m_folded = folded;
    m_foldedLanguage = language;
    m_foldedLineCount = lineCount;
    update();
}

QString TableBlockItem::summaryForTesting() const
{
    const QString countPart = m_foldedLineCount == 1
        ? QStringLiteral("(1 line)")
        : QStringLiteral("(%1 lines)").arg(m_foldedLineCount);
    if (m_foldedLanguage.isEmpty())
        return QStringLiteral("``` %1").arg(countPart);
    return QStringLiteral("```%1 %2").arg(m_foldedLanguage, countPart);
}
```

Add the public `QString summaryForTesting() const;` declaration to the header.

In `TableBlockItem::boundingRect()`, prepend:

```cpp
if (m_folded) {
    const qreal h = QFontMetrics(m_font).lineSpacing() + 6;
    return QRectF(0, 0, m_maxWidth, h);
}
// ... existing unfolded bounding-rect computation ...
```

(Replace `m_font` with the actual font accessor used by TableBlockItem — check the class; it likely holds a `QFont m_font` already for code rendering.)

In `TableBlockItem::paint(...)`, at the top of the body:

```cpp
if (m_folded) {
    painter->save();
    painter->fillRect(boundingRect(), QColor(245, 245, 245));  // match code bg
    painter->setPen(Qt::darkGray);
    painter->setFont(m_font);
    const QString text = summaryForTesting();
    painter->drawText(boundingRect().adjusted(8, 0, -4, 0),
                      Qt::AlignVCenter | Qt::AlignLeft, text);
    painter->restore();
    return;
}
// ... existing unfolded paint ...
```

- [ ] **Step 5: Run tests — expect pass**

```
cmake --build build --target tst_markoff_code_block_paint
ctest --test-dir build -R tst_markoff_code_block_paint --output-on-failure
```

Expected: 4 slots pass.

- [ ] **Step 6: Commit**

```
git add libs/markoff/src/TableBlockItem.h libs/markoff/src/TableBlockItem.cpp \
         libs/markoff/tests/tst_code_block_paint.cpp libs/markoff/tests/CMakeLists.txt
git commit -m "feat(markoff): TableBlockItem folded paint mode"
```

---

## Task 8: SceneCoordinator visibility handles code-block regions

**Files:**
- Modify: `libs/markoff/src/SceneCoordinator.cpp`

Extend `applyFoldVisibility` to flip `TableBlockItem::setFolded` for folded code-block regions.

- [ ] **Step 1: Add the code-block visibility branch**

In `libs/markoff/src/SceneCoordinator.cpp`, find the non-MTI branch in `applyFoldVisibility`:

```cpp
if (!mti) {
    // Non-text item: hide/show based on enclosing heading.
    const QStringList path = (hIdx >= 0) ? hs[hIdx].path : QStringList{};
    bool hidden = !path.isEmpty()
        && (m_foldingModel->isFolded(path)
            || m_foldingModel->isHiddenByFold(path));
    m_items[itemIdx]->asGraphicsItem()->setVisible(!hidden);
    continue;
}
```

Replace with:

```cpp
if (!mti) {
    // Resolve this item's own region (if any) — code blocks live here.
    const int rIdx = m_blockToRegionIdx.value({itemIdx, 0}, -1);
    const QList<FoldableRegion> &regs = m_foldingModel->regions();
    const bool isCodeBlock = rIdx >= 0 && rIdx < regs.size()
        && regs[rIdx].type == FoldableRegion::CodeBlock;

    // First: does an ancestor heading fold hide this item entirely?
    const QStringList encPath = (hIdx >= 0) ? hs[hIdx].path : QStringList{};
    bool hiddenByHeading = !encPath.isEmpty()
        && (m_foldingModel->isFolded(encPath)
            || m_foldingModel->isHiddenByFold(encPath));

    auto *tbi = dynamic_cast<TableBlockItem *>(m_items[itemIdx]->asGraphicsItem());
    if (isCodeBlock && tbi) {
        const bool selfFolded = m_foldingModel->isFolded(regs[rIdx].path);
        tbi->setFolded(selfFolded, regs[rIdx].language, regs[rIdx].lineCount);
        m_items[itemIdx]->asGraphicsItem()->setVisible(!hiddenByHeading);
    } else {
        m_items[itemIdx]->asGraphicsItem()->setVisible(!hiddenByHeading);
    }
    continue;
}
```

Add `#include "TableBlockItem.h"` at the top if not already there.

- [ ] **Step 2: Run all fold tests to confirm no regression**

```
cmake --build build --target tst_markoff_folding_integration tst_markoff_fold_gutter tst_markoff_folding_model tst_markoff_folding_reconcile tst_markoff_fold_persistence tst_markoff_code_block_paint
ctest --test-dir build -R "tst_markoff_(fold|code_block|folding)" --output-on-failure
```

Expected: all pass.

- [ ] **Step 3: Commit**

```
git add libs/markoff/src/SceneCoordinator.cpp
git commit -m "feat(markoff): SceneCoordinator folds code-block TableBlockItems"
```

---

## Task 9: FoldGutter iterates regions

**Files:**
- Modify: `libs/markoff/src/FoldGutter.cpp`
- Modify: `libs/markoff/src/SceneCoordinator.h` / `.cpp` (rename helpers)

- [ ] **Step 1: Rename helpers to region-terms**

In `libs/markoff/src/SceneCoordinator.h`, rename:
- `headingSceneY(int) → regionSceneY(int)`
- `headingIndexAtSceneY(qreal) → regionIndexAtSceneY(qreal)`

Keep `headingIndexForItem` unchanged (it's still the "is this item a heading" lookup; used for v1 gutter click test compat — the new code path in Task 10 supersedes it at runtime).

Update impls in `.cpp` to iterate `m_foldingModel->regions()` instead of `headings()`.

Same `regionAtBlock` logic as before, just against regions.

- [ ] **Step 2: Update FoldGutter paint**

In `libs/markoff/src/FoldGutter.cpp`:

```cpp
void FoldGutter::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (!m_coordinator || !m_model) return;
    const auto &regions = m_model->regions();
    int x = 0;
    for (auto *col : m_columns) {
        for (int i = 0; i < regions.size(); ++i) {
            const qreal sceneY = m_coordinator->regionSceneY(i);
            if (sceneY < 0) continue;
            const QPointF topLeftLocal = mapFromScene(QPointF(0, sceneY));
            const int cellH = 22; // approx line height; tune
            const QRect cell(x, int(topLeftLocal.y()), col->width(), cellH);
            col->paintCell(p, cell, i);
        }
        x += col->width();
    }
}

void FoldGutter::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    if (!m_coordinator || !m_model) { e->ignore(); return; }
    QPoint local;
    const int ci = columnAt(e->pos().x(), &local);
    if (ci < 0) { e->ignore(); return; }
    const int regionIdx = m_coordinator->regionIndexAtSceneY(e->scenePos().y());
    if (regionIdx < 0) { e->ignore(); return; }
    const bool handled = m_columns[ci]->handleClick(local, regionIdx, e->modifiers());
    if (handled) e->accept(); else e->ignore();
}
```

- [ ] **Step 3: Run gutter tests**

```
cmake --build build --target tst_markoff_fold_gutter
ctest --test-dir build -R tst_markoff_fold_gutter --output-on-failure
```

Expected: existing tests pass (the test hook `handleMouseClickForTesting` still works; test passes `headingIndex=0` which is now `regionIndex=0` by the same mapping for heading-only test scenarios).

- [ ] **Step 4: Commit**

```
git add libs/markoff/src/FoldGutter.cpp libs/markoff/src/SceneCoordinator.h libs/markoff/src/SceneCoordinator.cpp
git commit -m "feat(markoff): FoldGutter paints per-region (headings + code blocks)"
```

---

## Task 10: FoldArrowColumn handles code-block clicks

**Files:**
- Modify: `libs/markoff/src/FoldArrowColumn.cpp`

The column receives a region index now (not a heading index). Dispatch by region type.

- [ ] **Step 1: Update FoldArrowColumn**

```cpp
void FoldArrowColumn::paintCell(QPainter *p, const QRect &rect, int idx)
{
    const auto &regions = m_model->regions();
    if (idx < 0 || idx >= regions.size()) return;
    const auto &r = regions[idx];
    const bool folded = m_model->isFolded(r.path);
    // ... existing triangle paint using `folded` ...
}

bool FoldArrowColumn::handleClick(QPoint, int idx, Qt::KeyboardModifiers mods)
{
    const auto &regions = m_model->regions();
    if (idx < 0 || idx >= regions.size()) return false;
    const auto &r = regions[idx];

    if (mods & Qt::ControlModifier) {
        if (r.type == FoldableRegion::Heading) {
            const int level = r.level;
            bool allFolded = true;
            for (const auto &h : regions) {
                if (h.type == FoldableRegion::Heading && h.level == level
                    && !m_model->isFolded(h.path)) { allFolded = false; break; }
            }
            if (allFolded) m_model->unfoldAllAtLevel(level);
            else m_model->foldAllAtLevel(level);
        } else { // CodeBlock
            const FoldRegionKey sectionPath = r.path.mid(0, r.path.size() - 1);
            bool allFolded = true;
            for (const auto &cb : regions) {
                if (cb.type != FoldableRegion::CodeBlock) continue;
                if (cb.path.size() <= 1) continue;
                if (cb.path.mid(0, cb.path.size() - 1) != sectionPath) continue;
                if (!m_model->isFolded(cb.path)) { allFolded = false; break; }
            }
            if (allFolded) m_model->unfoldAllCodeBlocksInSection(sectionPath);
            else m_model->foldAllCodeBlocksInSection(sectionPath);
        }
    } else {
        m_model->toggle(r.path);
    }
    return true;
}
```

- [ ] **Step 2: Run tests**

```
cmake --build build --target tst_markoff_fold_gutter
ctest --test-dir build -R tst_markoff_fold_gutter --output-on-failure
```

Expected: existing tests pass.

- [ ] **Step 3: Commit**

```
git add libs/markoff/src/FoldArrowColumn.cpp
git commit -m "feat(markoff): FoldArrowColumn dispatches code-block toggles"
```

---

## Task 11: Editor public API additions

**Files:**
- Modify: `libs/markoff/include/markoff/Editor.h`
- Modify: `libs/markoff/src/Editor.cpp`

- [ ] **Step 1: Declare new API**

In `libs/markoff/include/markoff/Editor.h`, add to the `// --- Folding ---` block:

```cpp
QList<QStringList> codeBlockPaths() const;
void foldAllCodeBlocks();
void unfoldAllCodeBlocks();
```

- [ ] **Step 2: Implement**

In `libs/markoff/src/Editor.cpp`:

```cpp
QList<QStringList> Editor::codeBlockPaths() const {
    QList<QStringList> out;
    for (const auto &r : m_foldingModel->codeBlockRegions())
        out.append(r.path);
    return out;
}

void Editor::foldAllCodeBlocks() { m_foldingModel->foldAllCodeBlocks(); }
void Editor::unfoldAllCodeBlocks() { m_foldingModel->unfoldAllCodeBlocks(); }
```

- [ ] **Step 3: Build**

```
cmake --build build --target markoff
```

Expected: clean build.

- [ ] **Step 4: Commit**

```
git add libs/markoff/include/markoff/Editor.h libs/markoff/src/Editor.cpp
git commit -m "feat(markoff): Editor API for code-block folding"
```

---

## Task 12: Auto-unfold for code-block find matches

**Files:**
- Modify: `libs/markoff/src/Editor.cpp`

The existing `findText` auto-unfold resolves an enclosing heading path via `enclosingHeadingPathAtBlock`. For find matches inside folded code-block items (TableBlockItem, non-MTI), we need to also handle the item-as-region case.

- [ ] **Step 1: Extend the `commitMatch` lambda**

In `libs/markoff/src/Editor.cpp`, find the `commitMatch` lambda in `findText`. Replace:

```cpp
const QStringList path = m_coordinator->enclosingHeadingPathAtBlock(idx, blockNumber);
if (!path.isEmpty()) {
    const auto expanded = m_foldingModel->unfoldAncestors(path);
    if (!expanded.isEmpty()) emit foldsAutoExpanded(expanded);
}
```

With:

```cpp
// If the matched item IS a code-block region (non-MTI), use its own path
// as the unfold target. Otherwise fall back to enclosing-heading lookup.
QStringList path;
const int regionIdx = m_coordinator->regionAtBlock(idx, blockNumber);
if (regionIdx >= 0) {
    const auto &regs = m_foldingModel->regions();
    if (regionIdx < regs.size()
        && regs[regionIdx].type == FoldableRegion::CodeBlock) {
        path = regs[regionIdx].path;
    }
}
if (path.isEmpty())
    path = m_coordinator->enclosingHeadingPathAtBlock(idx, blockNumber);
if (!path.isEmpty()) {
    const auto expanded = m_foldingModel->unfoldAncestors(path);
    if (!expanded.isEmpty()) emit foldsAutoExpanded(expanded);
}
```

- [ ] **Step 2: Commit**

```
git add libs/markoff/src/Editor.cpp
git commit -m "feat(markoff): auto-unfold code blocks on find match"
```

---

## Task 13: End-to-end code-block folding test binary

**Files:**
- Create: `libs/markoff/tests/tst_code_block_folding.cpp`
- Modify: `libs/markoff/tests/CMakeLists.txt`

- [ ] **Step 1: Write the test file**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <markoff/Editor.h>

using namespace Markoff;

class TstCodeBlockFolding : public QObject {
    Q_OBJECT
private slots:
    void codeBlockPaths_singleBlock_oneEntry();
    void codeBlockPaths_ordinalsWithinSection();
    void fold_codeBlock_emitsSignal();
    void foldAllCodeBlocks_foldsEveryCode_keepsHeadings();
    void persistence_mixedPaths_roundTrip();
    void rename_enclosingHeading_dropsCodeBlockFold();
    void auto_unfold_findText_insideFoldedCode();
};

static const QString kDoc =
    "# Intro\n\n"
    "Body.\n\n"
    "```python\n"
    "print('hello')\n"
    "```\n\n"
    "```cpp\n"
    "puts(\"hi\");\n"
    "```\n\n"
    "## Other\n\n"
    "```rust\n"
    "fn main() {}\n"
    "```\n";

static void waitReparse() { QTest::qWait(300); }

void TstCodeBlockFolding::codeBlockPaths_singleBlock_oneEntry()
{
    Editor e;
    e.setPlainText(QStringLiteral("```py\nx=1\n```\n"));
    waitReparse();
    const auto paths = e.codeBlockPaths();
    QCOMPARE(paths.size(), 1);
    QCOMPARE(paths[0], (QStringList{"code:0"}));
}

void TstCodeBlockFolding::codeBlockPaths_ordinalsWithinSection()
{
    Editor e;
    e.setPlainText(kDoc);
    waitReparse();
    const auto paths = e.codeBlockPaths();
    QVERIFY(paths.contains((QStringList{"Intro","code:0"})));
    QVERIFY(paths.contains((QStringList{"Intro","code:1"})));
    QVERIFY(paths.contains((QStringList{"Other","code:0"})));
}

void TstCodeBlockFolding::fold_codeBlock_emitsSignal()
{
    Editor e;
    e.setPlainText(kDoc);
    waitReparse();
    QSignalSpy spy(&e, &Editor::foldStateChanged);
    e.fold({"Intro","code:0"});
    QCOMPARE(spy.count(), 1);
    QVERIFY(e.isFolded({"Intro","code:0"}));
}

void TstCodeBlockFolding::foldAllCodeBlocks_foldsEveryCode_keepsHeadings()
{
    Editor e;
    e.setPlainText(kDoc);
    waitReparse();
    e.foldAllCodeBlocks();
    for (const auto &p : e.codeBlockPaths())
        QVERIFY(e.isFolded(p));
    for (const auto &hp : e.headingPaths())
        QVERIFY(!e.isFolded(hp));
}

void TstCodeBlockFolding::persistence_mixedPaths_roundTrip()
{
    Editor e;
    e.setPlainText(kDoc);
    waitReparse();
    e.fold({"Intro","code:0"});
    e.fold({"Other"});
    const auto j = e.serializeFoldState();

    Editor e2;
    e2.setPlainText(kDoc);
    waitReparse();
    e2.restoreFoldState(j);
    QVERIFY(e2.isFolded({"Intro","code:0"}));
    QVERIFY(e2.isFolded({"Other"}));
}

void TstCodeBlockFolding::rename_enclosingHeading_dropsCodeBlockFold()
{
    Editor e;
    e.setPlainText(kDoc);
    waitReparse();
    e.fold({"Intro","code:0"});

    QString renamed = kDoc;
    renamed.replace(QStringLiteral("# Intro"),
                    QStringLiteral("# Introduction"));
    e.setPlainText(renamed);
    waitReparse();

    QVERIFY(!e.isFolded({"Intro","code:0"}));
}

void TstCodeBlockFolding::auto_unfold_findText_insideFoldedCode()
{
    Editor e;
    e.setPlainText(kDoc);
    waitReparse();
    e.fold({"Intro","code:0"});
    QSignalSpy spy(&e, &Editor::foldsAutoExpanded);

    const bool found = e.findText(QStringLiteral("hello"));
    QTest::qWait(50);

    QVERIFY(found);
    QVERIFY(!e.isFolded({"Intro","code:0"}));
    QCOMPARE(spy.count(), 1);
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    TstCodeBlockFolding t;
    return QTest::qExec(&t, argc, argv);
}
#include "tst_code_block_folding.moc"
```

Register in `libs/markoff/tests/CMakeLists.txt`:

```cmake
add_executable(tst_markoff_code_block_folding tst_code_block_folding.cpp)
add_test(NAME tst_markoff_code_block_folding COMMAND tst_markoff_code_block_folding)
target_link_libraries(tst_markoff_code_block_folding PRIVATE Qt6::Test Qt6::Widgets markoff)
target_include_directories(tst_markoff_code_block_folding PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set_tests_properties(tst_markoff_code_block_folding PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: Build & run**

```
cmake --build build --target tst_markoff_code_block_folding
ctest --test-dir build -R tst_markoff_code_block_folding --output-on-failure -V 2>&1 | tail -30
```

Expected: 7 slots pass.

- [ ] **Step 3: Commit**

```
git add libs/markoff/tests/tst_code_block_folding.cpp libs/markoff/tests/CMakeLists.txt
git commit -m "test(markoff): end-to-end code-block folding suite"
```

---

## Task 14: Full suite + manual visual check

- [ ] **Step 1: Full markoff test run**

```
ctest --test-dir build -R tst_markoff --output-on-failure 2>&1 | tail -30
```

Expected: all markoff tests pass (incl. v1 heading-folding suite — 5 prior binaries + 2 new = 7 fold-related binaries, plus the usual non-fold ones).

- [ ] **Step 2: Manual visual check**

```
./build/bin/markoff-testapp libs/markoff/tests/showcase.md
```

Verify:
1. Each fenced code block has a gutter arrow at its top line.
2. Clicking an arrow folds the code block to its summary row (e.g., ` ```cpp (12 lines)`).
3. Clicking again unfolds it.
4. Ctrl+Click on one code-block arrow folds all sibling code blocks in the same section.
5. Heading folding still works exactly as v1.
6. Folding a heading with code blocks inside hides both the heading's body and the code blocks (including their summary rows).

If any step fails, open a new task to investigate.

- [ ] **Step 3: Update TODO.md**

Append to `libs/markoff/docs/TODO.md`, under "Recently fixed":

```
- Code-block folding (v2 sprint 1): fenced code blocks fold to
  `\`\`\`lang (N lines)` summary rows. Path encoding
  `["Section","code:N"]` matches heading path identity scheme;
  Ctrl+Click on code arrow folds section siblings; auto-unfold on
  find match inside folded code. Plan:
  `docs/plans/2026-04-15-code-block-folding.md`.
```

- [ ] **Step 4: Final commit**

```
git add libs/markoff/docs/TODO.md
git commit -m "docs(markoff): note code-block folding v2 sprint complete"
```

---

## Self-review checklist

Run before declaring complete:

1. **Spec coverage**
   - `FoldableRegion` data shape — Task 2, 3
   - Path encoding with ordinal reset — Task 2
   - Region detection via TreeSitterParser — Task 1, 5
   - Reconcile via regions — Task 3, 4, 6
   - TableBlockItem folded paint — Task 7
   - Visibility integration — Task 8
   - Gutter paints regions — Task 9
   - Ctrl+Click section-scoped toggle — Task 10
   - Editor API additions — Task 11
   - Auto-unfold for code matches — Task 12
   - Persistence: no schema change needed, covered by Task 13 round-trip test
   - All new test binaries registered — Task 7, 13

2. **Type consistency**
   - `FoldableRegion::Type::Heading | CodeBlock` used throughout.
   - `computeRegions()` returns `QList<FoldableRegion>`.
   - `FoldingModel::reconcile(QList<FoldableRegion>)` new signature.
   - Renames applied consistently: `headingSceneY → regionSceneY`, `headingIndexAtSceneY → regionIndexAtSceneY`, `headingAtBlock` kept as a compat wrapper over `regionAtBlock`.

3. **No placeholders**: All code blocks in every task are complete. Tasks 5 and 9 reference existing code blocks that the engineer will see during editing; annotations explicitly call out "unchanged" and "existing".

4. **Verification-before-completion**: every task ends with a `ctest` invocation that must pass before moving on.
