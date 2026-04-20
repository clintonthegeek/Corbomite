# Graph View Integration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire libforcegraph into Corbomite with a global graph tab (Ctrl+G) and local graph sidebar panel, completing Phase 3.

**Architecture:** `GraphDataBuilder` transforms SQLiteIndex link data + VaultModel note list into `ForceGraph::GraphNode`/`GraphEdge` vectors. `GraphViewTab` wraps `ForceGraphView` as an editor tab. `LocalGraphPanel` shows a mini neighborhood graph in the right sidebar. Both connect node clicks to note navigation.

**Tech Stack:** C++20, Qt6 Widgets, libforcegraph, SQLiteIndex

**Spec:** `docs/superpowers/specs/2026-03-31-graph-view-integration-design.md`

**Current state:** 19 tests passing. libforcegraph implemented with engine + view. SQLiteIndex has link queries.

---

### Task 1: SQLiteIndex::allLinks() + GraphDataBuilder

**Files:**
- Modify: `libs/storage/include/corbomite/storage/SQLiteIndex.h`
- Modify: `libs/storage/src/SQLiteIndex.cpp`
- Create: `src/graph/GraphDataBuilder.h`
- Create: `src/graph/GraphDataBuilder.cpp`
- Create: `tests/graph/CMakeLists.txt`
- Create: `tests/graph/tst_graphdatabuilder.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `CMakeLists.txt` (add test subdirectory)

- [ ] **Step 1: Add allLinks() to SQLiteIndex**

In `libs/storage/include/corbomite/storage/SQLiteIndex.h`, add to public section after `orphanLinks()`:
```cpp
    QVector<LinkInfo> allLinks() const;
```

In `libs/storage/src/SQLiteIndex.cpp`, implement:
```cpp
QVector<LinkInfo> SQLiteIndex::allLinks() const
{
    QVector<LinkInfo> results;
    if (!m_isOpen) return results;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.exec(QStringLiteral(
        "SELECT source_path, target_path, link_type, display_text FROM links"));

    while (q.next()) {
        LinkInfo info;
        info.sourcePath = q.value(0).toString();
        info.targetPath = q.value(1).toString();
        info.linkType = q.value(2).toString();
        info.displayText = q.value(3).toString();
        results.append(info);
    }
    return results;
}
```

- [ ] **Step 2: Write GraphDataBuilder tests**

`tests/graph/tst_graphdatabuilder.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include "graph/GraphDataBuilder.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/models/VaultModel.h"

class TestGraphDataBuilder : public QObject {
    Q_OBJECT

    void createFile(const QString &path, const QString &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content.toUtf8());
        f.close();
    }

