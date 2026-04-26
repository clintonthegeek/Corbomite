# Workspace Serializer Consolidation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace today's two divergent `workspace.json` writers (live `Workspace::serialize` flat shape; dead-but-tested `WorkspaceSerializer::toJson` flat shape) with one hybrid writer that walks KDDW for tree topology and `Workspace` for leaf payload. Closes punch-list P1 #1, #2, #3.

**Architecture:** `Corbomite::WorkspaceSerializer::{toJson,fromJson}` is promoted to canonical. `Workspace::{serialize,deserialize}` become thin forwarders. `toJson` walks `KDDockWidgets::LayoutSaver::serializeLayout()` JSON for the recursive split tree + per-group current-tab + floating-window topology, then joins with `Workspace::findLeafById()` lookups for per-leaf state. Existing 9-fixture test contract preserved with `workspace=nullptr` (placeholder leaves); new tests F10–F14 cover the production `workspace=this` path.

**Tech Stack:** Qt6, KDDockWidgets 2.4 (`KDAB::kddockwidgets`), QJsonObject/QJsonDocument, QtTest.

**Spec:** [`docs/superpowers/specs/2026-04-26-workspace-serializer-consolidation-design.md`](../specs/2026-04-26-workspace-serializer-consolidation-design.md)

---

## File touch list (decomposition)

**Modified:**
- `libs/core/src/WorkspaceSerializer.h` — clarify `workspace` semantics; remove "deferred to later phase" wording.
- `libs/core/src/WorkspaceSerializer.cpp` — replace `walkKddwTreeSimple` with recursive `walkLayoutSaverTree` driven by `LayoutSaver::serializeLayout()` JSON; wire `Workspace*` into `materializeTabs` so it drives `Workspace::createLeafInGroupOf` instead of raw `DockWidget` construction; extend floating-window walker symmetrically; keep `leafSidecar` and `stackedSidecar` only as the `workspace=nullptr` (test-only) fallback.
- `libs/core/src/Workspace.cpp` — `serialize()` and `deserialize()` reduced to forwarders; the post-load defer/active-leaf logic stays in place.
- `libs/core/include/corbomite/core/WorkspaceLeaf.h` + `libs/core/src/WorkspaceLeaf.cpp` — add `m_unknownLeafKeys` (`QJsonObject`, accessor pair), and `m_stacked` (`bool`, accessor pair, only meaningful for the first leaf in a tab group).
- `libs/core/include/corbomite/core/Workspace.h` — update stale comment claiming KDDW has no public Group enumeration API (lines 267–271).
- `tests/core/tst_workspace_serializer.cpp` — add F10 through F14.
- `tests/core/fixtures/workspace-obsidian/` — add fixtures `10-roundtrip-with-leafstate.json`, `11-per-group-currenttab.json`, `12-nested-with-state.json`, `13-popout-nested-split.json`, `14-defer-set.json`.
- `docs/punch-list.md` — mark P1 #1, #2, #3 done; add follow-up entries from the spec's "Deferred follow-ups" table.
- `docs/PROJECT-STATE.md` — refresh "Current focus" to reflect closure (1–2 lines).
- `docs/decisions-archive.md` — append closeout under a new dated H2.

**Created:**
- `docs/superpowers/specs/2026-04-26-kddw-layoutsaver-shape.md` — research artifact: dumped LayoutSaver JSON shape for fixture 03, with field-name annotations.
- `docs/obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md` — audit correction.

**Untouched (call-site stable):**
- `src/app/SessionManager.{h,cpp}` — no API change.
- `src/app/MainWindow.cpp:809-831, 2130-2160` — call sites unchanged.
- All `WorkspaceLeaf` consumers — `WorkspaceLeaf::serialize/deserialize` semantics preserved.

---

## Task 1: Discover KDDW LayoutSaver JSON schema (research)

**Files:**
- Modify (temporary probe): `tests/core/tst_workspace_serializer.cpp`
- Create: `docs/superpowers/specs/2026-04-26-kddw-layoutsaver-shape.md`

This is a one-time probe. The KDDW package on this host is `kddockwidgets 2.4.0-2`; the JSON schema returned by `LayoutSaver::serializeLayout()` is not documented in the public headers, only its existence (`LayoutSaver.h:84`). We need the field names before we can write the parser in Task 2.

- [ ] **Step 1: Add probe test method**

Append the following to `tests/core/tst_workspace_serializer.cpp` after `fixture09_orphanLeaf_reHomedToRoot` and before `QTEST_MAIN`. Also add the slot declaration `void probe_layoutsaver_shape();` to the `private slots:` block.

```cpp
#include <kddockwidgets/LayoutSaver.h>
#include <QFileInfo>

void TestWorkspaceSerializer::probe_layoutsaver_shape()
{
    auto json = readFixture(QStringLiteral("03-nested-splits.json"));
    QVERIFY(!json.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-probe"), KDDockWidgets::MainWindowOption_None);
    mainWindow->show();

    Corbomite::WorkspaceSerializer::fromJson(json, mainWindow.get(), nullptr);

    KDDockWidgets::LayoutSaver saver;
    QByteArray dumped = saver.serializeLayout();

    QFile out(QStringLiteral("/tmp/corbomite-kddw-shape-fixture03.json"));
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(dumped);
    out.close();
    qDebug() << "KDDW LayoutSaver shape written to" << QFileInfo(out).absoluteFilePath();
}
```

- [ ] **Step 2: Build and run the probe**

Run:
```bash
cmake --build build -j 10 --target tst_workspace_serializer
QT_QPA_PLATFORM=offscreen ./build/tests/core/tst_workspace_serializer -platform offscreen probe_layoutsaver_shape
```

Expected: PASS, with debug line "KDDW LayoutSaver shape written to /tmp/corbomite-kddw-shape-fixture03.json".

If the test fails because `mainWindow->show()` segfaults under offscreen, fall back to `QT_QPA_PLATFORM=minimal`. If still failing, run without `-platform offscreen` (uses the user's Wayland session).

- [ ] **Step 3: Capture the schema**

Read `/tmp/corbomite-kddw-shape-fixture03.json` and write `docs/superpowers/specs/2026-04-26-kddw-layoutsaver-shape.md` with:
- The pretty-printed JSON output
- A short annotation listing: which key holds the recursive split tree, what node-type discriminator is used (e.g. `isContainer`, `objectName`), where dock widget unique names appear, where group/frame info appears, where floating-window geometry appears.

This document is the schema reference for Task 2's parser.

- [ ] **Step 4: Revert the probe**

```bash
git checkout -- tests/core/tst_workspace_serializer.cpp
```

The probe is research scaffolding, not part of the final test suite.

- [ ] **Step 5: Commit research artifact**

```bash
git add docs/superpowers/specs/2026-04-26-kddw-layoutsaver-shape.md
git commit -m "spec: dump KDDW LayoutSaver JSON shape for fixture 03

Reference for the upcoming WorkspaceSerializer recursive walker; KDDW 2.4
does not document its layout JSON schema in headers, so this artifact is
the source of truth for parser field names.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: Replace `walkKddwTreeSimple` with recursive walker

**Files:**
- Modify: `libs/core/src/WorkspaceSerializer.cpp` (`walkKddwTreeSimple` → `walkLayoutSaverTree`)
- Modify: `tests/core/tst_workspace_serializer.cpp` (existing fixture03 round-trip assertion)

This task fixes punch-list P1 #2 (nested-split round-trip) on the **write side**. The `fromJson` materialize path already constructs nested splits correctly; only the walk-back is broken.

- [ ] **Step 1: Strengthen the existing fixture03 test to assert round-trip shape**

The current `fixture03_nestedSplits_threeDockWidgetsInCorrectGroups` only asserts `dockwidgets().size() == 3` and `groups().size() == 3` after `fromJson`. Strengthen it to also call `toJson` and assert the output preserves the nested-split structure (one `split` containing one `tabs` and one `split`, the inner `split` containing two `tabs`).

Replace the test body at `tests/core/tst_workspace_serializer.cpp:126-143` with:

```cpp
void TestWorkspaceSerializer::fixture03_nestedSplits_threeDockWidgetsInCorrectGroups()
{
    auto json = readFixture(QStringLiteral("03-nested-splits.json"));
    QVERIFY(!json.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f03"), KDDockWidgets::MainWindowOption_None);
    mainWindow->show();

    Corbomite::WorkspaceSerializer::fromJson(json, mainWindow.get(), nullptr);

    auto *registry = KDDockWidgets::DockRegistry::self();
    QCOMPARE(registry->dockwidgets().size(), 3);
    QVERIFY(registry->dockByName(QStringLiteral("leaf01aaaaaaaaaa")));
    QVERIFY(registry->dockByName(QStringLiteral("leaf02aaaaaaaaaa")));
    QVERIFY(registry->dockByName(QStringLiteral("leaf03aaaaaaaaaa")));
    QCOMPARE(registry->groups().size(), 3);

    // Round-trip shape: outer split has two children — one tabs (with leaf01)
    // and one inner split (with two tabs holding leaf02 and leaf03).
    auto out = Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr);
    auto outerChildren = out.value(QStringLiteral("main")).toObject()
                            .value(QStringLiteral("children")).toArray();
    QCOMPARE(outerChildren.size(), 2);

    bool sawOuterTabs = false;
    bool sawInnerSplit = false;
    for (const auto &v : outerChildren) {
        auto obj = v.toObject();
        auto type = obj.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("tabs"))
            sawOuterTabs = true;
        if (type == QStringLiteral("split")) {
            sawInnerSplit = true;
            auto innerChildren = obj.value(QStringLiteral("children")).toArray();
            QCOMPARE(innerChildren.size(), 2);
            for (const auto &vv : innerChildren) {
                QCOMPARE(vv.toObject().value(QStringLiteral("type")).toString(),
                         QStringLiteral("tabs"));
            }
        }
    }
    QVERIFY(sawOuterTabs);
    QVERIFY(sawInnerSplit);
}
```

- [ ] **Step 2: Run the strengthened test, confirm it fails**

```bash
cmake --build build -j 10 --target tst_workspace_serializer
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: FAIL on `outerChildren.size() == 2`. Today's `walkKddwTreeSimple` flattens to one tabs node containing all three dock widgets — so `outerChildren.size() == 1` and `sawInnerSplit` is false.

- [ ] **Step 3: Implement the recursive walker**

In `libs/core/src/WorkspaceSerializer.cpp`, replace `walkKddwTreeSimple` (lines 196-240) with a new `walkLayoutSaverTree` function. The implementation parses the LayoutSaver JSON output (per the schema documented in Task 1) and emits a `SplitNode`/`TabsNode` tree. Skeleton:

```cpp
// Parse LayoutSaver JSON and build a SplitNode tree. Field names below
// reference docs/superpowers/specs/2026-04-26-kddw-layoutsaver-shape.md.
SplitNode walkLayoutSaverTree(KDDockWidgets::QtWidgets::MainWindow *main,
                              Workspace *workspace)
{
    KDDockWidgets::LayoutSaver saver;
    QByteArray bytes = saver.serializeLayout();
    QJsonDocument doc = QJsonDocument::fromJson(bytes);
    QJsonObject root = doc.object();

    // The path from `root` to the main-area split tree depends on KDDW's
    // schema (Task 1 output). Typical shape is:
    //   root["mainWindows"][<our affinity>]["multiSplitterLayout"]["root"]
    // with recursive children under the "children" or "items" array, each
    // child carrying either nested-container metadata (split direction,
    // children) or a frame metadata block (group of dock widgets).
    //
    // Implementer: replace the path traversal below with the literal keys
    // from kddw-layoutsaver-shape.md. The recursion structure is:
    //   - Container node => SplitNode { direction: "horizontal"|"vertical",
    //                                   children: [recurse on each child] }
    //   - Group node     => TabsNode  { id, currentTab, children: [LeafNode] }
    //   - Leaf in group  => LeafNode  { id: dw.uniqueName }, then enrich with
    //                                  Workspace::findLeafById(stripVaultPrefix(uniqueName))
    //                                  ->serialize() when workspace != nullptr.

    SplitNode out;
    out.direction = QStringLiteral("vertical");

    QJsonObject mainArea = locateMainArea(root, main);   // see helper below
    if (mainArea.isEmpty()) return out;
    walkContainer(mainArea, out, main, workspace);
    return out;
}

// Recursive helper: read one container/frame node from the LayoutSaver JSON
// and append its translated child to `parent`. When the node is a frame
// (group), emit a TabsNode; when it's a container with children, recurse
// into a fresh SplitNode and append it to parent.splitChildren.
void walkContainer(const QJsonObject &node, SplitNode &parent,
                   KDDockWidgets::QtWidgets::MainWindow *main,
                   Workspace *workspace);
```

Two helpers fill in field names against the schema:

```cpp
QJsonObject locateMainArea(const QJsonObject &root,
                           KDDockWidgets::QtWidgets::MainWindow *main)
{
    // Locate the entry for our specific MainWindow (by uniqueName /
    // affinity / objectName per the schema doc) and return its
    // multiSplitter root subtree.
    // FIELDS_FROM_SCHEMA: replace key names with those documented
    // in 2026-04-26-kddw-layoutsaver-shape.md.
    const auto mws = root.value(QStringLiteral("mainWindows")).toArray();
    for (const auto &v : mws) {
        auto obj = v.toObject();
        if (obj.value(QStringLiteral("uniqueName")).toString()
            == main->uniqueName()) {
            return obj.value(QStringLiteral("multiSplitterLayout")).toObject()
                      .value(QStringLiteral("root")).toObject();
        }
    }
    return {};
}

void walkContainer(const QJsonObject &node, SplitNode &parent,
                   KDDockWidgets::QtWidgets::MainWindow *main,
                   Workspace *workspace)
{
    // FIELDS_FROM_SCHEMA: discriminator below depends on whether KDDW
    // marks frames vs splits with `isContainer`, `type`, or `objectName`.
    if (node.value(QStringLiteral("isContainer")).toBool()) {
        SplitNode child;
        child.direction =
            node.value(QStringLiteral("orientation")).toInt() == 1   // Qt::Vertical
            ? QStringLiteral("vertical") : QStringLiteral("horizontal");
        const auto children = node.value(QStringLiteral("children")).toArray();
        for (const auto &v : children)
            walkContainer(v.toObject(), child, main, workspace);
        parent.splitChildren.append(child);
    } else {
        // Frame/group node — emit a TabsNode.
        TabsNode tabs;
        tabs.currentTab = node.value(QStringLiteral("currentTabIndex")).toInt(0);
        const auto frameDws = node.value(QStringLiteral("dockWidgets")).toArray();
        for (const auto &v : frameDws) {
            const QString dwName = v.toObject()
                .value(QStringLiteral("uniqueName")).toString();
            LeafNode leaf;
            if (workspace) {
                const QString stripped = stripVaultPrefix(dwName, workspace->vaultId());
                if (auto *wl = workspace->findLeafById(stripped)) {
                    QJsonObject obj = wl->serialize();
                    leaf = leafFromJson(obj);
                    continue_if_filled(leaf, tabs);
                    continue;
                }
            }
            // Fallback: workspace=nullptr, or leaf not found.
            leaf = leafSidecar().value(dwName, LeafNode{});
            if (leaf.id.isEmpty()) {
                leaf.id = dwName;
                leaf.viewType = QStringLiteral("empty");
                leaf.icon = QStringLiteral("lucide-file");
                leaf.title = QStringLiteral("New tab");
            }
            tabs.children.append(leaf);
        }
        if (workspace) {
            // Stacked bit: source-of-truth is Workspace when we have one.
            // (If KDDW exposes a per-Group stacked accessor, prefer that —
            // see Task 8 for the decision.)
            tabs.stacked = workspace->isTabGroupStacked(
                tabs.children.isEmpty() ? QString{} : tabs.children.first().id);
        } else {
            tabs.stacked = !tabs.children.isEmpty()
                && stackedSidecar().value(tabs.children.first().id, false);
        }
        parent.tabsChildren.append(tabs);
    }
}

QString stripVaultPrefix(const QString &uniqueName, const QString &vaultId)
{
    if (vaultId.isEmpty()) return uniqueName;
    const QString prefix = vaultId + QChar(':');
    return uniqueName.startsWith(prefix)
        ? uniqueName.mid(prefix.size())
        : uniqueName;
}

LeafNode leafFromJson(const QJsonObject &obj)
{
    LeafNode n;
    n.id = obj.value(QStringLiteral("id")).toString();
    auto stateObj = obj.value(QStringLiteral("state")).toObject();
    n.viewType = stateObj.value(QStringLiteral("type")).toString();
    n.icon = stateObj.value(QStringLiteral("icon")).toString();
    n.title = stateObj.value(QStringLiteral("title")).toString();
    n.state = stateObj.value(QStringLiteral("state")).toObject();
    n.pinned = obj.value(QStringLiteral("pinned")).toBool(false);
    n.group = obj.value(QStringLiteral("group")).toString();
    return n;
}
```

(`Workspace::isTabGroupStacked` is added in Task 8; for now, treat the call as a stub that returns false — we'll revisit when stacked is wired through the production path. `WorkspaceLeaf::m_unknownLeafKeys` plumbing is added in Task 7.)

- [ ] **Step 4: Update `toJson` to call the new walker**

In `libs/core/src/WorkspaceSerializer.cpp:445-448`, replace:

```cpp
out[QStringLiteral("main")] = renderSplit(walkKddwTreeSimple(main));
```

with:

```cpp
out[QStringLiteral("main")] = renderSplit(walkLayoutSaverTree(main, workspace));
```

- [ ] **Step 5: Run all serializer tests**

```bash
cmake --build build -j 10 --target tst_workspace_serializer
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: all PASS, including the strengthened fixture03 round-trip.

If fixtures 01, 02, 04, 06, 07, 08, 09 fail, the most likely cause is wrong field-name mapping in `locateMainArea` or `walkContainer`. Re-read the schema doc, fix, re-run.

- [ ] **Step 6: Commit**

```bash
git add libs/core/src/WorkspaceSerializer.cpp tests/core/tst_workspace_serializer.cpp
git commit -m "core/serializer: walk LayoutSaver JSON for nested-split toJson

Replaces the flat walkKddwTreeSimple with a recursive walker that parses
KDDW's LayoutSaver output and emits Obsidian-shape nested splits.
Strengthens fixture03 round-trip assertion. Fixes punch-list P1 #2
on the write side. Read side already worked via materializeSplit.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: Per-group `currentTab` from KDDW

**Files:**
- Modify: `libs/core/src/WorkspaceSerializer.cpp` (`walkContainer`)
- Create: `tests/core/fixtures/workspace-obsidian/11-per-group-currenttab.json`
- Modify: `tests/core/tst_workspace_serializer.cpp` (add `fixture11`)

The walker introduced in Task 2 already reads `node["currentTabIndex"]` from KDDW's JSON, but the source of truth is `Group::currentTabIndex()` for live groups; the JSON we parse from `LayoutSaver` may report `0` for groups that haven't had their currentTab explicitly set during `fromJson` materialization. This task ensures the per-group bit round-trips correctly. Closes punch-list P1 #3.

- [ ] **Step 1: Add fixture 11**

Create `tests/core/fixtures/workspace-obsidian/11-per-group-currenttab.json`:

```json
{
  "main": {
    "id": "rootf11aaaaaaaaa",
    "type": "split",
    "direction": "horizontal",
    "children": [
      {
        "id": "tabsf11A11aaaaaa",
        "type": "tabs",
        "currentTab": 2,
        "children": [
          { "id": "f11A1aaaaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } },
          { "id": "f11A2aaaaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } },
          { "id": "f11A3aaaaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } }
        ]
      },
      {
        "id": "tabsf11B11aaaaaa",
        "type": "tabs",
        "currentTab": 1,
        "children": [
          { "id": "f11B1aaaaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } },
          { "id": "f11B2aaaaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } },
          { "id": "f11B3aaaaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } }
        ]
      }
    ]
  },
  "active": "f11A3aaaaaaaaaaa",
  "lastOpenFiles": []
}
```

- [ ] **Step 2: Add the failing test**

Append to `tests/core/tst_workspace_serializer.cpp`. Add slot declaration `void fixture11_perGroupCurrentTab_roundtrips();` to `private slots:`.

```cpp
void TestWorkspaceSerializer::fixture11_perGroupCurrentTab_roundtrips()
{
    auto jsonIn = readFixture(QStringLiteral("11-per-group-currenttab.json"));
    QVERIFY(!jsonIn.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f11"), KDDockWidgets::MainWindowOption_None);
    mainWindow->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, mainWindow.get(), nullptr);
    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr);

    auto children = jsonOut.value(QStringLiteral("main")).toObject()
                          .value(QStringLiteral("children")).toArray();
    QCOMPARE(children.size(), 2);

    // Group A should report currentTab = 2; group B should report 1.
    QSet<int> seen;
    for (const auto &v : children) {
        auto tabs = v.toObject();
        QCOMPARE(tabs.value(QStringLiteral("type")).toString(),
                 QStringLiteral("tabs"));
        seen.insert(tabs.value(QStringLiteral("currentTab")).toInt());
    }
    QVERIFY(seen.contains(2));
    QVERIFY(seen.contains(1));
}
```

- [ ] **Step 3: Run, confirm it fails**

```bash
cmake --build build -j 10 --target tst_workspace_serializer
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: FAIL — likely both groups report `currentTab: 0`. The reason is that `fromJson`'s `materializeTabs` already calls `setAsCurrentTab` for the indexed leaf (`WorkspaceSerializer.cpp:279-284`), so the *KDDW state* is correct; the fail mode is more likely that the JSON path/keys in the schema doc don't surface `currentTabIndex` correctly. Fix per Step 4.

- [ ] **Step 4: Fix the walker to use `Group::currentTabIndex()` directly**

If LayoutSaver JSON's `currentTabIndex` is unreliable, switch to looking up the live `Core::Group*` and querying `Group::currentTabIndex()` directly. In `walkContainer`, when emitting a `TabsNode`, look up the matching live group:

```cpp
} else {
    // Frame/group node — emit a TabsNode.
    TabsNode tabs;
    const auto frameDws = node.value(QStringLiteral("dockWidgets")).toArray();
    QStringList dwIds;
    for (const auto &v : frameDws)
        dwIds.append(v.toObject().value(QStringLiteral("uniqueName")).toString());

    // Resolve the live KDDW Group via the first dock widget's id, then
    // read currentTabIndex from KDDW directly. JSON-side currentTabIndex
    // can be stale if the group's state changed after serialization.
    if (!dwIds.isEmpty()) {
        if (auto *core = KDDockWidgets::Core::DockWidget::byName(dwIds.first())) {
            if (auto *grp = core->dptr()->group()) {  // see note: prefer public path
                tabs.currentTab = grp->currentTabIndex();
            }
        }
    }
    // ... rest as in Task 2 ...
}
```

If `DockWidget::dptr()` is private, use the public alternative: walk `MainWindow::layout()->groups()` and find the group whose `dockWidgets()` contains a `DockWidget` with the matching `uniqueName()`. Snippet:

```cpp
auto *layout = main->layout();
if (!layout) return;
for (auto *grp : layout->groups()) {
    bool match = false;
    for (auto *dw : grp->dockWidgets())
        if (dw->uniqueName() == dwIds.first()) { match = true; break; }
    if (match) { tabs.currentTab = grp->currentTabIndex(); break; }
}
```

- [ ] **Step 5: Run, confirm test passes**

```bash
cmake --build build -j 10 --target tst_workspace_serializer
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: PASS for fixture11; existing fixtures still PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/core/src/WorkspaceSerializer.cpp tests/core/tst_workspace_serializer.cpp tests/core/fixtures/workspace-obsidian/11-per-group-currenttab.json
git commit -m "core/serializer: per-group currentTab via Group::currentTabIndex()