private Q_SLOTS:
    void testGlobalGraphBasic()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/A.md", "Links to [[B]]");
        createFile(vault + "/B.md", "Links to [[C]]");
        createFile(vault + "/C.md", "No links");

        Corbomite::VaultModel vaultModel;
        vaultModel.open(vault);

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultModel);

        QCOMPARE(result.nodes.size(), 3);
        QCOMPARE(result.edges.size(), 2); // A→B, B→C
    }

    void testUnresolvedNodes()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/A.md", "Links to [[NonExistent]]");

        Corbomite::VaultModel vaultModel;
        vaultModel.open(vault);

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultModel);

        // A + NonExistent (unresolved)
        QCOMPARE(result.nodes.size(), 2);
        QCOMPARE(result.edges.size(), 1);

        // Find the unresolved node
        bool foundUnresolved = false;
        for (const auto &node : result.nodes) {
            if (node.id == QStringLiteral("NonExistent.md")) {
                QCOMPARE(node.color, QColor(136, 136, 136)); // Gray
                foundUnresolved = true;
            }
        }
        QVERIFY(foundUnresolved);
    }

    void testOrphanNode()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/linked.md", "Has [[other]]");
        createFile(vault + "/other.md", "Target");
        createFile(vault + "/orphan.md", "No links at all");

        Corbomite::VaultModel vaultModel;
        vaultModel.open(vault);

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultModel);

        QCOMPARE(result.nodes.size(), 3);

        // Orphan should have light gray color
        bool foundOrphan = false;
        for (const auto &node : result.nodes) {
            if (node.id == QStringLiteral("orphan.md")) {
                QCOMPARE(node.color, QColor(170, 170, 170));
                foundOrphan = true;
            }
        }
        QVERIFY(foundOrphan);
    }

    void testNodeRadiusScalesWithDegree()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/hub.md", "Links to [[A]] and [[B]] and [[C]]");
        createFile(vault + "/A.md", "Just a note");
        createFile(vault + "/B.md", "Just a note");
        createFile(vault + "/C.md", "Just a note");

        Corbomite::VaultModel vaultModel;
        vaultModel.open(vault);

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultModel);

        // Find hub node — should have larger radius than leaf nodes
        double hubRadius = 0, leafRadius = 0;
        for (const auto &node : result.nodes) {
            if (node.id == QStringLiteral("hub.md")) hubRadius = node.radius;
            if (node.id == QStringLiteral("A.md")) leafRadius = node.radius;
        }
        QVERIFY(hubRadius > leafRadius);
    }

    void testLocalGraph()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        createFile(vault + "/center.md", "Links to [[neighbor]]");
        createFile(vault + "/neighbor.md", "Links to [[far]]");
        createFile(vault + "/far.md", "Far away");
        createFile(vault + "/unrelated.md", "No connection");

        Corbomite::VaultModel vaultModel;
        vaultModel.open(vault);

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        // Depth 1: only center + direct neighbors
        auto result1 = Corbomite::GraphDataBuilder::buildLocalGraph(
            &index, &vaultModel, QStringLiteral("center.md"), 1);

        QCOMPARE(result1.nodes.size(), 2); // center + neighbor
        QCOMPARE(result1.edges.size(), 1);

        // Depth 2: center + neighbor + far
        auto result2 = Corbomite::GraphDataBuilder::buildLocalGraph(
            &index, &vaultModel, QStringLiteral("center.md"), 2);

        QCOMPARE(result2.nodes.size(), 3); // center + neighbor + far
        QCOMPARE(result2.edges.size(), 2);

        // "unrelated" should NOT be included at any depth
        for (const auto &node : result2.nodes) {
            QVERIFY(node.id != QStringLiteral("unrelated.md"));
        }
    }

    void testEmptyIndex()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        QDir().mkpath(vault);

        Corbomite::VaultModel vaultModel;
        vaultModel.open(vault);

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        auto result = Corbomite::GraphDataBuilder::buildGlobalGraph(&index, &vaultModel);
        QCOMPARE(result.nodes.size(), 0);
        QCOMPARE(result.edges.size(), 0);
    }
};

QTEST_MAIN(TestGraphDataBuilder)
#include "tst_graphdatabuilder.moc"
```

- [ ] **Step 3: Implement GraphDataBuilder**

`src/graph/GraphDataBuilder.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QVector>
#include <forcegraph/GraphTypes.h>

namespace Corbomite {

class SQLiteIndex;
class VaultModel;

class GraphDataBuilder {
public:
    struct Result {
        QVector<ForceGraph::GraphNode> nodes;
        QVector<ForceGraph::GraphEdge> edges;
    };

    static Result buildGlobalGraph(SQLiteIndex *index, VaultModel *vault);
    static Result buildLocalGraph(SQLiteIndex *index, VaultModel *vault,
                                   const QString &centerNotePath, int depth = 2);
};

} // namespace Corbomite
```

`src/graph/GraphDataBuilder.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphDataBuilder.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/core/NoteMeta.h"

#include <QSet>
#include <QQueue>
#include <cmath>