Reads currentTab from the live KDDW Group rather than the LayoutSaver JSON,
which can be stale. Closes punch-list P1 #3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 4: Floating-window walker recursion

**Files:**
- Modify: `libs/core/src/WorkspaceSerializer.cpp` (`floatingWindowAsSplit`)
- Create: `tests/core/fixtures/workspace-obsidian/13-popout-nested-split.json`
- Modify: `tests/core/tst_workspace_serializer.cpp` (add `fixture13`)

Today's `floatingWindowAsSplit` (`WorkspaceSerializer.cpp:390-403`) collapses any popout's contents to a single tabs node. With the recursive walker from Task 2, popouts containing nested splits should round-trip too.

- [ ] **Step 1: Add fixture 13**

Create `tests/core/fixtures/workspace-obsidian/13-popout-nested-split.json`:

```json
{
  "main": {
    "id": "f13mainaaaaaaaaa",
    "type": "split",
    "direction": "vertical",
    "children": [
      {
        "id": "f13mtabsaaaaaaaa",
        "type": "tabs",
        "children": [
          { "id": "f13mleafaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } }
        ]
      }
    ]
  },
  "floating": {
    "id": "f13floatingaaaaa",
    "type": "floating",
    "children": [
      {
        "id": "f13winaaaaaaaaaa",
        "type": "window",
        "direction": "horizontal",
        "x": 100, "y": 100, "width": 800, "height": 600, "maximize": false,
        "children": [
          {
            "id": "f13ltabsaaaaaaaa",
            "type": "tabs",
            "children": [
              { "id": "f13lleafaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } }
            ]
          },
          {
            "id": "f13rtabsaaaaaaaa",
            "type": "tabs",
            "children": [
              { "id": "f13rleafaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } }
            ]
          }
        ]
      }
    ]
  },
  "active": "f13mleafaaaaaaaa",
  "lastOpenFiles": []
}
```

- [ ] **Step 2: Add failing test**

Append to `tests/core/tst_workspace_serializer.cpp`. Add slot declaration `void fixture13_popoutNestedSplit_roundtrips();`.

```cpp
void TestWorkspaceSerializer::fixture13_popoutNestedSplit_roundtrips()
{
    auto jsonIn = readFixture(QStringLiteral("13-popout-nested-split.json"));
    QVERIFY(!jsonIn.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f13"), KDDockWidgets::MainWindowOption_None);
    mainWindow->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, mainWindow.get(), nullptr);

    auto *registry = KDDockWidgets::DockRegistry::self();
    QCOMPARE(registry->dockwidgets().size(), 3);
    QCOMPARE(registry->floatingWindows().size(), 1);

    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr);
    auto floatingOut = jsonOut.value(QStringLiteral("floating")).toObject()
                              .value(QStringLiteral("children")).toArray();
    QCOMPARE(floatingOut.size(), 1);
    auto win = floatingOut.first().toObject();
    auto winChildren = win.value(QStringLiteral("children")).toArray();
    // The popout had a horizontal split with two tabs nodes; round-trip
    // should preserve that, not flatten to one tabs.
    QCOMPARE(winChildren.size(), 2);
    for (const auto &v : winChildren) {
        QCOMPARE(v.toObject().value(QStringLiteral("type")).toString(),
                 QStringLiteral("tabs"));
    }
}
```

- [ ] **Step 3: Run, confirm fail**

```bash
cmake --build build -j 10 --target tst_workspace_serializer
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: FAIL — `winChildren.size() == 1` because `floatingWindowAsSplit` flattens.

- [ ] **Step 4: Implement recursive popout walker**

Replace `floatingWindowAsSplit` (`WorkspaceSerializer.cpp:390-403`) with a function that walks the FloatingWindow's layout via the same `walkLayoutSaverTree` mechanism. Since `FloatingWindow` has its own `Layout`, we can reuse `walkContainer` symmetrically, but the entry path through LayoutSaver JSON is different (`floatingWindows[i].multiSplitterLayout.root` instead of `mainWindows[...]`).

```cpp
SplitNode floatingWindowAsSplit(KDDockWidgets::Core::FloatingWindow *fw,
                                Workspace *workspace,
                                const QJsonObject &layoutSaverRoot)
{
    SplitNode root;
    root.direction = QStringLiteral("vertical");

    // Locate this floating window in the LayoutSaver JSON by its identifier.
    // FIELDS_FROM_SCHEMA: replace key names with those from the schema doc.
    const auto fws = layoutSaverRoot.value(QStringLiteral("floatingWindows")).toArray();
    for (const auto &v : fws) {
        auto obj = v.toObject();
        // Match against fw's identity (e.g. by parentMainWindow / first
        // dockwidget / index — pick whichever the schema exposes).
        if (matchesFw(obj, fw)) {
            QJsonObject area = obj.value(QStringLiteral("multiSplitterLayout"))
                                  .toObject().value(QStringLiteral("root")).toObject();
            walkContainer(area, root, /*main*/ nullptr, workspace);
            return root;
        }
    }
    // Fallback: KDDW's LayoutSaver did not include this fw in its dump
    // (defensive — should not happen for a live fw). Emit the flat shape.
    TabsNode tabs;
    for (auto *dw : fw->dockWidgets()) {
        LeafNode l;
        l.id = dw->uniqueName();
        l.viewType = QStringLiteral("empty");
        tabs.children.append(l);
    }
    root.tabsChildren.append(tabs);
    return root;
}
```

Then update `toJson` (`WorkspaceSerializer.cpp:450-473`) to capture `layoutSaverRoot` once and pass it to both `walkLayoutSaverTree` (refactor to accept a pre-parsed root to avoid double-serialization) and `floatingWindowAsSplit`.

```cpp
QJsonObject toJson(KDDockWidgets::QtWidgets::MainWindow *main, Workspace *workspace)
{
    QJsonObject out;
    KDDockWidgets::LayoutSaver saver;
    QJsonObject layoutSaverRoot =
        QJsonDocument::fromJson(saver.serializeLayout()).object();

    out[QStringLiteral("main")] =
        renderSplit(walkLayoutSaverTree(main, workspace, layoutSaverRoot));

    auto *registry = KDDockWidgets::DockRegistry::self();
    auto fws = registry->floatingWindows();
    if (!fws.isEmpty()) {
        QJsonObject floating;
        floating[QStringLiteral("type")] = QStringLiteral("floating");
        QJsonArray windows;
        for (auto *fw : fws) {
            QJsonObject windowObj =
                renderSplit(floatingWindowAsSplit(fw, workspace, layoutSaverRoot));
            windowObj[QStringLiteral("type")] = QStringLiteral("window");
            const auto rect = fw->geometry();
            windowObj[QStringLiteral("x")] = rect.x();
            windowObj[QStringLiteral("y")] = rect.y();
            windowObj[QStringLiteral("width")] = rect.width();
            windowObj[QStringLiteral("height")] = rect.height();
            if (fw->view() && fw->view()->isMaximized())
                windowObj[QStringLiteral("maximize")] = true;
            windows.append(windowObj);
        }
        floating[QStringLiteral("children")] = windows;
        out[QStringLiteral("floating")] = floating;
    }
    return out;
}
```

`matchesFw` is a small helper that decides which floating window in the JSON corresponds to the live `FloatingWindow*` — implementation depends on the schema (likely matches the first dock widget's uniqueName).

- [ ] **Step 5: Run all tests, confirm pass**

```bash
cmake --build build -j 10 --target tst_workspace_serializer
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: all PASS, including fixture05 (single-tabs popout) and fixture13 (nested-split popout).

- [ ] **Step 6: `materializeFloatingWindow` symmetry**

The read-side `materializeFloatingWindow` (`WorkspaceSerializer.cpp:358-385`) only handles single-tabs popouts (line 361 hard-coded check). For full read-side support of nested splits inside popouts, refactor it to use `materializeSplit` recursively. Replace the body:

```cpp
void materializeFloatingWindow(const WindowNode &w,
                               KDDockWidgets::QtWidgets::MainWindow *main)
{
    if (w.content.tabsChildren.isEmpty() && w.content.splitChildren.isEmpty())
        return;

    // Phase 4: full nested-split support inside floating windows.
    // Strategy: dock the first leaf into main as a momentary anchor, float
    // it to create the FloatingWindow with proper geometry, then materialize
    // the remaining tree relative to that anchor inside the float.
    KDDockWidgets::QtWidgets::DockWidget *first = nullptr;
    auto firstLeafId = findFirstLeafId(w.content); // walks tabs/splits DFS
    if (firstLeafId.isEmpty()) return;

    first = new KDDockWidgets::QtWidgets::DockWidget(firstLeafId);
    main->addDockWidget(first, KDDockWidgets::Location_OnRight);
    first->dockWidget()->setFloating(true);
    if (w.width > 0 && w.height > 0) {
        first->dockWidget()->setFloatingGeometry(
            QRect(w.x, w.y, w.width, w.height));
    }
    if (w.maximize) {
        if (auto *fw = first->dockWidget()->floatingWindow();
            fw && fw->view())
            fw->view()->showMaximized();
    }

    // Materialize the rest of the tree relative to `first`. Re-use
    // materializeSplit; it already handles the "first child uses
    // baseLocation, subsequent siblings derive from split.direction"
    // pattern. We pass `first` as the relativeTo so all docking happens
    // inside the floating window.
    SplitNode trimmed = w.content;
    removeFirstLeaf(trimmed, firstLeafId); // strips firstLeafId from trimmed
    materializeSplit(trimmed, main, first, KDDockWidgets::Location_OnRight);
}

QString findFirstLeafId(const SplitNode &node)
{
    if (!node.tabsChildren.isEmpty() && !node.tabsChildren.first().children.isEmpty())
        return node.tabsChildren.first().children.first().id;
    for (const auto &s : node.splitChildren) {
        QString r = findFirstLeafId(s);
        if (!r.isEmpty()) return r;
    }
    return {};
}

void removeFirstLeaf(SplitNode &node, const QString &id)
{
    if (!node.tabsChildren.isEmpty()) {
        auto &firstTabs = node.tabsChildren.first();
        if (!firstTabs.children.isEmpty() && firstTabs.children.first().id == id) {
            firstTabs.children.removeFirst();
            if (firstTabs.children.isEmpty())
                node.tabsChildren.removeFirst();
            return;
        }
    }
    for (auto &s : node.splitChildren)
        removeFirstLeaf(s, id);
}
```

- [ ] **Step 7: Run all tests again**

```bash
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: all PASS, including fixture05 (single-tabs popout: `materializeSplit` on the trimmed empty body is a no-op, leaving just the anchored first leaf).

- [ ] **Step 8: Commit**

```bash
git add libs/core/src/WorkspaceSerializer.cpp tests/core/tst_workspace_serializer.cpp tests/core/fixtures/workspace-obsidian/13-popout-nested-split.json
git commit -m "core/serializer: nested-split round-trip for popouts

floatingWindowAsSplit reuses walkContainer; materializeFloatingWindow
delegates to materializeSplit for the post-anchor tree. Single-tabs
popouts (fixture05) still pass via the empty-trimmed path.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 5: Wire `Workspace*` into `materializeTabs` (read side)

**Files:**
- Modify: `libs/core/src/WorkspaceSerializer.cpp` (`materializeTabs`)
- Create: `tests/core/fixtures/workspace-obsidian/12-nested-with-state.json`
- Modify: `tests/core/tst_workspace_serializer.cpp` (add `fixture12`)