namespace Corbomite {

GraphDataBuilder::Result GraphDataBuilder::buildGlobalGraph(SQLiteIndex *index, VaultModel *vault)
{
    Result result;
    if (!index || !vault) return result;

    // Collect all note paths and their degrees
    QHash<QString, int> degree; // path → connection count
    auto allNotes = vault->allNotes();
    QSet<QString> existingPaths;

    for (const auto &meta : allNotes) {
        existingPaths.insert(meta.relativePath);
        degree[meta.relativePath] = 0;
    }

    // Get all links and count degrees
    auto allLinks = index->allLinks();
    QSet<QString> unresolvedPaths;

    for (const auto &link : allLinks) {
        if (link.linkType == QStringLiteral("embed")) continue; // Skip embeds for graph

        degree[link.sourcePath]++;
        degree[link.targetPath]++;

        if (!existingPaths.contains(link.targetPath)) {
            unresolvedPaths.insert(link.targetPath);
        }
    }

    // Build nodes
    for (const auto &meta : allNotes) {
        ForceGraph::GraphNode node;
        node.id = meta.relativePath;
        node.label = meta.nameFromPath();

        int deg = degree.value(meta.relativePath, 0);
        node.radius = 4.0 + std::log(1.0 + deg) * 3.0;

        if (deg == 0) {
            node.color = QColor(170, 170, 170); // Orphan — light gray
        } else {
            node.color = QColor(123, 108, 217); // Regular — purple
        }

        result.nodes.append(node);
    }

    // Add unresolved nodes
    for (const auto &path : unresolvedPaths) {
        ForceGraph::GraphNode node;
        node.id = path;
        QString name = path;
        name = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
        node.label = name;
        node.radius = 3.0;
        node.color = QColor(136, 136, 136); // Unresolved — gray
        result.nodes.append(node);
    }

    // Build edges (skip embeds)
    for (const auto &link : allLinks) {
        if (link.linkType == QStringLiteral("embed")) continue;

        ForceGraph::GraphEdge edge;
        edge.sourceId = link.sourcePath;
        edge.targetId = link.targetPath;
        result.edges.append(edge);
    }

    return result;
}

GraphDataBuilder::Result GraphDataBuilder::buildLocalGraph(SQLiteIndex *index, VaultModel *vault,
                                                            const QString &centerNotePath, int depth)
{
    Result result;
    if (!index || !vault || centerNotePath.isEmpty()) return result;

    // BFS from center to collect nodes within N hops
    QSet<QString> collected;
    QQueue<QPair<QString, int>> queue; // (path, current_depth)
    queue.enqueue({centerNotePath, 0});
    collected.insert(centerNotePath);

    while (!queue.isEmpty()) {
        auto [path, d] = queue.dequeue();
        if (d >= depth) continue;

        // Outlinks
        auto outlinks = index->outlinksFor(path);
        for (const auto &link : outlinks) {
            if (link.linkType == QStringLiteral("embed")) continue;
            if (!collected.contains(link.targetPath)) {
                collected.insert(link.targetPath);
                queue.enqueue({link.targetPath, d + 1});
            }
        }

        // Backlinks
        auto backlinks = index->backlinksFor(path);
        for (const auto &link : backlinks) {
            if (!collected.contains(link.sourcePath)) {
                collected.insert(link.sourcePath);
                queue.enqueue({link.sourcePath, d + 1});
            }
        }
    }

    // Build nodes for collected paths
    QHash<QString, int> degree;
    for (const auto &path : collected) degree[path] = 0;

    // Get edges between collected nodes
    auto allLinks = index->allLinks();
    for (const auto &link : allLinks) {
        if (link.linkType == QStringLiteral("embed")) continue;
        if (collected.contains(link.sourcePath) && collected.contains(link.targetPath)) {
            ForceGraph::GraphEdge edge;
            edge.sourceId = link.sourcePath;
            edge.targetId = link.targetPath;
            result.edges.append(edge);

            degree[link.sourcePath]++;
            degree[link.targetPath]++;
        }
    }

    // Create nodes
    for (const auto &path : collected) {
        ForceGraph::GraphNode node;
        node.id = path;

        QString name = path;
        name = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
        node.label = name;

        int deg = degree.value(path, 0);
        node.radius = 4.0 + std::log(1.0 + deg) * 3.0;

        if (path == centerNotePath) {
            node.color = QColor(86, 182, 194);  // Center — teal, slightly larger
            node.radius += 3.0;
        } else if (vault->noteExists(path)) {
            node.color = QColor(123, 108, 217);  // Regular — purple
        } else {
            node.color = QColor(136, 136, 136);  // Unresolved — gray
        }

        result.nodes.append(node);
    }

    return result;
}

} // namespace Corbomite
```

- [ ] **Step 4: Create test and build CMake**

`tests/graph/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(Corbomite_GraphTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets Sql)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_graphdatabuilder tst_graphdatabuilder.cpp)
add_test(NAME tst_graphdatabuilder COMMAND tst_graphdatabuilder)
target_link_libraries(tst_graphdatabuilder PRIVATE
    Qt6::Test CorbomiteApp)