This task makes `fromJson` drive `Workspace::createLeafInGroupOf` instead of constructing raw `DockWidget`s when `workspace` is non-null. The view-state on each leaf gets set so it materializes a real `View` (or, in tests, the registry's "empty" placeholder).

- [ ] **Step 1: Add fixture 12**

Create `tests/core/fixtures/workspace-obsidian/12-nested-with-state.json`:

```json
{
  "main": {
    "id": "f12rootaaaaaaaaa",
    "type": "split",
    "direction": "horizontal",
    "children": [
      {
        "id": "f12tabsAaaaaaaaa",
        "type": "tabs",
        "children": [
          {
            "id": "f12leafA1aaaaaaa",
            "type": "leaf",
            "state": { "type": "markdown", "state": { "file": "FolderA/Note1.md" } },
            "pinned": true,
            "group": "linked-pair-1"
          }
        ]
      },
      {
        "id": "f12tabsBaaaaaaaaa",
        "type": "tabs",
        "children": [
          {
            "id": "f12leafB1aaaaaaa",
            "type": "leaf",
            "state": { "type": "markdown", "state": { "file": "FolderB/Note2.md" } },
            "group": "linked-pair-1"
          },
          {
            "id": "f12leafB2aaaaaaa",
            "type": "leaf",
            "state": { "type": "markdown", "state": { "file": "FolderB/Note3.md" } }
          }
        ]
      }
    ]
  },
  "active": "f12leafA1aaaaaaa",
  "lastOpenFiles": []
}
```

- [ ] **Step 2: Add the failing test**

Append to `tests/core/tst_workspace_serializer.cpp`. The test constructs a real `Workspace` with a `ViewRegistry` containing only an "empty" view factory (so unknown view types fall back; we don't need `markdown` to actually render). Add slot declaration `void fixture12_nestedWithState_workspaceRoundTrip();`. Also add includes:

```cpp
#include "corbomite/core/View.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
```

```cpp
namespace {

class FakeView : public Corbomite::View
{
public:
    explicit FakeView(Corbomite::WorkspaceLeaf *leaf) : Corbomite::View(leaf) {}
    QString getViewType() const override { return m_type; }
    void setViewType(const QString &t) { m_type = t; }
    QJsonObject getState() const override { return m_state; }
    void setState(const QJsonObject &s) override { m_state = s; }
    QString getDisplayText() const override { return QStringLiteral("Fake"); }
    QString getIcon() const override { return QStringLiteral("document"); }
    void open(QWidget *) override {}
    void close() override {}

private:
    QString m_type = QStringLiteral("empty");
    QJsonObject m_state;
};

} // namespace

void TestWorkspaceSerializer::fixture12_nestedWithState_workspaceRoundTrip()
{
    auto jsonIn = readFixture(QStringLiteral("12-nested-with-state.json"));
    QVERIFY(!jsonIn.isEmpty());

    Corbomite::ViewRegistry registry;
    registry.registerViewCreator(QStringLiteral("empty"),
        [](Corbomite::WorkspaceLeaf *leaf) -> Corbomite::View * {
            auto *v = new FakeView(leaf);
            v->setViewType(QStringLiteral("empty"));
            return v;
        });
    registry.registerViewCreator(QStringLiteral("markdown"),
        [](Corbomite::WorkspaceLeaf *leaf) -> Corbomite::View * {
            auto *v = new FakeView(leaf);
            v->setViewType(QStringLiteral("markdown"));
            return v;
        });

    Corbomite::Workspace workspace(QStringLiteral("test-vault"), &registry);
    auto *kddwMain = qobject_cast<KDDockWidgets::QtWidgets::MainWindow *>(
        workspace.rootWidget());
    QVERIFY(kddwMain);
    kddwMain->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, kddwMain, &workspace);

    // Three Workspace leaves should exist with the JSON-side ids.
    auto *leafA1 = workspace.findLeafById(QStringLiteral("f12leafA1aaaaaaa"));
    auto *leafB1 = workspace.findLeafById(QStringLiteral("f12leafB1aaaaaaa"));
    auto *leafB2 = workspace.findLeafById(QStringLiteral("f12leafB2aaaaaaa"));
    QVERIFY(leafA1); QVERIFY(leafB1); QVERIFY(leafB2);

    QCOMPARE(leafA1->pinned(), true);
    QCOMPARE(leafA1->group(), QStringLiteral("linked-pair-1"));
    QCOMPARE(leafB1->group(), QStringLiteral("linked-pair-1"));

    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(kddwMain, &workspace);
    auto outChildren = jsonOut.value(QStringLiteral("main")).toObject()
                              .value(QStringLiteral("children")).toArray();
    QCOMPARE(outChildren.size(), 2);   // two top-level tabs (one per side)
}
```

- [ ] **Step 3: Run, confirm fail**

```bash
cmake --build build -j 10 --target tst_workspace_serializer
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: FAIL — `findLeafById` returns nullptr because `materializeTabs` doesn't drive `Workspace` when `workspace != nullptr` today (it just constructs a raw `DockWidget`).

- [ ] **Step 4: Update `materializeTabs` to drive Workspace when non-null**

Replace `materializeTabs` (`WorkspaceSerializer.cpp:263-286`) with:

```cpp
KDDockWidgets::QtWidgets::DockWidget *
materializeTabs(const TabsNode &tabs,
                KDDockWidgets::QtWidgets::MainWindow *main,
                KDDockWidgets::QtWidgets::DockWidget *relativeTo,
                KDDockWidgets::Location location,
                Workspace *workspace)
{
    KDDockWidgets::QtWidgets::DockWidget *first = nullptr;
    Corbomite::WorkspaceLeaf *firstWlInGroup = nullptr;
    for (const auto &leaf : tabs.children) {
        KDDockWidgets::QtWidgets::DockWidget *dw = nullptr;
        if (workspace) {
            // Drive Workspace to construct a WorkspaceLeaf + DockWidget.
            // For the first leaf in the group: anchor=null (creates new
            // group). For subsequent leaves: anchor=firstWlInGroup (joins
            // its group).
            auto *wl = workspace->createLeafInGroupOf(firstWlInGroup);
            // Rebind id from the auto-generated one to the JSON-side id.
            // findLeafById bookkeeping mirrors Workspace::deserialize's
            // current behaviour (Workspace.cpp:786-790).
            const QString autoId = wl->id();
            workspace->dropLeafIdMapping(autoId);   // see helper added below
            wl->setId(leaf.id);
            workspace->insertLeafIdMapping(leaf.id, wl);

            if (leaf.pinned) wl->setPinned(true);
            if (!leaf.group.isEmpty()) wl->setGroup(leaf.group);

            QJsonObject viewState;
            viewState[QStringLiteral("type")] = leaf.viewType;
            if (!leaf.state.isEmpty())
                viewState[QStringLiteral("state")] = leaf.state;
            if (!leaf.icon.isEmpty())
                viewState[QStringLiteral("icon")] = leaf.icon;
            if (!leaf.title.isEmpty())
                viewState[QStringLiteral("title")] = leaf.title;
            if (!viewState.value(QStringLiteral("type")).toString().isEmpty())
                wl->setViewState(viewState);

            // Carry unknown leaf keys (Task 7 plumbing).
            wl->setUnknownLeafKeys(leaf.unknownKeys);

            dw = wl->dockWidget();
            if (!firstWlInGroup) firstWlInGroup = wl;
        } else {
            // Test fallback: raw DockWidget construction (existing behaviour).
            dw = new KDDockWidgets::QtWidgets::DockWidget(leaf.id);
            if (!first) {
                main->addDockWidget(dw, location, relativeTo);
            } else {
                first->addDockWidgetAsTab(dw);
            }
        }
        if (!first) first = dw;
    }
    if (first && tabs.currentTab > 0 && tabs.currentTab < tabs.children.size()) {
        if (auto *current = KDDockWidgets::Core::DockWidget::byName(
                tabs.children[tabs.currentTab].id)) {
            current->setAsCurrentTab();
        }
    }
    return first;
}
```

`createLeafInGroupOf(firstWlInGroup)` already calls `addDockWidget` (when `firstWlInGroup` is null) or `addDockWidgetAsTab` (when non-null) internally — so the location/relativeTo path is implicit through `Workspace`. **However**, Workspace's `createLeafInGroupOf` always docks new groups at `Location_OnRight` (`Workspace.cpp:284-286`). For nested splits we need the materializer to control placement. Adapter: when `workspace != nullptr`, after the first leaf in a non-first group is created, *move* its dock widget to the requested location.

Add to `Workspace.h` public API:

```cpp
/// Place the leaf's dock widget at `location` relative to `relativeTo`.
/// Used by WorkspaceSerializer to control split placement during
/// fromJson materialization. If `relativeTo` is null, docks against the
/// MainWindow root at the requested side.
void placeLeafAt(WorkspaceLeaf *leaf, int location, WorkspaceLeaf *relativeTo);

/// O(1) leaf-id index helpers used by WorkspaceSerializer.
void dropLeafIdMapping(const QString &id);
void insertLeafIdMapping(const QString &id, WorkspaceLeaf *leaf);
```

Implementation in `Workspace.cpp`:

```cpp
void Workspace::placeLeafAt(WorkspaceLeaf *leaf, int location,
                             WorkspaceLeaf *relativeTo)
{
    if (!leaf || !leaf->dockWidget() || !m_kddwMain) return;
    auto *kddwLoc = static_cast<KDDockWidgets::Location>(location);
    auto *dw = leaf->dockWidget();
    auto *anchor = relativeTo ? relativeTo->dockWidget() : nullptr;
    QSignalBlocker block(dw);
    if (anchor)
        m_kddwMain->addDockWidget(dw, kddwLoc, anchor);
    else
        m_kddwMain->addDockWidget(dw, kddwLoc);
    Q_EMIT layoutChanged();
}

void Workspace::dropLeafIdMapping(const QString &id)
{
    m_leavesById.remove(id);
}

void Workspace::insertLeafIdMapping(const QString &id, WorkspaceLeaf *leaf)
{
    m_leavesById.insert(id, leaf);
}
```

Then `materializeTabs` and `materializeSplit` are updated to thread the workspace pointer through and call `Workspace::placeLeafAt(firstWlInGroup, location, relativeToWl)` for the first leaf of each new group when `workspace != nullptr`.

(The exact threading is a refactor of `placeChild` lambdas in `materializeSplit`; preserve the existing first/anchor logic but route placement through `Workspace::placeLeafAt` when applicable.)

- [ ] **Step 5: Run, confirm fixture12 passes; existing fixtures still pass**

```bash
cmake --build build -j 10 --target tst_workspace_serializer
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: all PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/Workspace.h libs/core/src/Workspace.cpp libs/core/src/WorkspaceSerializer.cpp tests/core/tst_workspace_serializer.cpp tests/core/fixtures/workspace-obsidian/12-nested-with-state.json
git commit -m "core/serializer: wire Workspace into materializeTabs for production fromJson

Workspace::placeLeafAt threads location/relativeTo through, so the
recursive materializer can place leaves into nested splits while still
going through Workspace's leaf factory (registers leaves, wires KDDW
signals, namespaces unique names).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 6: Wire `Workspace*` into `walkContainer` (write side leaf payload)

**Files:**
- Modify: `tests/core/tst_workspace_serializer.cpp` (extend fixture12 assertions)

The walker from Task 2 already has the `workspace`-aware code path (looking up `WorkspaceLeaf` via `findLeafById` and using `wl->serialize()`). This task strengthens fixture 12 to actually assert that round-trip preserves leaf state, then runs.

- [ ] **Step 1: Strengthen fixture 12 assertions**

In `fixture12_nestedWithState_workspaceRoundTrip`, append to the existing assertions:

```cpp
    // Drill into the output and confirm leaf state survived.
    bool sawA1 = false, sawB1 = false;
    for (const auto &v : outChildren) {
        auto tabs = v.toObject();
        for (const auto &lv : tabs.value(QStringLiteral("children")).toArray()) {
            auto leaf = lv.toObject();
            const QString id = leaf.value(QStringLiteral("id")).toString();
            const auto state = leaf.value(QStringLiteral("state")).toObject();
            if (id == QStringLiteral("f12leafA1aaaaaaa")) {
                sawA1 = true;
                QCOMPARE(state.value(QStringLiteral("type")).toString(),
                         QStringLiteral("markdown"));
                QCOMPARE(state.value(QStringLiteral("state")).toObject()
                              .value(QStringLiteral("file")).toString(),
                         QStringLiteral("FolderA/Note1.md"));
                QCOMPARE(leaf.value(QStringLiteral("pinned")).toBool(), true);
                QCOMPARE(leaf.value(QStringLiteral("group")).toString(),
                         QStringLiteral("linked-pair-1"));
            }
            if (id == QStringLiteral("f12leafB1aaaaaaa")) {
                sawB1 = true;
                QCOMPARE(leaf.value(QStringLiteral("group")).toString(),
                         QStringLiteral("linked-pair-1"));
            }
        }
    }
    QVERIFY(sawA1);
    QVERIFY(sawB1);
```

- [ ] **Step 2: Run, confirm pass**

```bash
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: PASS. Task 2's walker already calls `wl->serialize()` when workspace is non-null; the only thing in question is whether `viewType` and inner state survive. They should because the FakeView in fixture12 echoes back its setState() input and reports its registered viewType.

If FAIL: most likely `WorkspaceLeaf::getViewState()` is returning empty when the leaf is deferred. Inspect `WorkspaceLeaf.cpp:129-142`. If the leaf got deferred during `fromJson` (via `setDeferred`), getViewState returns `m_deferredViewState`. Confirm that `m_deferredViewState` is hydrated in the fixture path. Adjust the materializer if needed.

- [ ] **Step 3: Commit**

```bash
git add tests/core/tst_workspace_serializer.cpp
git commit -m "core/serializer: assert leaf state survives toJson with Workspace

Strengthens fixture12 to verify markdown viewType, file path, pinned,
and group all round-trip through the workspace-non-null write path.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 7: `WorkspaceLeaf::m_unknownLeafKeys` round-trip

**Files:**
- Modify: `libs/core/include/corbomite/core/WorkspaceLeaf.h`
- Modify: `libs/core/src/WorkspaceLeaf.cpp`
- Modify: `libs/core/src/WorkspaceSerializer.cpp`
- Modify: `tests/core/tst_workspace_serializer.cpp` (add `fixture08b`)

Today's `leafSidecar` static map (`WorkspaceSerializer.cpp:51-55`) carries unknown leaf keys for the `workspace=nullptr` round-trip in fixture 08. For production, unknown keys must travel on the live `WorkspaceLeaf`. We add a field, accessor pair, and wire `materializeTabs` to populate it, `walkContainer` to read it.

- [ ] **Step 1: Add fixture 08b — workspace-non-null unknown-keys round-trip**

Append to `tests/core/tst_workspace_serializer.cpp`. Add slot declaration `void fixture08b_unknownKeys_workspaceRoundTrip();`.

```cpp
void TestWorkspaceSerializer::fixture08b_unknownKeys_workspaceRoundTrip()
{
    auto jsonIn = readFixture(QStringLiteral("08-unknown-keys.json"));
    QVERIFY(!jsonIn.isEmpty());

    Corbomite::ViewRegistry registry;
    registry.registerViewCreator(QStringLiteral("empty"),
        [](Corbomite::WorkspaceLeaf *leaf) -> Corbomite::View * {
            auto *v = new FakeView(leaf);
            v->setViewType(QStringLiteral("empty"));
            return v;
        });
    Corbomite::Workspace workspace(QStringLiteral("test-vault-08b"), &registry);
    auto *kddwMain = qobject_cast<KDDockWidgets::QtWidgets::MainWindow *>(
        workspace.rootWidget());
    kddwMain->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, kddwMain, &workspace);
    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(kddwMain, &workspace);

    auto leaf = jsonOut.value(QStringLiteral("main")).toObject()
                       .value(QStringLiteral("children")).toArray()
                       .first().toObject()
                       .value(QStringLiteral("children")).toArray()
                       .first().toObject();
    auto obsidianInternal = leaf.value(QStringLiteral("obsidianInternal")).toObject();
    QCOMPARE(obsidianInternal.value(QStringLiteral("someField")).toInt(), 42);
}
```

- [ ] **Step 2: Run, confirm fail**

```bash
cmake --build build -j 10 --target tst_workspace_serializer
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: FAIL — `obsidianInternal` not present in output because `wl->serialize()` doesn't carry unknown keys today.

- [ ] **Step 3: Add `m_unknownLeafKeys` to WorkspaceLeaf**

Edit `libs/core/include/corbomite/core/WorkspaceLeaf.h`. Find the persistence section (around `serialize/deserialize` declarations) and add:

```cpp
/// Unknown root-level keys read from this leaf's JSON during fromJson.
/// Round-tripped verbatim by serialize() so vault-format bumps in
/// Obsidian don't lose data on Corbomite save.
QJsonObject unknownLeafKeys() const;
void setUnknownLeafKeys(const QJsonObject &keys);
```

And add the field next to `m_pinned`/`m_group`:

```cpp
QJsonObject m_unknownLeafKeys;
```

Edit `libs/core/src/WorkspaceLeaf.cpp`. Implement the accessors:

```cpp
QJsonObject WorkspaceLeaf::unknownLeafKeys() const { return m_unknownLeafKeys; }

void WorkspaceLeaf::setUnknownLeafKeys(const QJsonObject &keys)
{
    m_unknownLeafKeys = keys;
}
```

Update `WorkspaceLeaf::serialize` (`WorkspaceLeaf.cpp:329-342`) to merge unknown keys into the output:

```cpp
QJsonObject WorkspaceLeaf::serialize() const
{
    QJsonObject json;
    json[QStringLiteral("id")] = id();
    json[QStringLiteral("type")] = QStringLiteral("leaf");
    json[QStringLiteral("state")] = getViewState();

    if (m_pinned)
        json[QStringLiteral("pinned")] = true;
    if (!m_group.isEmpty())
        json[QStringLiteral("group")] = m_group;

    // Round-trip Obsidian's unknown leaf keys (forward-compat).
    for (auto it = m_unknownLeafKeys.begin(); it != m_unknownLeafKeys.end(); ++it)
        json.insert(it.key(), it.value());

    return json;
}
```

- [ ] **Step 4: Wire `materializeTabs` to populate `m_unknownLeafKeys`**

Already present in Task 5's snippet (`wl->setUnknownLeafKeys(leaf.unknownKeys);`). Verify the line is in place. If absent, add it after `wl->setViewState(...)`.

- [ ] **Step 5: Wire `walkContainer` to read `m_unknownLeafKeys`**

In `walkContainer`'s `wl->serialize()` path, the merged keys come through automatically because `serialize()` now merges them. No additional code needed.

- [ ] **Step 6: Run all tests**

```bash
cmake --build build -j 10 --target tst_workspace_serializer
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: all PASS, including fixture08 (workspace=nullptr; uses leafSidecar fallback) and fixture08b (workspace-non-null; uses m_unknownLeafKeys).

- [ ] **Step 7: Commit**

```bash
git add libs/core/include/corbomite/core/WorkspaceLeaf.h libs/core/src/WorkspaceLeaf.cpp libs/core/src/WorkspaceSerializer.cpp tests/core/tst_workspace_serializer.cpp
git commit -m "core/leaf: WorkspaceLeaf carries unknown leaf JSON keys

Round-tripped verbatim by serialize() so Obsidian-authored vault format
bumps don't lose data on Corbomite save. Production path uses
m_unknownLeafKeys; tests with workspace=nullptr keep the legacy
leafSidecar map.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 8: `stacked` per-tab-group bit storage

**Files:**
- Modify: `libs/core/include/corbomite/core/Workspace.h`
- Modify: `libs/core/src/Workspace.cpp`
- Modify: `libs/core/src/WorkspaceSerializer.cpp`

KDDW's `Group` has no public `stacked` accessor (verified against `/usr/include/kddockwidgets-qt6/kddockwidgets/core/Group.h`). For workspace-non-null path we carry stacked on `Workspace` keyed by tabGroupId. For workspace-null tests, the existing `stackedSidecar` static map continues to work.

- [ ] **Step 1: Confirm fixture04 currently passes with workspace=nullptr**

```bash
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: all PASS. Fixture 04 sets the bit in `stackedSidecar` during parse and reads it back during walk.

- [ ] **Step 2: Add `Workspace::isTabGroupStacked` / `setTabGroupStacked`**

Edit `libs/core/include/corbomite/core/Workspace.h`. Add public methods:

```cpp
/// Whether the tab group named `tabGroupId` is rendered in stacked
/// (all-tabs-side-by-side) mode. Round-tripped verbatim through
/// workspace.json. Currently advisory only — KDDW lacks the rendering
/// hook to actually display stacked tabs natively, but the bit
/// preserves the user's intent across save+load.
bool isTabGroupStacked(const QString &tabGroupId) const;
void setTabGroupStacked(const QString &tabGroupId, bool stacked);
```

And the field, next to `m_tabGroupOf`:

```cpp
QHash<QString, bool> m_stackedGroups;
```

Implementation in `Workspace.cpp`:

```cpp
bool Workspace::isTabGroupStacked(const QString &tabGroupId) const
{
    return m_stackedGroups.value(tabGroupId, false);
}

void Workspace::setTabGroupStacked(const QString &tabGroupId, bool stacked)
{
    if (stacked) m_stackedGroups.insert(tabGroupId, true);
    else m_stackedGroups.remove(tabGroupId);
}
```

- [ ] **Step 3: Wire `materializeTabs` to set stacked bit when workspace non-null**

In Task 5's `materializeTabs` extension, after the first leaf is created, add:

```cpp
            if (firstWlInGroup == wl && tabs.stacked && workspace) {
                // Carry the stacked bit on the live tabGroupId of the
                // first leaf in this group.
                workspace->setTabGroupStacked(
                    workspace->tabGroupIdOf(wl), true);
            }
```

(`Workspace::tabGroupIdOf(WorkspaceLeaf*)` reads `m_tabGroupOf.value(leaf)`. Add it as a public accessor:)

```cpp
// Workspace.h
QString tabGroupIdOf(WorkspaceLeaf *leaf) const;

// Workspace.cpp
QString Workspace::tabGroupIdOf(WorkspaceLeaf *leaf) const
{
    return m_tabGroupOf.value(leaf);
}
```

- [ ] **Step 4: Wire `walkContainer` to read stacked from `Workspace`**

This is already in Task 2's skeleton (`tabs.stacked = workspace->isTabGroupStacked(...)`). Verify the call wires up correctly: the lookup key is the *live* `tabGroupId` for the first leaf. Implementation:

```cpp
if (workspace && !tabs.children.isEmpty()) {
    if (auto *wl = workspace->findLeafById(tabs.children.first().id))
        tabs.stacked = workspace->isTabGroupStacked(workspace->tabGroupIdOf(wl));
}
```

- [ ] **Step 5: Run all tests**

```bash
cmake --build build -j 10 --target tst_workspace_serializer
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: all PASS, including fixture04 (workspace=nullptr; stackedSidecar) and any new test with workspace-non-null.

- [ ] **Step 6: Add fixture04b — workspace-non-null stacked round-trip**

Append to `tst_workspace_serializer.cpp`. Add slot `void fixture04b_stacked_workspaceRoundTrip();`.

```cpp
void TestWorkspaceSerializer::fixture04b_stacked_workspaceRoundTrip()
{
    auto jsonIn = readFixture(QStringLiteral("04-stacked-tabs.json"));
    QVERIFY(!jsonIn.isEmpty());

    Corbomite::ViewRegistry registry;
    registry.registerViewCreator(QStringLiteral("empty"),
        [](Corbomite::WorkspaceLeaf *leaf) -> Corbomite::View * {
            auto *v = new FakeView(leaf);
            v->setViewType(QStringLiteral("empty"));
            return v;
        });
    Corbomite::Workspace workspace(QStringLiteral("test-vault-04b"), &registry);
    auto *kddwMain = qobject_cast<KDDockWidgets::QtWidgets::MainWindow *>(
        workspace.rootWidget());
    kddwMain->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, kddwMain, &workspace);
    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(kddwMain, &workspace);

    auto tabsOut = jsonOut.value(QStringLiteral("main")).toObject()
                          .value(QStringLiteral("children")).toArray()
                          .first().toObject();
    QCOMPARE(tabsOut.value(QStringLiteral("type")).toString(),
             QStringLiteral("tabs"));
    QCOMPARE(tabsOut.value(QStringLiteral("stacked")).toBool(), true);
}
```

Run, confirm pass.

- [ ] **Step 7: Commit**

```bash
git add libs/core/include/corbomite/core/Workspace.h libs/core/src/Workspace.cpp libs/core/src/WorkspaceSerializer.cpp tests/core/tst_workspace_serializer.cpp
git commit -m "core/workspace: stacked tab-group bit on Workspace