set_tests_properties(tst_graphdatabuilder PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

Add to root `CMakeLists.txt`: `add_subdirectory(tests/graph)`

Add `graph/GraphDataBuilder.cpp` to `src/CMakeLists.txt` source list. Add `${CMAKE_CURRENT_SOURCE_DIR}/graph` to include directories.

- [ ] **Step 5: Build and run tests**

```bash
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R tst_graphdatabuilder --output-on-failure
```

Expected: All tests pass.

- [ ] **Step 6: Commit**

```bash
git add libs/storage/ src/graph/ tests/graph/ src/CMakeLists.txt CMakeLists.txt
git commit -m "feat: add GraphDataBuilder + SQLiteIndex::allLinks() for graph data

GraphDataBuilder transforms vault notes + link index into ForceGraph
node/edge data. Supports global (full vault) and local (N-hop BFS)
graph building. Node radius scales with degree. Unresolved/orphan
nodes get distinct colors. 6 unit tests."
```

---

### Task 2: GraphViewTab + EditorViewSpace Integration

**Files:**
- Create: `src/graph/GraphViewTab.h`
- Create: `src/graph/GraphViewTab.cpp`
- Modify: `src/editor/EditorViewSpace.h`
- Modify: `src/editor/EditorViewSpace.cpp`
- Modify: `src/editor/EditorViewManager.h`
- Modify: `src/editor/EditorViewManager.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Implement GraphViewTab**

`src/graph/GraphViewTab.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace ForceGraph {
class ForceLayoutEngine;
class ForceGraphView;
}

namespace Corbomite {

class SQLiteIndex;
class VaultModel;

class GraphViewTab : public QWidget {
    Q_OBJECT

public:
    explicit GraphViewTab(SQLiteIndex *index, VaultModel *vault, QWidget *parent = nullptr);
    ~GraphViewTab() override;

    void buildGraph();

Q_SIGNALS:
    void noteActivated(const QString &relativePath);

private:
    ForceGraph::ForceGraphView *m_graphView;
    ForceGraph::ForceLayoutEngine *m_engine;
    SQLiteIndex *m_index;
    VaultModel *m_vault;
    // Future: add filter controls panel overlay
};

} // namespace Corbomite
```

`src/graph/GraphViewTab.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphViewTab.h"
#include "GraphDataBuilder.h"

#include <forcegraph/ForceLayoutEngine.h>
#include <forcegraph/ForceGraphView.h>

#include <QVBoxLayout>

namespace Corbomite {

GraphViewTab::GraphViewTab(SQLiteIndex *index, VaultModel *vault, QWidget *parent)
    : QWidget(parent)
    , m_index(index)
    , m_vault(vault)
{
    m_engine = new ForceGraph::ForceLayoutEngine(this);
    m_graphView = new ForceGraph::ForceGraphView(this);
    m_graphView->setEngine(m_engine);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_graphView);

    connect(m_graphView, &ForceGraph::ForceGraphView::nodeClicked,
            this, [this](const QString &id) {
        Q_EMIT noteActivated(id);
    });
    connect(m_graphView, &ForceGraph::ForceGraphView::nodeDoubleClicked,
            this, [this](const QString &id) {
        Q_EMIT noteActivated(id);
    });

    buildGraph();
}

GraphViewTab::~GraphViewTab()
{
    m_engine->stop();
}

void GraphViewTab::buildGraph()
{
    auto data = GraphDataBuilder::buildGlobalGraph(m_index, m_vault);
    m_graphView->setNodes(data.nodes);
    m_graphView->setEdges(data.edges);
    m_engine->start();

    // Zoom to fit after layout settles
    connect(m_engine, &ForceGraph::ForceLayoutEngine::simulationStable,
            this, [this]() {
        m_graphView->zoomToFit();
    }, Qt::SingleShotConnection);
}

} // namespace Corbomite
```

- [ ] **Step 2: Add openGraphView() to EditorViewSpace**

In `src/editor/EditorViewSpace.h`, add forward declaration and method:
```cpp
class GraphViewTab;  // forward declare in Corbomite namespace

// In public section:
    void openGraphView(SQLiteIndex *index, VaultModel *vault);
    bool hasGraphView() const;
```

In `src/editor/EditorViewSpace.cpp`, add `#include "graph/GraphViewTab.h"` and implement:

```cpp
void EditorViewSpace::openGraphView(SQLiteIndex *index, VaultModel *vault)
{
    // Only one graph tab allowed
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toString() == QStringLiteral("__graph__")) {
            m_tabBar->setCurrentIndex(i);
            return;
        }
    }

    auto *graphTab = new GraphViewTab(index, vault, m_stack);
    int stackIdx = m_stack->addWidget(graphTab);
    int tabIdx = m_tabBar->addTab(QIcon::fromTheme(QStringLiteral("preferences-system-network")),
                                   QStringLiteral("Graph View"));
    m_tabBar->setTabData(tabIdx, QStringLiteral("__graph__"));
    m_tabBar->setCurrentIndex(tabIdx);

    connect(graphTab, &GraphViewTab::noteActivated,
            this, [this](const QString &path) {
        // Find parent MainWindow and call onNoteActivated
        // Emit a signal that MainWindow connects to
        Q_EMIT activeEditorChanged(nullptr); // Signal navigation
    });
}

bool EditorViewSpace::hasGraphView() const
{
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toString() == QStringLiteral("__graph__"))
            return true;
    }
    return false;
}
```

Wait — `EditorViewSpace` currently emits `activeEditorChanged` but for graph navigation we need `noteActivated` from the GraphViewTab to reach MainWindow. Let me simplify: connect the GraphViewTab's `noteActivated` signal directly to the EditorViewSpace, which forwards it.

Add a signal to `EditorViewSpace`:
```cpp
Q_SIGNALS:
    void activeEditorChanged(NoteEditorWidget *editor);
    void cursorInfoChanged(int line, int column, int wordCount);
    void graphNoteActivated(const QString &relativePath);  // New
```

And in `openGraphView()`:
```cpp
    connect(graphTab, &GraphViewTab::noteActivated,
            this, &EditorViewSpace::graphNoteActivated);
```

Then in `EditorViewManager`, forward this signal:
```cpp
// In header, add signal:
    void graphNoteActivated(const QString &relativePath);

// In constructor, connect:
    connect(m_viewSpace, &EditorViewSpace::graphNoteActivated,
            this, &EditorViewManager::graphNoteActivated);
```

Also add forwarding methods to `EditorViewManager`:
```cpp
    void openGraphView(SQLiteIndex *index, VaultModel *vault);
    bool hasGraphView() const;

// Implementation:
void EditorViewManager::openGraphView(SQLiteIndex *index, VaultModel *vault) {
    m_viewSpace->openGraphView(index, vault);
}
bool EditorViewManager::hasGraphView() const { return m_viewSpace->hasGraphView(); }
```

- [ ] **Step 3: Add GraphViewTab.cpp + GraphDataBuilder.cpp to src/CMakeLists.txt**

Add `graph/GraphViewTab.cpp` to CorbomiteApp sources (GraphDataBuilder.cpp was added in Task 1).

- [ ] **Step 4: Build**

```bash
cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add src/graph/GraphViewTab.h src/graph/GraphViewTab.cpp \
        src/editor/EditorViewSpace.h src/editor/EditorViewSpace.cpp \
        src/editor/EditorViewManager.h src/editor/EditorViewManager.cpp \
        src/CMakeLists.txt
git commit -m "feat: add GraphViewTab as special editor tab with graph view

Opens global force-directed graph in the editor area. Single graph
tab at a time. Node clicks emit noteActivated for navigation.
Zooms to fit after layout stabilizes."
```

---

### Task 3: LocalGraphPanel + MainWindow Wiring

**Files:**
- Create: `src/graph/LocalGraphPanel.h`
- Create: `src/graph/LocalGraphPanel.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `src/app/corbomiteui.rc.in`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Implement LocalGraphPanel**

`src/graph/LocalGraphPanel.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace ForceGraph {
class ForceLayoutEngine;
class ForceGraphView;
}

namespace Corbomite {

class NoteDocument;
class SQLiteIndex;
class VaultModel;

class LocalGraphPanel : public QWidget {
    Q_OBJECT

public:
    explicit LocalGraphPanel(QWidget *parent = nullptr);

    void setIndex(SQLiteIndex *index);
    void setVaultModel(VaultModel *vault);
    void setCurrentNote(NoteDocument *doc);

Q_SIGNALS:
    void noteActivated(const QString &relativePath);

private:
    void refresh();