QHash<tabGroupId, bool>; round-tripped through workspace.json. KDDW lacks
a stacked-rendering hook so the bit is advisory, but the user's intent
survives save+load. Tests with workspace=nullptr keep the legacy
stackedSidecar fallback.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 9: `Workspace::serialize / deserialize` forwarders

**Files:**
- Modify: `libs/core/src/Workspace.cpp` (`serialize`, `deserialize`)
- Modify: `tests/core/tst_workspace_serializer.cpp` (add identity test)
- Modify: `tests/core/tst_workspace_serialize.cpp` (existing direct-Workspace serialize tests must continue to pass)

This is the consolidation milestone. After this task, every `workspace.json` write goes through `WorkspaceSerializer`. Closes punch-list P1 #1.

- [ ] **Step 1: Add identity test — Workspace::serialize matches WorkspaceSerializer::toJson**

Append to `tst_workspace_serializer.cpp`. Add slot `void identity_workspaceSerializeMatchesSerializerToJson();`.

```cpp
void TestWorkspaceSerializer::identity_workspaceSerializeMatchesSerializerToJson()
{
    Corbomite::ViewRegistry registry;
    registry.registerViewCreator(QStringLiteral("empty"),
        [](Corbomite::WorkspaceLeaf *leaf) -> Corbomite::View * {
            auto *v = new FakeView(leaf);
            v->setViewType(QStringLiteral("empty"));
            return v;
        });
    Corbomite::Workspace workspace(QStringLiteral("test-id"), &registry);
    auto *kddwMain = qobject_cast<KDDockWidgets::QtWidgets::MainWindow *>(
        workspace.rootWidget());
    kddwMain->show();

    auto jsonIn = readFixture(QStringLiteral("12-nested-with-state.json"));
    Corbomite::WorkspaceSerializer::fromJson(jsonIn, kddwMain, &workspace);

    QJsonObject viaWorkspace = workspace.serialize();
    QJsonObject viaSerializer = Corbomite::WorkspaceSerializer::toJson(kddwMain, &workspace);

    QCOMPARE(QJsonDocument(viaWorkspace).toJson(QJsonDocument::Compact),
             QJsonDocument(viaSerializer).toJson(QJsonDocument::Compact));
}
```

- [ ] **Step 2: Run, confirm fail**

```bash
cmake --build build -j 10 --target tst_workspace_serializer
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: FAIL — today's `Workspace::serialize` builds its own flat shape (`Workspace.cpp:649-713`) which won't match the recursive walker output.

- [ ] **Step 3: Replace `Workspace::serialize` body**

In `libs/core/src/Workspace.cpp`, replace the entire body of `Workspace::serialize` (lines 649-713) with:

```cpp
QJsonObject Workspace::serialize() const
{
    QJsonObject json = WorkspaceSerializer::toJson(
        m_kddwMain, const_cast<Workspace *>(this));
    json[QStringLiteral("active")] = m_activeLeaf ? m_activeLeaf->id() : QString{};
    if (!m_lastOpenFiles.isEmpty()) {
        QJsonArray files;
        for (const auto &f : m_lastOpenFiles) files.append(f);
        json[QStringLiteral("lastOpenFiles")] = files;
    }
    return json;
}
```

Add `#include "WorkspaceSerializer.h"` to `Workspace.cpp` (it's a libs/core/src/-relative include since the header is private).

- [ ] **Step 4: Replace `Workspace::deserialize` parse+materialize portion**

Find the existing `Workspace::deserialize` (lines 748-852). Replace lines 748-802 (everything from `setLayoutReady(false)` through the leaf-materialization loop) with:

```cpp
void Workspace::deserialize(const QJsonObject &json)
{
    setLayoutReady(false);

    qDeleteAll(m_leaves);
    m_leaves.clear();
    m_leavesById.clear();
    m_tabGroupOf.clear();
    m_stackedGroups.clear();
    m_activeLeaf = nullptr;
    m_undoHistory.clear();

    WorkspaceSerializer::fromJson(json, m_kddwMain, this);

    QString activeId = json[QStringLiteral("active")].toString();
    if (!activeId.isEmpty())
        m_activeLeaf = findLeafById(activeId);
    if (!m_activeLeaf && !m_leaves.isEmpty())
        m_activeLeaf = m_leaves.first();

    m_lastOpenFiles.clear();
    for (const auto &v : json[QStringLiteral("lastOpenFiles")].toArray())
        m_lastOpenFiles.append(v.toString());

    // Defer non-active, non-currentTab leaves so they don't materialize
    // their View until the user focuses them. The active leaf and each
    // tab group's currentTab leaf load eagerly. Per-group currentTab is
    // now real (was synthetic "first leaf in group" pre-consolidation).
    QHash<QString, WorkspaceLeaf *> currentInGroup;
    auto *layout = m_kddwMain ? m_kddwMain->layout() : nullptr;
    if (layout) {
        for (auto *grp : layout->groups()) {
            if (grp->dockWidgetCount() == 0) continue;
            int idx = grp->currentTabIndex();
            if (idx < 0 || idx >= grp->dockWidgetCount()) idx = 0;
            const QString uniqueName = grp->dockWidgetAt(idx)->uniqueName();
            const QString stripped = m_vaultId.isEmpty()
                ? uniqueName
                : (uniqueName.startsWith(m_vaultId + QChar(':'))
                   ? uniqueName.mid(m_vaultId.size() + 1)
                   : uniqueName);
            if (auto *leaf = findLeafById(stripped))
                currentInGroup.insert(m_tabGroupOf.value(leaf), leaf);
        }
    }
    for (auto *leaf : m_leaves) {
        if (leaf == m_activeLeaf) continue;
        const QString gid = m_tabGroupOf.value(leaf);
        if (currentInGroup.value(gid) == leaf) continue;

        auto state = leaf->getViewState();
        QString icon = state[QStringLiteral("icon")].toString();
        QString title = state[QStringLiteral("title")].toString();
        if (icon.isEmpty()) icon = QStringLiteral("document");
        if (title.isEmpty()) title = QStringLiteral("Untitled");
        leaf->setDeferred(true, icon, title);
    }
    if (m_activeLeaf && m_activeLeaf->isDeferred())
        m_activeLeaf->loadIfDeferred();

    Q_EMIT layoutChanged();
    setLayoutReady(true);

    if (auto *target = m_activeLeaf) {
        m_activeLeaf = nullptr;
        setActiveLeaf(target);
    }
}
```