    ForceGraph::ForceGraphView *m_graphView;
    ForceGraph::ForceLayoutEngine *m_engine;
    SQLiteIndex *m_index = nullptr;
    VaultModel *m_vault = nullptr;
    NoteDocument *m_currentDoc = nullptr;
};

} // namespace Corbomite
```

`src/graph/LocalGraphPanel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "LocalGraphPanel.h"
#include "GraphDataBuilder.h"
#include "corbomite/core/NoteDocument.h"

#include <forcegraph/ForceLayoutEngine.h>
#include <forcegraph/ForceGraphView.h>

#include <QVBoxLayout>

namespace Corbomite {

LocalGraphPanel::LocalGraphPanel(QWidget *parent)
    : QWidget(parent)
{
    m_engine = new ForceGraph::ForceLayoutEngine(this);
    m_graphView = new ForceGraph::ForceGraphView(this);
    m_graphView->setEngine(m_engine);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_graphView);

    connect(m_graphView, &ForceGraph::ForceGraphView::nodeClicked,
            this, [this](const QString &id) {
        Q_EMIT noteActivated(id);
    });
}

void LocalGraphPanel::setIndex(SQLiteIndex *index)
{
    m_index = index;
    refresh();
}

void LocalGraphPanel::setVaultModel(VaultModel *vault)
{
    m_vault = vault;
}

void LocalGraphPanel::setCurrentNote(NoteDocument *doc)
{
    m_currentDoc = doc;
    refresh();
}

void LocalGraphPanel::refresh()
{
    m_engine->stop();

    if (!m_index || !m_vault || !m_currentDoc) {
        m_engine->clear();
        return;
    }

    auto data = GraphDataBuilder::buildLocalGraph(
        m_index, m_vault, m_currentDoc->relativePath(), 2);

    m_graphView->setNodes(data.nodes);
    m_graphView->setEdges(data.edges);

    if (!data.nodes.isEmpty()) {
        m_engine->start();
        // Highlight center note
        m_graphView->setHighlightedNode(m_currentDoc->relativePath());

        connect(m_engine, &ForceGraph::ForceLayoutEngine::simulationStable,
                this, [this]() {
            m_graphView->zoomToFit();
        }, Qt::SingleShotConnection);
    }
}

} // namespace Corbomite
```

- [ ] **Step 2: Wire into MainWindow**

Add to `MainWindow.h`:
```cpp
class LocalGraphPanel;  // forward declaration

// Private member:
    LocalGraphPanel *m_localGraphPanel = nullptr;

// Private method:
    void openGraphView();
```

Add includes to `MainWindow.cpp`:
```cpp
#include "graph/LocalGraphPanel.h"
#include "graph/GraphViewTab.h"
```

In `setupActions()`, add:
```cpp
    auto *graphView = ac->addAction(QStringLiteral("graph_view"));
    graphView->setText(i18n("Graph View"));
    graphView->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-network")));
    ac->setDefaultShortcut(graphView, QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(graphView, &QAction::triggered, this, &MainWindow::openGraphView);
```

In `setupSidebars()`, after the outline panel, add:
```cpp
    // Right sidebar: Local Graph
    auto *localGraphView = createToolView(
        nullptr,
        QStringLiteral("local_graph_panel"),
        KMultiTabBar::Right,
        QIcon::fromTheme(QStringLiteral("preferences-system-network")),
        i18n("Local Graph")
    );
    m_localGraphPanel = new LocalGraphPanel(localGraphView);
    localGraphView->layout()->addWidget(m_localGraphPanel);
    connect(m_localGraphPanel, &LocalGraphPanel::noteActivated,
            this, &MainWindow::onNoteActivated);
```

In `onVaultOpened()`, after setting index on other panels:
```cpp
    m_localGraphPanel->setIndex(m_searchIndex);
    m_localGraphPanel->setVaultModel(vault);
```

In the `activeEditorChanged` connection that updates panels, add:
```cpp
        m_localGraphPanel->setCurrentNote(editor->noteDocument());
```
And for the null case:
```cpp
        m_localGraphPanel->setCurrentNote(nullptr);
```

In `onVaultClosed()`:
```cpp
    m_localGraphPanel->setIndex(nullptr);
    m_localGraphPanel->setVaultModel(nullptr);
    m_localGraphPanel->setCurrentNote(nullptr);
```

Implement `openGraphView()`:
```cpp
void MainWindow::openGraphView()
{
    if (!m_vaultService->isOpen() || !m_searchIndex) return;
    m_editorManager->openGraphView(m_searchIndex, m_vaultService->vault());
}
```

Connect `graphNoteActivated` from EditorViewManager:
In `onVaultOpened()`, add:
```cpp
    connect(m_editorManager, &EditorViewManager::graphNoteActivated,
            this, &MainWindow::onNoteActivated, Qt::UniqueConnection);
```

- [ ] **Step 3: Update XMLGUI**

Replace `corbomiteui.rc.in` — bump version to 4, add `graph_view` to Go menu and toolbar:

```xml
<!DOCTYPE gui SYSTEM "kpartgui.dtd">
<gui name="@CORBOMITE_COMPONENT_NAME@" version="4">
  <MenuBar>
    <Menu name="file">
      <text>&amp;File</text>
      <Action name="file_open_vault"/>
      <Separator/>
      <Action name="file_new_note"/>
      <Separator/>
      <Action name="file_save"/>
      <Separator/>
    </Menu>
    <Menu name="go">
      <text>&amp;Go</text>
      <Action name="quick_switcher"/>
      <Action name="command_palette"/>
      <Separator/>
      <Action name="search_vault"/>
      <Action name="graph_view"/>
    </Menu>
    <Menu name="view">
      <text>&amp;View</text>
      <Action name="editor_toggle_mode"/>
      <Separator/>
      <Action name="view_toggle_left_sidebar"/>
      <Separator/>
      <Action name="view_zoom_in"/>
      <Action name="view_zoom_out"/>
      <Action name="view_zoom_reset"/>
    </Menu>
  </MenuBar>
  <ToolBar name="mainToolBar" noMerge="1">
    <text>Main Toolbar</text>
    <Action name="file_open_vault"/>
    <Action name="file_new_note"/>
    <Action name="file_save"/>
    <Separator/>
    <Action name="quick_switcher"/>
    <Action name="command_palette"/>
    <Action name="search_vault"/>
    <Action name="graph_view"/>
    <Separator/>
    <Action name="editor_toggle_mode"/>
  </ToolBar>
</gui>
```

- [ ] **Step 4: Add LocalGraphPanel.cpp to src/CMakeLists.txt**

Add `graph/LocalGraphPanel.cpp` to CorbomiteApp sources.

- [ ] **Step 5: Build and verify**

```bash
cmake --build build && cd build && ctest --output-on-failure
```

All tests should pass.

- [ ] **Step 6: Commit**

```bash
git add src/graph/LocalGraphPanel.h src/graph/LocalGraphPanel.cpp \
        src/app/MainWindow.h src/app/MainWindow.cpp \
        src/app/corbomiteui.rc.in src/CMakeLists.txt
git commit -m "feat: add graph view (Ctrl+G) and local graph sidebar panel

Global graph: force-directed vault visualization as editor tab.
Local graph: 2-hop neighborhood of current note in right sidebar.
Both use libforcegraph engine + view. Node clicks navigate to notes.
XMLGUI version 4 with graph_view action in Go menu + toolbar."
```

---

Self-review:

1. **Spec coverage:** GraphDataBuilder (global + local) ✓ (6 tests). Unresolved nodes ✓. Orphan nodes ✓. Node radius scaling ✓. GraphViewTab as editor tab ✓. One graph tab max ✓. Node click → note navigation ✓. LocalGraphPanel in right sidebar ✓. BFS depth parameter ✓. Ctrl+G shortcut ✓. XMLGUI v4 ✓. Active note update for local graph ✓.

2. **Placeholder scan:** All code complete. No TBDs. Future items in comments.

3. **Type consistency:** `GraphDataBuilder::Result` has `nodes`/`edges` matching `ForceGraph::GraphNode`/`GraphEdge`. `ForceGraphView::nodeClicked(QString)` matches `noteActivated(QString)`. `LocalGraphPanel` follows same setter pattern as BacklinksPanel/OutlinksPanel. `EditorViewSpace::openGraphView` uses `__graph__` sentinel in tab data.