The local `collectLeafObjects` helper at `Workspace.cpp:715-744` becomes unused. Delete it (and its enclosing anonymous namespace if it's the only occupant).

- [ ] **Step 5: Run all workspace-related tests**

```bash
cmake --build build -j 10 --target tst_workspace_serializer
cmake --build build -j 10 --target tst_workspace_serialize
cmake --build build -j 10 --target tst_workspace_deferred
cmake --build build -j 10 --target tst_workspace_session
cmake --build build -j 10 --target tst_workspace_integration
ctest --test-dir build -R '^tst_workspace_' --output-on-failure
```

Expected: all PASS. The most likely regression is in `tst_workspace_serialize` (older direct-Workspace serialize tests), which may have asserted exact output shape from the old flat writer; if so, those assertions now match the new recursive shape — adjust the expected JSON to match.

- [ ] **Step 6: Commit**

```bash
git add libs/core/src/Workspace.cpp tests/core/tst_workspace_serializer.cpp tests/core/tst_workspace_serialize.cpp
git commit -m "core/workspace: serialize/deserialize delegate to WorkspaceSerializer

Workspace::serialize and ::deserialize are now thin forwarders. The
post-load defer/active-leaf logic stays in Workspace; only parse+
materialize moves into the serializer. Closes punch-list P1 #1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 10: F14 — defer-set verification with real per-group currentTab

**Files:**
- Create: `tests/core/fixtures/workspace-obsidian/14-defer-set.json`
- Modify: `tests/core/tst_workspace_serializer.cpp` (add `fixture14`)

The defer-set logic in `Workspace::deserialize` (Task 9 step 4) now consults *real* per-group currentTab. Verify the deferred set is what we expect.

- [ ] **Step 1: Add fixture 14**

Create `tests/core/fixtures/workspace-obsidian/14-defer-set.json`:

```json
{
  "main": {
    "id": "f14rootaaaaaaaaa",
    "type": "split",
    "direction": "horizontal",
    "children": [
      {
        "id": "f14tabsAaaaaaaaa",
        "type": "tabs",
        "currentTab": 1,
        "children": [
          { "id": "f14A1aaaaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } },
          { "id": "f14A2aaaaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } },
          { "id": "f14A3aaaaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } }
        ]
      },
      {
        "id": "f14tabsBaaaaaaaa",
        "type": "tabs",
        "currentTab": 0,
        "children": [
          { "id": "f14B1aaaaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } },
          { "id": "f14B2aaaaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } },
          { "id": "f14B3aaaaaaaaaaa", "type": "leaf", "state": { "type": "empty", "state": {} } }
        ]
      }
    ]
  },
  "active": "f14A2aaaaaaaaaaa",
  "lastOpenFiles": []
}
```

- [ ] **Step 2: Add the test**

Append to `tst_workspace_serializer.cpp`. Add slot `void fixture14_deferSet_perGroupCurrentTabRespected();`.

```cpp
void TestWorkspaceSerializer::fixture14_deferSet_perGroupCurrentTabRespected()
{
    auto jsonIn = readFixture(QStringLiteral("14-defer-set.json"));
    QVERIFY(!jsonIn.isEmpty());

    Corbomite::ViewRegistry registry;
    registry.registerViewCreator(QStringLiteral("empty"),
        [](Corbomite::WorkspaceLeaf *leaf) -> Corbomite::View * {
            auto *v = new FakeView(leaf);
            v->setViewType(QStringLiteral("empty"));
            return v;
        });
    Corbomite::Workspace workspace(QStringLiteral("test-vault-14"), &registry);
    auto *kddwMain = qobject_cast<KDDockWidgets::QtWidgets::MainWindow *>(
        workspace.rootWidget());
    kddwMain->show();

    workspace.deserialize(jsonIn);

    // Expected: active leaf A2 not deferred. Group A's currentTab=1
    // = A2 (already not-deferred via active). Group B's currentTab=0
    // = B1 not deferred. The other four (A1, A3, B2, B3) are deferred.
    QCOMPARE(workspace.findLeafById(QStringLiteral("f14A2aaaaaaaaaaa"))->isDeferred(), false);
    QCOMPARE(workspace.findLeafById(QStringLiteral("f14B1aaaaaaaaaaa"))->isDeferred(), false);
    QCOMPARE(workspace.findLeafById(QStringLiteral("f14A1aaaaaaaaaaa"))->isDeferred(), true);
    QCOMPARE(workspace.findLeafById(QStringLiteral("f14A3aaaaaaaaaaa"))->isDeferred(), true);
    QCOMPARE(workspace.findLeafById(QStringLiteral("f14B2aaaaaaaaaaa"))->isDeferred(), true);
    QCOMPARE(workspace.findLeafById(QStringLiteral("f14B3aaaaaaaaaaa"))->isDeferred(), true);
}
```

- [ ] **Step 3: Run, confirm pass**

```bash
ctest --test-dir build -R '^tst_workspace_serializer$' --output-on-failure
```

Expected: PASS. The defer logic was rewritten in Task 9 to use `Layout::groups()` + `currentTabIndex()` so it now respects per-group currentTab.

- [ ] **Step 4: Commit**

```bash
git add tests/core/tst_workspace_serializer.cpp tests/core/fixtures/workspace-obsidian/14-defer-set.json
git commit -m "core/workspace: defer-set test for per-group currentTab

Verifies Workspace::deserialize defers exactly the four non-current
non-active leaves across two groups with distinct currentTabs.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 11: Build the full app, end-to-end smoke test

**Files:**
- (none modified; this is verification)

- [ ] **Step 1: Configure + build full app**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON
cmake --build build -j 10
```

Expected: clean build.

- [ ] **Step 2: Run all tests**

```bash
cd build && ctest --output-on-failure -j 10
```

Expected: all PASS. Note any unrelated regressions; investigate before proceeding.

- [ ] **Step 3: Smoke-test the app interactively**

```bash
./build/Corbomite
```

In the app:
1. Open a small dev vault (or create one).
2. Open three notes — drag the second tab to create a horizontal split, then drag the third tab to split the right pane vertically (creating a nested split).
3. In the second group, click the second tab to select it.
4. Pop out the third tab to a floating window.
5. Quit the app.
6. Inspect `<vault>/.obsidian/workspace.json` — verify nested splits, per-group `currentTab`, and `floating[].children[].x/y/width/height` are all preserved.
7. Relaunch with the same vault — verify layout restored exactly.

Document outcomes in the commit message of Task 12.

If anything is broken, file a punch-list entry and consider whether to roll back the consolidation; otherwise, proceed.

- [ ] **Step 4: Side-by-side Obsidian compatibility check (optional but recommended)**

If a real Obsidian-authored vault is available:
1. Open it in Obsidian, set up a multi-pane layout, close Obsidian.
2. Back up `.obsidian/workspace.json` to `workspace.json.before-corbomite`.
3. Open the same vault in Corbomite. Make no UI changes. Quit.
4. `diff` the two `workspace.json` files. Expected: shape preserved (nested splits, per-group currentTab, leaf state per leaf, popouts). The `left`/`right` subtrees pass through unchanged via `m_unknownRoot` (P1 #4 deferred).
5. Open back in Obsidian — confirm layout is intact.

Document any divergences.

- [ ] **Step 5: No commit (verification step)**

If Step 3 surfaced an issue, file follow-up bugs but proceed to Task 12 unless the regression is critical.

---

## Task 12: Punch-list updates, audit addendum, project-state refresh

**Files:**
- Modify: `docs/punch-list.md`
- Create: `docs/obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md`
- Modify: `docs/PROJECT-STATE.md`
- Modify: `docs/decisions-archive.md`
- Modify: `libs/core/include/corbomite/core/Workspace.h` (comment update)

- [ ] **Step 1: Mark P1 #1, #2, #3 as done**

Edit `docs/punch-list.md`. Change the three lines under "## P1 — Workspace.json round-trip fixes" from `- [ ]` to `- [x]`:

```markdown
- [x] [workspace] Consolidate `Workspace::serialize` and `WorkspaceSerializer::toJson` into one writer — see [workspace.md](audit-2026-04-26/workspace.md) §"Notable concerns / suspected bugs"
- [x] [workspace] Implement nested-split round-trip so opening + saving an Obsidian-authored layout doesn't degrade — see [workspace.md](audit-2026-04-26/workspace.md) §"Top gaps"
- [x] [workspace] Per-group `currentTab` instead of conflated global active-leaf-index — see [workspace.md](audit-2026-04-26/workspace.md) §"Top gaps"
```

- [ ] **Step 2: Add deferred-follow-up entries**

Append to the "P1" section of `docs/punch-list.md` (insert before the existing remaining unchecked P1 items, since these new items inherit P1 priority from their predecessors):

```markdown
- [ ] [workspace][settings] Decide `left`/`right`/`floating` JSON write-through policy in `SessionManager::doSave`. Three options: (A) drop on save; (B) pass through unmodified unless Corbomite mutated sidebar state (recommend); (C) translate to/from `CorbomiteMDI::Sidebar`. Spec: [`specs/2026-04-26-workspace-serializer-consolidation-design.md`](superpowers/specs/2026-04-26-workspace-serializer-consolidation-design.md) §Deferred follow-ups. Audit: [workspace.md](audit-2026-04-26/workspace.md) §"High severity" #4 + Layout JSON compat table row `left`/`right`
- [ ] [workspace] Repurpose `m_tabGroupOf` against live `Layout::groups()` (cache or eliminate). Update `Workspace.h:267-271` comment claiming KDDW has no public Group enumeration API. Audit: [workspace.md](audit-2026-04-26/workspace.md) §"High severity" #1 (now solvable via the public KDDW API documented in [`obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md`](obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md))
```

- [ ] **Step 3: Write the audit addendum**

Create `docs/obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md`:

```markdown
# Audit addendum — KDDW 2.4 public layout-enumeration API

**Corrects:** [`docs/obsidian-audit/domains/workspace.md`](../domains/workspace.md) (no direct line edit — addendum overrides) and `docs/audit-2026-04-26/workspace.md:14-15` and `libs/core/include/corbomite/core/Workspace.h:267-271`.

**Date:** 2026-04-26
**Source:** `kddockwidgets-qt6` 2.4.0-2 headers under `/usr/include/kddockwidgets-qt6/`.

The audit (and code comments echoing it) claim KDDW exposes no public Group/Frame enumeration API. This is **stale**; KDDW 2.4 ships the following as public interfaces:

| API | Header | Returns |
|---|---|---|
| `KDDockWidgets::Core::MainWindow::layout()` | `core/MainWindow.h:251` | `Core::Layout *` |
| `KDDockWidgets::Core::Layout::groups()` | `core/Layout.h:171` | `Vector<Core::Group *>` |
| `KDDockWidgets::Core::Layout::rootItem()` | `core/Layout.h:194` | `Core::ItemContainer *` |
| `KDDockWidgets::Core::Layout::dockWidgets()` | `core/Layout.h:174` | `Vector<Core::DockWidget *>` |
| `KDDockWidgets::Core::DropArea::groups()` | `core/DropArea.h:65` | `Vector<Core::Group *>` |
| `KDDockWidgets::Core::Group::currentDockWidget()` | `core/Group.h:89` | `DockWidget *` |
| `KDDockWidgets::Core::Group::currentTabIndex()` | `core/Group.h:184` | `int` |
| `KDDockWidgets::Core::Group::dockWidgets()` | `core/Group.h:107` | `Vector<DockWidget *>` |
| `KDDockWidgets::Core::Group::layoutItem()` | `core/Group.h:204` | `Core::Item *` (opaque to consumers; `Core::Item` is forward-declared, no public header) |
| `KDDockWidgets::Core::Group::serialize()` | `core/Group.h:52` | `LayoutSaver::Group` |
| `KDDockWidgets::LayoutSaver::serializeLayout()` | `LayoutSaver.h:84` | `QByteArray` (KDDW-shape JSON document) |

**Caveat:** `Core::Item` and `Core::ItemContainer` are forward-declared as types but their headers are not in the public include tree (kddockwidgets-qt6 2.4 does not ship `core/Item.h`). For external consumers, `Layout::groups()` is the canonical way to enumerate live groups; the *split topology* between groups is reconstructed by parsing `LayoutSaver::serializeLayout()` JSON output.

**Implications for Corbomite:**

1. The audit's "no public enumeration" caveat does not block recursive split-tree introspection.
2. `m_tabGroupOf` lag-after-drag (audit §"High severity" #1) is a follow-up addressable via `Layout::groups()` rather than an architectural blocker.
3. `WorkspaceSerializer` now uses `LayoutSaver::serializeLayout()` JSON as the source of truth for split topology, joined with `Workspace::findLeafById()` for per-leaf state — see [`docs/superpowers/specs/2026-04-26-workspace-serializer-consolidation-design.md`](../../superpowers/specs/2026-04-26-workspace-serializer-consolidation-design.md).

**KDDW JSON schema for `serializeLayout()`:** see [`docs/superpowers/specs/2026-04-26-kddw-layoutsaver-shape.md`](../../superpowers/specs/2026-04-26-kddw-layoutsaver-shape.md) (companion artifact captured during the consolidation work).
```

- [ ] **Step 4: Update `Workspace.h:267-271` comment**

In `libs/core/include/corbomite/core/Workspace.h`, replace the comment block at lines 264-270 with:

```cpp
    // Leaf indexes. `m_leaves` is insertion-ordered for stable iteration;
    // `m_leavesById` is the O(1) findLeafById index;
    // `m_tabGroupOf` carries the opaque tab-group identifier per leaf —
    // KDDW 2.4 exposes Layout::groups() + Group::dockWidgets() for live
    // enumeration; m_tabGroupOf is currently a cache (lags user-initiated
    // drag-tab-to-other-group). Repurpose-against-Layout::groups follow-up
    // tracked in docs/punch-list.md. See
    // docs/obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md
    // for the corrected KDDW API surface.
```

- [ ] **Step 5: Refresh PROJECT-STATE current focus**

Edit `docs/PROJECT-STATE.md` "Current focus" section. Replace the existing top entry with:

```markdown
**P1 workspace serializer consolidation done** (2026-04-26). Items #1–#3 (consolidation, nested-split round-trip, per-group currentTab) closed via the hybrid KDDW+Workspace writer at `libs/core/src/WorkspaceSerializer.cpp`. P1 #4 (`m_unknownRoot` left/right write-through) and the `m_tabGroupOf` lag-after-drag follow-up remain in P1 punch-list. Next pick from top of P1 unless redirected.
```

- [ ] **Step 6: Append to decisions-archive**

Append to `docs/decisions-archive.md`:

```markdown
## 2026-04-26 — Workspace serializer consolidation (P1 #1, #2, #3)

`Workspace::serialize` and `WorkspaceSerializer::toJson` consolidated into
one hybrid writer. KDDW provides split topology + per-group currentTab via
`LayoutSaver::serializeLayout()` JSON + `Layout::groups()`; Workspace
provides leaf payload via `findLeafById()` + `WorkspaceLeaf::serialize()`.
Sidecar maps (`leafSidecar`, `stackedSidecar`) retained as test-only
fallback when `workspace=nullptr`; production-path leaf-unknown-keys live
on `WorkspaceLeaf::m_unknownLeafKeys` and stacked lives on
`Workspace::m_stackedGroups`.

Ten test fixtures (01–14, with 04b/08b duplicates for the
workspace-non-null path) cover the new contract.

Audit addendum filed:
[`docs/obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md`](obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md)
correcting the audit's stale claim that KDDW lacks public Group
enumeration.

P1 #4 (`m_unknownRoot` left/right write-through) deferred to a separate
punch-list entry — sidedock modeling is out of scope for serializer
fidelity. The `m_tabGroupOf` lag-after-drag follow-up similarly punted.

Spec: [`docs/superpowers/specs/2026-04-26-workspace-serializer-consolidation-design.md`](superpowers/specs/2026-04-26-workspace-serializer-consolidation-design.md)
Plan: [`docs/superpowers/plans/2026-04-26-workspace-serializer-consolidation.md`](superpowers/plans/2026-04-26-workspace-serializer-consolidation.md)
```

- [ ] **Step 7: Commit**

```bash
git add docs/punch-list.md docs/obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md docs/PROJECT-STATE.md docs/decisions-archive.md libs/core/include/corbomite/core/Workspace.h
git commit -m "docs: close P1 #1-#3, file KDDW-enumeration addendum, refresh state

P1 #1 (consolidation), #2 (nested-split round-trip), #3 (per-group
currentTab) marked done. New follow-up entries for P1 #4 (deferred:
m_unknownRoot left/right write-through policy) and the m_tabGroupOf
lag-after-drag refactor enabled by the corrected KDDW API surface.

Audit addendum documents Layout::groups() / rootItem() / Group::
currentTabIndex() / LayoutSaver::serializeLayout() as the public KDDW
APIs Corbomite now consumes — corrects workspace.md:14-15 and
Workspace.h:267-271 stale claims. Comment in Workspace.h updated to
point at the addendum.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Self-review

Spec coverage:
- §"Architecture / one writer / hybrid input" → Tasks 2, 3, 4, 5, 6, 7, 8, 9
- §"Audit correction" → Task 12 (addendum + Workspace.h comment)
- §"Module structure (option B)" → Tasks 2, 9
- §"Data flow / serialize" → Tasks 2, 3, 4, 6, 7, 8
- §"Data flow / deserialize" → Tasks 5, 9
- §"Test contract — F10–F14" → Tasks 5 (F12), 3 (F11), 4 (F13), 10 (F14); F10 absorbed into Task 5/6 fixture12 round-trip; F08b in Task 7; F04b in Task 8; identity test in Task 9
- §"Failure modes" → covered inline in Task 2 (orphan dock widget, malformed `main`), Task 7 (unknown leaf keys field)
- §"Deferred follow-ups" → Task 12 punch-list entries
- §"Acceptance criteria" → Task 9 (identity test); Task 11 (end-to-end smoke + Obsidian round-trip)

Type consistency: `WorkspaceSerializer::toJson(MainWindow*, Workspace*)` and `fromJson(json, MainWindow*, Workspace*)` signatures unchanged from current header. New helpers consistently named. `Workspace::placeLeafAt` / `dropLeafIdMapping` / `insertLeafIdMapping` / `tabGroupIdOf` / `isTabGroupStacked` / `setTabGroupStacked` referenced consistently across Tasks 5, 8.

Placeholders: Task 2 has `FIELDS_FROM_SCHEMA` markers and a skeleton implementation that depends on Task 1's research output. This is acknowledged research-first work — the implementer (you) will fill in concrete field names against the dumped schema. Not a plan failure; flagged explicitly.

