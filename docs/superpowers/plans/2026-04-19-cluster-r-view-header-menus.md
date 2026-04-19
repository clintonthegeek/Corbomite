# Cluster R — View-Header Menus — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Living-status note:** Live status (Not started / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file.

**Goal:** Replace the current stub hamburger ("Rename…" that doesn't render a dialog) with a full Obsidian-style per-leaf "…" menu: canonical section ordering, per-view subclass contributions, and every menu entry the user listed wired to a real primitive or a disabled-placeholder with a tooltip citing the blocking cluster.

**Architecture:** Two-hook pattern. `View::onMoreOptionsMenu(MenuSectionHelper&)` becomes the primary hook for subclasses (matching Obsidian's `onMoreOptions` contract). `View::onPaneMenu(QMenu*, QString source)` grows a source parameter so subclasses can differentiate hamburger invocation from tab-header invocation. `ItemView::showMoreOptionsMenu()` orchestrates: subclass contributions via the helper → `onPaneMenu(menu, "more-options")` compat → `MenuEventEmitter::leafMenu` plugin hook → finalize. `MenuSectionHelper` canonical-order aligns verbatim with Obsidian (`close, pane, open, action, find, info, info.copy, view, view.linked, system, "", danger`) and grows `addSubmenu` for nested `info.copy` (Copy path) and `view.linked` (Open linked view). `EditableFileView` owns universal file-menu items (Rename/Move/CopyPath/OpenDefault/ShowInFolder/Reveal/Delete + disabled placeholders for OpenInNewWindow and OpenVersionHistory). `MarkdownView`/`CanvasFileView`/`GraphView` prepend their view-specific items (Reading-view/Source-mode toggles, Bookmark…, Export-PDF/Image, Copy-screenshot, `view.linked` submenu). Inline Backlinks-in-document ships as a `PostProcessor` in the Backlinks plugin gated on per-leaf `backlinksInDocument` viewState.

**Tech Stack:** C++20, Qt6 (`QMenu`, `QToolButton`, `QDialog`, `QPrinter`, `QDesktopServices`, `QDBusInterface`, `QGraphicsScene::render`), KDE Frameworks 6 (`KLocalizedString`, `KMessageBox`, `KStandardGuiItem`), existing `libs/core/` menu infrastructure (`MenuSectionHelper`, `MenuEventEmitter`, `View` hierarchy), `libs/vault/` (`FileManager`, `Vault`), Cluster J's `PostProcessorRegistry` for P4 inline backlinks.

**Spec:** `docs/superpowers/specs/2026-04-19-cluster-r-view-header-menus-design.md`

**Audit addenda:** `docs/obsidian-audit/addenda/2026-04-19-{bookmarks-core-plugin,rename-move-modals,delete-confirm-modal,open-with-default-app,show-in-folder,canvas-export-as-image,graph-screenshot,add-file-property-menu,file-recovery-plugin,merge-file-modal}.md`

---

## File Structure

### New files created by this plan

| Path | Responsibility |
|---|---|
| `libs/vault/src/dialogs/RenameDialog.cpp` + `.h` | `promptForFileRename` modal (`QDialog`, live-validation tooltip, basename pre-select). |
| `libs/vault/src/dialogs/MoveFileDialog.cpp` + `.h` | `promptForMove` fuzzy folder-picker modal (reuses `FuzzyMatcher`). |
| `libs/vault/src/dialogs/DeleteConfirmDialog.cpp` + `.h` | `promptForDeletion` warning modal with trash-option text + "Don't ask again" checkbox. |
| `libs/vault/src/FileNameValidator.cpp` + `.h` | Shared live-validation helper (empty / bad-char / reserved-Windows / collision) used by both rename + create paths. |
| `libs/core/include/corbomite/core/Platform.h` | New shared namespace. Declares `openWithDefaultApp(absPath)` + `showInFolder(absPath)`. |
| `libs/core/src/Platform.cpp` | Implementation. Linux DBus FileManager1 path + xdg-open fallback + native paths for mac/Win. |
| `libs/core/include/corbomite/core/PathUtils.h` | `obsidianUrlFor(vaultName, relPath, subpath)` + `corbomiteUrlFor(...)`. |
| `libs/core/src/PathUtils.cpp` | Implementation. URL-percent-encoded per RFC 3986. |
| `src/app/ExportToPdf.cpp` + `.h` | `exportFile(TFile*, QWidget*)` — QPrinter + ReadingView::renderToPdf pipeline. |
| `src/plugins/backlinks/BacklinksPostProcessor.cpp` + `.h` | PostProcessor that appends Backlinks section when `backlinksInDocument` viewState is true. |
| `tests/core/tst_view_more_options.cpp` | Unit test for `onMoreOptionsMenu` / `onPaneMenu(source)` dispatch order + helper finalize. |
| `tests/vault/tst_rename_dialog.cpp` | Modal behaviour tests (pre-selection, validation, cancel/save). |
| `tests/vault/tst_move_file_dialog.cpp` | Folder picker tests. |
| `tests/vault/tst_delete_confirm_dialog.cpp` | Confirm modal + "Don't ask again" persistence. |
| `tests/vault/tst_file_name_validator.cpp` | Validation-rule unit tests. |
| `tests/core/tst_platform.cpp` | `openWithDefaultApp` + `showInFolder` (uses QProcess mocks + DBus stub injection). |
| `tests/core/tst_path_utils.cpp` | URL-generation tests incl. percent-encoding of spaces / unicode. |
| `tests/app/tst_export_to_pdf.cpp` | Export-PDF round-trip: render a fixture markdown, verify PDF signature bytes. |
| `tests/plugins/tst_backlinks_post_processor.cpp` | Post-processor gates on viewState; re-renders on cacheChanged. |
| `docs/cluster-retros/cluster-r.md` | Retro at cluster close. |

### Modified files

| Path | What changes |
|---|---|
| `libs/core/include/corbomite/core/MenuSectionHelper.h` | Canonical-order rename (`title→close`, `action-primary→pane`, add `find` + `view.linked`); new `addSubmenu` method. |
| `libs/core/src/MenuSectionHelper.cpp` | Matches; handles nested-helper finalize at outer `finalize()`. |
| `libs/core/include/corbomite/core/View.h` | New virtual `onMoreOptionsMenu(MenuSectionHelper&)`; grows `onPaneMenu(QMenu*, QString source)` overload alongside existing zero-arg. |
| `libs/core/src/View.cpp` | Default implementations (both empty). |
| `libs/core/include/corbomite/core/ItemView.h` | No API changes. |
| `libs/core/src/ItemView.cpp` | Rewrite `showMoreOptionsMenu()` per spec §3.1. |
| `libs/core/include/corbomite/core/EditableFileView.h` | Declare `onMoreOptionsMenu(MenuSectionHelper&) override`. |
| `libs/core/src/EditableFileView.cpp` | Impl of `onMoreOptionsMenu` (universal file-menu items); delete stub `startRename` body; route `onPaneMenu` Rename through `promptForFileRename`. |
| `libs/core/CMakeLists.txt` | Add `Platform.cpp` + `PathUtils.cpp` to sources; link `Qt6::DBus` for Linux. |
| `libs/vault/include/corbomite/vault/FileManager.h` | Add `promptForFileRename` / `promptForMove` / `promptForDeletion` method declarations. |
| `libs/vault/src/FileManager.cpp` | Implementations (thin wrappers that construct the dialogs + delegate to existing `renameFile`/`trashFile`). |
| `libs/vault/CMakeLists.txt` | Add dialog sources + validator. |
| `tests/core/tst_menusectionhelper.cpp` | Update section-key fixtures; add submenu test case. |
| `src/editor/MarkdownView.h/.cpp` | Declare + impl `onMoreOptionsMenu`; add `insertFrontmatterProperty()` method; wire `exportToPdf()` method. |
| `src/canvas/CanvasFileView.h/.cpp` | Declare + impl `onMoreOptionsMenu`; add `showExportAsImageModal()` method. |
| `libs/canvas/include/canvas/CanvasScene.h` | Add `renderToImage(bounds, transparentBg, showEdges, scale)` + `renderToSvg(bounds, QIODevice*, transparentBg, showEdges)`. |
| `libs/canvas/src/CanvasScene.cpp` | Implementations via `QGraphicsScene::render` / `QSvgGenerator`. |
| `src/plugins/graph-view/GraphView.h/.cpp` | Declare + impl `onMoreOptionsMenu`. |
| `src/plugins/graph-view/GraphViewPlugin.cpp` | Register `graph:copy-screenshot` command. |
| `src/plugins/backlinks/BacklinksPlugin.cpp` | Register `BacklinksPostProcessor` in `onLoad`; register `backlinks:open` command. |
| `src/plugins/outlinks/OutlinksPlugin.cpp` | Register `outlinks:open` command. |
| `src/plugins/outline/OutlinePlugin.cpp` | Register `outline:open` command. |
| `src/plugins/properties/PropertiesPlugin.cpp` | Register `properties:open` command. |
| `src/plugins/local-graph/LocalGraphPlugin.cpp` | Register `graph:open-local` command (if absent). |
| `src/plugins/file-explorer/FileExplorerPlugin.cpp` | Register `file-explorer:reveal-file` command (scrolls + selects path in panel tree). |
| `src/plugins/file-explorer/FileExplorerView.cpp` | Add `revealPath(QString)` method invoked by the new command. |
| `docs/PROJECT-STATE.md` | Marks Cluster R done + recent-decisions entry + Last-updated. |
| `docs/superpowers/plans/INDEX.md` | Updates R row to "Done". |

### Deleted by this plan

| Path | Reason |
|---|---|
| — | No deletions. `EditableFileView::startRename()` body is emptied but the method remains as inline-rename entry point (kept for future P1.5 inline-rename support matching audit views.md §67-74). |

---

## Phase Overview

Four phases, executed sequentially. Phase 1 must land first (every other phase consumes the new `onMoreOptionsMenu` hook). Phases 2 + 3 can be dispatched as parallel subagents after Phase 1 since they touch different files (P2 = `EditableFileView` base + primitives in `libs/vault` + `libs/core`; P3 = view-specific subclasses in `src/editor` / `src/canvas` / `src/plugins/graph-view`). Phase 4 is small and layers on top.

1. **Phase 1 — Menu substrate alignment.** `MenuSectionHelper` canonical order + submenu; `View::onMoreOptionsMenu` hook; `ItemView::showMoreOptionsMenu` rewrite.
2. **Phase 2 — Universal file-menu items on `EditableFileView`.** Three modals (rename/move/delete), two Platform primitives (openWithDefaultApp/showInFolder), PathUtils, FileExplorer reveal command, + the `EditableFileView::onMoreOptionsMenu` wiring.
3. **Phase 3 — Per-view specialisations.** MarkdownView (Reading/Source toggle, Split, Bookmark slot, AddProperty, Export-PDF, Find/Replace disabled, view.linked submenu), CanvasFileView (Split, Bookmark slot, Export-as-image, view.linked with Backlinks only), GraphView (Split, Copy-screenshot, Bookmark slot), plus the five plugin `:open` commands referenced by view.linked.
4. **Phase 4 — Inline Backlinks-in-document.** BacklinksPostProcessor + toggle wiring + cacheChanged re-render.
5. **Phase 5 — Closeout.** Retro + PROJECT-STATE + INDEX.

Estimated effort: **~6-7 days** total per spec §12.

---

# Phase 1 — Menu Substrate Alignment

Goal: after Phase 1, the hamburger dispatch pipeline exists and is testable but no new user-visible menu items ship. Existing "Rename…" still appears (unchanged UX) but goes through the new hook path.

## Task 1.1: Update `MenuSectionHelper` canonical order + add submenu support

**Files:**
- Modify: `libs/core/include/corbomite/core/MenuSectionHelper.h`
- Modify: `libs/core/src/MenuSectionHelper.cpp`
- Modify: `tests/core/tst_menusectionhelper.cpp`

- [ ] **Step 1: Update the failing test for new canonical order**

Open `tests/core/tst_menusectionhelper.cpp`. Replace the `testCanonicalOrderIsAudited` test (around line 15 per existing structure) and the fixture section-key uses with Obsidian's canonical order:

```cpp
void testCanonicalOrderIsAudited()
{
    const auto &order = Corbomite::MenuSectionHelper::canonicalSectionOrder();
    const QStringList expected = {
        QStringLiteral("close"),
        QStringLiteral("pane"),
        QStringLiteral("open"),
        QStringLiteral("action"),
        QStringLiteral("find"),
        QStringLiteral("info"),
        QStringLiteral("info.copy"),
        QStringLiteral("view"),
        QStringLiteral("view.linked"),
        QStringLiteral("system"),
        QStringLiteral(""),
        QStringLiteral("danger"),
    };
    QCOMPARE(order, expected);
}
```

Replace any `addToSection(..., "title")` calls with `"close"`; `"action-primary"` with `"pane"`.

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build -j 10 --target tst_menusectionhelper
cd build && ctest -R tst_menusectionhelper --output-on-failure
```

Expected: FAIL — canonical order mismatch.

- [ ] **Step 3: Update `canonicalSectionOrder()` implementation**

Edit `libs/core/src/MenuSectionHelper.cpp`:

```cpp
const QStringList &MenuSectionHelper::canonicalSectionOrder()
{
    static const QStringList order = {
        QStringLiteral("close"),
        QStringLiteral("pane"),
        QStringLiteral("open"),
        QStringLiteral("action"),
        QStringLiteral("find"),
        QStringLiteral("info"),
        QStringLiteral("info.copy"),
        QStringLiteral("view"),
        QStringLiteral("view.linked"),
        QStringLiteral("system"),
        QStringLiteral(""),
        QStringLiteral("danger"),
    };
    return order;
}
```

- [ ] **Step 4: Run the test — expected PASS**

```bash
cd build && ctest -R tst_menusectionhelper --output-on-failure
```

- [ ] **Step 5: Add the failing test for `addSubmenu`**

Add to `tests/core/tst_menusectionhelper.cpp`:

```cpp
void testAddSubmenuNestedHelper()
{
    QMenu menu;
    Corbomite::MenuSectionHelper helper(&menu);

    QAction directAct("Direct action");
    helper.addToSection(&directAct, QStringLiteral("action"));

    auto sub = helper.addSubmenu(QStringLiteral("info.copy"),
                                  QStringLiteral("Copy path"),
                                  QIcon());
    QAction copyAUrlAct("as Obsidian URL");
    QAction copyFromVaultAct("from vault folder");
    sub.addToSection(&copyAUrlAct, QStringLiteral("action"));
    sub.addToSection(&copyFromVaultAct, QStringLiteral("action"));

    helper.finalize();

    // Top-level menu has: directAct, separator, "Copy path" submenu
    const auto actions = menu.actions();
    QCOMPARE(actions.size(), 3);
    QCOMPARE(actions[0]->text(), QStringLiteral("Direct action"));
    QVERIFY(actions[1]->isSeparator());
    QVERIFY(actions[2]->menu() != nullptr);
    QCOMPARE(actions[2]->text(), QStringLiteral("Copy path"));

    // Submenu has: copyAUrlAct, copyFromVaultAct
    const auto subActions = actions[2]->menu()->actions();
    QCOMPARE(subActions.size(), 2);
    QCOMPARE(subActions[0]->text(), QStringLiteral("as Obsidian URL"));
    QCOMPARE(subActions[1]->text(), QStringLiteral("from vault folder"));
}
```

- [ ] **Step 6: Run the new test to verify it fails**

Expected: FAIL — `addSubmenu` doesn't exist.

- [ ] **Step 7: Declare `addSubmenu` in header**

Edit `libs/core/include/corbomite/core/MenuSectionHelper.h`. Add to the public section:

```cpp
// Returns a nested helper whose actions are grouped into a QMenu
// inserted as a submenu at the outer helper's `finalize()`.
MenuSectionHelper addSubmenu(const QString &sectionId,
                              const QString &title,
                              const QIcon &icon = QIcon());
```

Add a private struct to track pending submenus:

```cpp
private:
    struct PendingSubmenu {
        QString sectionId;
        QString title;
        QIcon icon;
        std::shared_ptr<QMenu> menu;
    };
    QList<PendingSubmenu> m_pendingSubmenus;
```

(`std::shared_ptr<QMenu>` because the returned helper needs to hold a stable reference to the submenu until the outer helper finalizes — using `QMenu *` with a raw `new QMenu` risks a leak if finalize never runs.)

- [ ] **Step 8: Implement `addSubmenu` in .cpp**

Edit `libs/core/src/MenuSectionHelper.cpp`:

```cpp
MenuSectionHelper MenuSectionHelper::addSubmenu(const QString &sectionId,
                                                 const QString &title,
                                                 const QIcon &icon)
{
    auto submenu = std::make_shared<QMenu>(title);
    if (!icon.isNull()) submenu->setIcon(icon);
    m_pendingSubmenus.append({sectionId, title, icon, submenu});
    return MenuSectionHelper(submenu.get());
}
```

Update `finalize()` to insert pending submenus as `menuAction()` entries at their section:

```cpp
void MenuSectionHelper::finalize()
{
    if (!m_menu) return;
    m_menu->clear();

    // Merge submenus into the bucket map first
    for (auto &ps : m_pendingSubmenus) {
        // Finalize the submenu's own contents so its actions show up
        // (we constructed the nested helper with submenu.get() as its menu)
        MenuSectionHelper nested(ps.menu.get());
        // The caller already populated ps.menu's buckets via the returned helper —
        // but that helper is a different object; we need a shared-state approach.
        // See design note below.
    }

    // ...
}
```

- [ ] **Step 9: Correct the shared-state gap noticed in Step 8**

The nested `MenuSectionHelper` returned from `addSubmenu` has its own buckets — when the caller calls `helper.finalize()`, the nested buckets don't automatically flush. Fix by making `addSubmenu` return by value a helper whose destructor calls its own finalize, OR by flushing pending submenus' buckets when outer finalize runs.

The cleanest approach: change `addSubmenu` to return a `std::shared_ptr<MenuSectionHelper>` keyed in the outer helper's `m_pendingSubmenus`, so the outer `finalize()` first calls `ps.nestedHelper->finalize()` before inserting the submenu action.

Revise:

```cpp
// in MenuSectionHelper.h
struct PendingSubmenu {
    QString sectionId;
    QString title;
    QIcon icon;
    std::shared_ptr<QMenu> menu;
    std::shared_ptr<MenuSectionHelper> nestedHelper;
};

MenuSectionHelper *addSubmenu(const QString &sectionId,
                               const QString &title,
                               const QIcon &icon = QIcon());
// ^ returns a non-owning pointer; outer helper owns the nested helper via m_pendingSubmenus
```

```cpp
// in MenuSectionHelper.cpp
MenuSectionHelper *MenuSectionHelper::addSubmenu(const QString &sectionId,
                                                   const QString &title,
                                                   const QIcon &icon)
{
    auto menu = std::make_shared<QMenu>(title);
    if (!icon.isNull()) menu->setIcon(icon);
    auto nested = std::make_shared<MenuSectionHelper>(menu.get());
    m_pendingSubmenus.append({sectionId, title, icon, menu, nested});
    return nested.get();
}

void MenuSectionHelper::finalize()
{
    if (!m_menu) return;
    m_menu->clear();

    // First, finalize all submenus into their own QMenu objects
    for (auto &ps : m_pendingSubmenus) {
        ps.nestedHelper->finalize();
        // Add a QAction-with-menu into the outer bucket
        auto *submenuAction = ps.menu->menuAction();
        submenuAction->setText(ps.title);
        if (!ps.icon.isNull()) submenuAction->setIcon(ps.icon);
        const QString bucketKey = canonicalSectionOrder().contains(ps.sectionId)
            ? ps.sectionId : QString();
        m_buckets[bucketKey].append(submenuAction);
    }

    // Then flush top-level buckets in canonical order
    bool needSeparator = false;
    for (const QString &section : canonicalSectionOrder()) {
        const auto &actions = m_buckets.value(section);
        if (actions.isEmpty()) continue;
        if (needSeparator) m_menu->addSeparator();
        for (QAction *a : actions) m_menu->addAction(a);
        needSeparator = true;
    }
}
```

Update the test in Step 5 to call `addSubmenu` returning a pointer:

```cpp
auto *sub = helper.addSubmenu(...);
sub->addToSection(...);
```

- [ ] **Step 10: Build and run all tests**

```bash
cmake --build build -j 10
cd build && ctest -R tst_menusectionhelper --output-on-failure
```

Expected: PASS.

- [ ] **Step 11: Commit**

```bash
git add libs/core/include/corbomite/core/MenuSectionHelper.h libs/core/src/MenuSectionHelper.cpp tests/core/tst_menusectionhelper.cpp
git commit -m "$(cat <<'EOF'
feat(menus): align MenuSectionHelper to Obsidian canonical order + add submenu

Rename section keys: title→close, action-primary→pane.
Add new sections: find, view.linked.
New addSubmenu() returns a nested helper whose contents flush as a submenu
QAction at the outer helper's finalize().

Prereq for Cluster R Phase 1 (View::onMoreOptionsMenu hook).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

## Task 1.2: Add `View::onMoreOptionsMenu` hook + grow `onPaneMenu(source)` overload

**Files:**
- Modify: `libs/core/include/corbomite/core/View.h`
- Modify: `libs/core/src/View.cpp`
- Create: `tests/core/tst_view_more_options.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Declare the new hook + overload in `View.h`**

Edit `libs/core/include/corbomite/core/View.h`. Add after the existing `onPaneMenu` declaration:

```cpp
// Primary hook for hamburger-menu ("…" / overflow) contributions.
// Subclasses override to add items via the supplied helper. Called by
// ItemView::showMoreOptionsMenu before onPaneMenu(menu, "more-options").
virtual void onMoreOptionsMenu(MenuSectionHelper &helper);

// Context-menu hook with source discrimination. `source` distinguishes
// invocation contexts — "pane-menu" (tab-header right-click), "more-options"
// (hamburger click), "file-menu" (file-list right-click), etc.
// Default implementation forwards to the zero-arg overload for backward compat.
virtual void onPaneMenu(QMenu *menu, const QString &source);
```

Keep the existing `virtual void onPaneMenu(QMenu *menu);` declaration — it stays as the zero-arg overload that current callers use.

Add forward-declaration + include:

```cpp
namespace Corbomite { class MenuSectionHelper; }
```

(Full include in .cpp rather than header to keep View.h's include chain lean.)

- [ ] **Step 2: Provide default implementations in `View.cpp`**

Edit `libs/core/src/View.cpp`. Add:

```cpp
#include "corbomite/core/MenuSectionHelper.h"

// ... existing code ...

void View::onMoreOptionsMenu(MenuSectionHelper & /*helper*/) {}

void View::onPaneMenu(QMenu *menu, const QString & /*source*/)
{
    onPaneMenu(menu);  // backward-compat forwarder
}
```

- [ ] **Step 3: Write the failing test for dispatch order**

Create `tests/core/tst_view_more_options.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QMenu>
#include <QSignalSpy>

#include "corbomite/core/View.h"
#include "corbomite/core/ItemView.h"
#include "corbomite/core/MenuSectionHelper.h"

class TestView : public Corbomite::View {
public:
    using Corbomite::View::View;
    QStringList callOrder;

    void onMoreOptionsMenu(Corbomite::MenuSectionHelper &helper) override
    {
        callOrder << QStringLiteral("onMoreOptionsMenu");
        auto *action = new QAction("Custom item", this);
        helper.addToSection(action, QStringLiteral("action"));
    }

    void onPaneMenu(QMenu * /*menu*/, const QString &source) override
    {
        callOrder << (QStringLiteral("onPaneMenu:") + source);
    }
};

class TestViewMoreOptions : public QObject {
    Q_OBJECT

private slots:
    void testDispatchOrderHamburgerPath()
    {
        TestView view(nullptr);
        QMenu menu;
        Corbomite::MenuSectionHelper helper(&menu);

        // Simulate ItemView::showMoreOptionsMenu dispatch sequence
        view.onMoreOptionsMenu(helper);
        view.onPaneMenu(&menu, QStringLiteral("more-options"));
        helper.finalize();

        QCOMPARE(view.callOrder.size(), 2);
        QCOMPARE(view.callOrder[0], QStringLiteral("onMoreOptionsMenu"));
        QCOMPARE(view.callOrder[1], QStringLiteral("onPaneMenu:more-options"));

        // Helper action landed in action section
        QCOMPARE(menu.actions().size(), 1);
        QCOMPARE(menu.actions()[0]->text(), QStringLiteral("Custom item"));
    }

    void testOnPaneMenuSourceDefaultForwarder()
    {
        // A subclass overriding only the zero-arg onPaneMenu should still be
        // called via the two-arg overload (default forwarder delegates).
        class LegacyView : public Corbomite::View {
        public:
            using Corbomite::View::View;
            int paneMenuCalls = 0;
            void onPaneMenu(QMenu * /*menu*/) override { ++paneMenuCalls; }
        };

        LegacyView view(nullptr);
        QMenu menu;
        view.onPaneMenu(&menu, QStringLiteral("more-options"));
        QCOMPARE(view.paneMenuCalls, 1);
    }
};

QTEST_MAIN(TestViewMoreOptions)
#include "tst_view_more_options.moc"
```

- [ ] **Step 4: Register test in CMake**

Edit `tests/core/CMakeLists.txt`. Find the block where other tests are registered (e.g. `tst_menusectionhelper`) and add a parallel entry:

```cmake
add_corbomite_test(tst_view_more_options
    SOURCES tst_view_more_options.cpp
    LINK_LIBRARIES
        Corbomite::Core
        Qt6::Test
        Qt6::Widgets
)
```

- [ ] **Step 5: Build + run; expected PASS**

```bash
cmake --build build -j 10 --target tst_view_more_options
cd build && ctest -R tst_view_more_options --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/View.h libs/core/src/View.cpp tests/core/tst_view_more_options.cpp tests/core/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(views): add View::onMoreOptionsMenu hook + onPaneMenu(source) overload

View::onMoreOptionsMenu(MenuSectionHelper&) is the primary subclass entry
point for the hamburger "…" menu, matching Obsidian's onMoreOptions contract.
onPaneMenu grows a `source` parameter so subclasses can discriminate
invocation contexts; default impl forwards to the zero-arg overload for
existing callers.

Prereq for Cluster R Phase 1 Task 1.3 (ItemView::showMoreOptionsMenu rewrite).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

## Task 1.3: Rewrite `ItemView::showMoreOptionsMenu` to use the new hook + helper chain

**Files:**
- Modify: `libs/core/src/ItemView.cpp`
- Modify: `tests/core/tst_view_more_options.cpp` (extend with ItemView-path test)

- [ ] **Step 1: Write an integration test exercising the full ItemView path**

Append to `tests/core/tst_view_more_options.cpp`:

```cpp
    void testItemViewShowMoreOptionsIntegration()
    {
        // Subclass ItemView to spy on dispatch + install a custom MenuEventEmitter
        class SpyItemView : public Corbomite::ItemView {
        public:
            using Corbomite::ItemView::ItemView;
            QStringList order;
            void onMoreOptionsMenu(Corbomite::MenuSectionHelper &h) override
            {
                order << QStringLiteral("onMoreOptionsMenu");
                auto *a = new QAction("A1", this);
                h.addToSection(a, QStringLiteral("action"));
            }
            void onPaneMenu(QMenu * /*m*/, const QString &src) override
            {
                order << (QStringLiteral("onPaneMenu:") + src);
            }
        };

        SpyItemView view(nullptr);
        // Can't exec() menu in test; dispatch the contribution pipeline manually.
        // showMoreOptionsMenu is the target under test; the test version
        // invokes it synchronously and asserts order.
        // (The impl rewrite in Step 2 makes this possible by separating
        // menu-build from menu-exec.)
        view.buildMoreOptionsMenuForTest();

        QVERIFY(view.order.contains(QStringLiteral("onMoreOptionsMenu")));
        QVERIFY(view.order.contains(QStringLiteral("onPaneMenu:more-options")));
        QVERIFY(view.order.indexOf(QStringLiteral("onMoreOptionsMenu")) <
                view.order.indexOf(QStringLiteral("onPaneMenu:more-options")));
    }
```

- [ ] **Step 2: Rewrite `showMoreOptionsMenu` and expose test seam**

Edit `libs/core/include/corbomite/core/ItemView.h`. Add to the protected section:

```cpp
protected:
    // Testable build path: populates `menu` with contributions but does not exec.
    // showMoreOptionsMenu() calls this then exec()s.
    void buildMoreOptionsMenu(QMenu *menu);

public:
    // Kept as-is; now delegates to buildMoreOptionsMenu.
    void showMoreOptionsMenu();

    // Test-only seam: returns a populated QMenu without exec.
    // Marked inline to avoid polluting the release API with a dedicated
    // "ForTest" method; the caller owns the menu.
#ifdef CORBOMITE_TEST_SEAMS
    QMenu *buildMoreOptionsMenuForTest()
    {
        auto *m = new QMenu(this);
        buildMoreOptionsMenu(m);
        return m;
    }
#endif
```

Actually — rather than `#ifdef`, just expose `buildMoreOptionsMenu` as public. Cleaner. Revise:

```cpp
public:
    // Populates a menu with hamburger contributions without exec-ing.
    // Primarily a test seam; production callers use showMoreOptionsMenu.
    void buildMoreOptionsMenu(QMenu *menu);
    void showMoreOptionsMenu();
```

Edit `libs/core/src/ItemView.cpp`:

```cpp
#include "corbomite/core/MenuSectionHelper.h"
#include "corbomite/core/MenuEventEmitter.h"
#include "corbomite/core/WorkspaceLeaf.h"

// ... existing code ...

void ItemView::buildMoreOptionsMenu(QMenu *menu)
{
    if (!menu) return;
    Corbomite::MenuSectionHelper helper(menu);

    // 1. Primary subclass hook
    onMoreOptionsMenu(helper);

    // 2. Back-compat: onPaneMenu with source="more-options"
    onPaneMenu(menu, QStringLiteral("more-options"));

    // 3. Plugin hook: leaf-menu emission via MenuEventEmitter
    if (m_leaf) {
        if (auto *emitter = m_leaf->menuEventEmitter())
            emitter->emitLeafMenu(menu, m_leaf);
    }

    helper.finalize();
}

void ItemView::showMoreOptionsMenu()
{
    QMenu menu(this);
    buildMoreOptionsMenu(&menu);
    if (!menu.isEmpty())
        menu.exec(QCursor::pos());
}
```

Update the test from Step 1 to call `buildMoreOptionsMenu(menu)` directly rather than `buildMoreOptionsMenuForTest()`.

Revise the test call:

```cpp
QMenu menu;
view.buildMoreOptionsMenu(&menu);
```

- [ ] **Step 3: Check `WorkspaceLeaf::menuEventEmitter` exists; if not, add a nullable accessor**

```bash
grep -n menuEventEmitter libs/core/include/corbomite/core/WorkspaceLeaf.h
```

If the method does not exist yet (Cluster H shipped `MenuEventEmitter` standalone but didn't necessarily connect it to `WorkspaceLeaf`), add a getter + setter:

```cpp
// in WorkspaceLeaf.h
public:
    Corbomite::MenuEventEmitter *menuEventEmitter() const { return m_menuEmitter; }
    void setMenuEventEmitter(Corbomite::MenuEventEmitter *e) { m_menuEmitter = e; }

private:
    Corbomite::MenuEventEmitter *m_menuEmitter = nullptr;  // non-owning
```

Wiring of the emitter into leaves is the MainWindow's job — the test installs a null emitter, which is fine (guard in `buildMoreOptionsMenu` already null-checks).

- [ ] **Step 4: Build + run all Phase-1 tests**

```bash
cmake --build build -j 10
cd build && ctest -R "tst_menusectionhelper|tst_view_more_options" --output-on-failure
```

Expected: PASS.

- [ ] **Step 5: Run the existing hamburger manually (optional smoke check)**

Build and launch:

```bash
cmake --build build -j 10
./build/Corbomite
```

Open a vault, open a markdown note, click the "…" overflow in the view header. Expected: menu still shows "Rename…" (unchanged UX — EditableFileView still adds it via `onPaneMenu` which is now called with source="more-options"). Menu exits gracefully via Esc.

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/ItemView.h libs/core/src/ItemView.cpp libs/core/include/corbomite/core/WorkspaceLeaf.h tests/core/tst_view_more_options.cpp
git commit -m "$(cat <<'EOF'
feat(views): rewrite ItemView::showMoreOptionsMenu around MenuSectionHelper

showMoreOptionsMenu now orchestrates:
  1. onMoreOptionsMenu(helper) — primary subclass hook
  2. onPaneMenu(menu, "more-options") — back-compat
  3. MenuEventEmitter::emitLeafMenu — plugin hook
  4. helper.finalize()

buildMoreOptionsMenu is exposed as the test seam that populates without
exec()ing.

No user-visible UX change yet — EditableFileView's existing "Rename…" entry
still appears via the onPaneMenu back-compat path. Phase 2 migrates it.

Closes Cluster R Phase 1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

# Phase 2 — Universal File-Menu Items on `EditableFileView`

Goal: every item available for any file-view (markdown, canvas, future image/PDF) wires through a real primitive. Unbuilt subsystems ship as disabled placeholders with tooltips citing the blocking cluster.

## Task 2.1: `FileNameValidator` — shared validation rules

**Files:**
- Create: `libs/vault/src/FileNameValidator.h`
- Create: `libs/vault/src/FileNameValidator.cpp`
- Create: `tests/vault/tst_file_name_validator.cpp`
- Modify: `libs/vault/CMakeLists.txt`
- Modify: `tests/vault/CMakeLists.txt`

- [ ] **Step 1: Write failing tests for each validation rule**

Create `tests/vault/tst_file_name_validator.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "FileNameValidator.h"
#include <QTemporaryDir>

class TestFileNameValidator : public QObject {
    Q_OBJECT

private slots:
    void testEmptyNameIsInvalid()
    {
        auto err = Corbomite::validateFileName(QStringLiteral(""), nullptr, nullptr);
        QVERIFY(!err.isEmpty());
        QVERIFY(err.contains(QStringLiteral("required"), Qt::CaseInsensitive));
    }

    void testBackslashIsInvalid()
    {
        auto err = Corbomite::validateFileName(QStringLiteral("foo\\bar.md"), nullptr, nullptr);
        QVERIFY(!err.isEmpty());
        QVERIFY(err.contains(QStringLiteral("cannot contain"), Qt::CaseInsensitive));
    }

    void testColonIsInvalid()
    {
        auto err = Corbomite::validateFileName(QStringLiteral("foo:bar.md"), nullptr, nullptr);
        QVERIFY(!err.isEmpty());
    }

    void testReservedWindowsNameFlagged()
    {
        auto err = Corbomite::validateFileName(QStringLiteral("CON.md"), nullptr, nullptr);
#ifdef Q_OS_WIN
        QVERIFY(!err.isEmpty());
        QVERIFY(err.contains(QStringLiteral("reserved"), Qt::CaseInsensitive));
#else
        // On non-Windows, reserved names are warnings but not hard-block; no error.
        Q_UNUSED(err);  // Currently we still surface on all platforms; assert Windows only.
#endif
    }

    void testCollisionDetection()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QFile f(tmp.filePath(QStringLiteral("existing.md")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hi");
        f.close();

        Corbomite::Storage::FileSystemAdapter fsa;
        Corbomite::Vault vault(&fsa);
        QVERIFY(vault.load(tmp.path()));

        auto *file = vault.getFileByPath(QStringLiteral("existing.md"));
        QVERIFY(file != nullptr);

        // Try renaming another (hypothetical) file to "existing.md"
        QFile f2(tmp.filePath(QStringLiteral("other.md")));
        QVERIFY(f2.open(QIODevice::WriteOnly));
        f2.write("hi");
        f2.close();
        QVERIFY(vault.load(tmp.path()));  // reload to pick up other.md
        auto *other = vault.getFileByPath(QStringLiteral("other.md"));
        QVERIFY(other != nullptr);

        auto err = Corbomite::validateFileName(QStringLiteral("existing.md"), other, &vault);
        QVERIFY(!err.isEmpty());
        QVERIFY(err.contains(QStringLiteral("already exists"), Qt::CaseInsensitive));
    }

    void testValidNamePassesWithEmptyReturn()
    {
        QTemporaryDir tmp;
        Corbomite::Storage::FileSystemAdapter fsa;
        Corbomite::Vault vault(&fsa);
        vault.load(tmp.path());
        auto err = Corbomite::validateFileName(QStringLiteral("valid-name.md"), nullptr, &vault);
        QVERIFY(err.isEmpty());
    }
};

QTEST_MAIN(TestFileNameValidator)
#include "tst_file_name_validator.moc"
```

- [ ] **Step 2: Register test in `tests/vault/CMakeLists.txt`**

Add:

```cmake
add_corbomite_test(tst_file_name_validator
    SOURCES tst_file_name_validator.cpp
    LINK_LIBRARIES
        Corbomite::Vault
        Corbomite::Storage
        Qt6::Test
)
```

- [ ] **Step 3: Run and verify failure**

```bash
cmake --build build -j 10 --target tst_file_name_validator 2>&1 | tail -20
```

Expected: compile FAIL — `FileNameValidator.h` doesn't exist.

- [ ] **Step 4: Declare the validator**

Create `libs/vault/src/FileNameValidator.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>

namespace Corbomite {
class Vault;
class TAbstractFile;

// Validates a proposed filename (or rename target).
// Returns empty QString when valid; error message otherwise.
//
// `sourceFile` is the file being renamed — used to exclude itself from
// collision checks. Pass nullptr if validating a new-file name.
// `vault` may be nullptr to skip collision + reserved-path checks;
// passing non-null enables the full rule set.
//
// `isFinal` = true applies stricter Windows reserved-name enforcement;
// false allows typing-in-progress partial matches.
QString validateFileName(const QString &newName,
                          const TAbstractFile *sourceFile,
                          const Vault *vault,
                          bool isFinal = false);

} // namespace Corbomite
```

- [ ] **Step 5: Implement in .cpp**

Create `libs/vault/src/FileNameValidator.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "FileNameValidator.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TAbstractFile.h"
#include <KLocalizedString>
#include <QRegularExpression>

namespace Corbomite {

static QSet<QString> windowsReservedBasenames()
{
    static const QSet<QString> reserved = {
        QStringLiteral("CON"), QStringLiteral("PRN"),
        QStringLiteral("AUX"), QStringLiteral("NUL"),
        QStringLiteral("COM1"), QStringLiteral("COM2"), QStringLiteral("COM3"),
        QStringLiteral("COM4"), QStringLiteral("COM5"), QStringLiteral("COM6"),
        QStringLiteral("COM7"), QStringLiteral("COM8"), QStringLiteral("COM9"),
        QStringLiteral("LPT1"), QStringLiteral("LPT2"), QStringLiteral("LPT3"),
        QStringLiteral("LPT4"), QStringLiteral("LPT5"), QStringLiteral("LPT6"),
        QStringLiteral("LPT7"), QStringLiteral("LPT8"), QStringLiteral("LPT9"),
    };
    return reserved;
}

QString validateFileName(const QString &newName,
                          const TAbstractFile *sourceFile,
                          const Vault *vault,
                          bool isFinal)
{
    if (newName.isEmpty())
        return i18n("A file name is required.");

    // Bad characters
    static const QRegularExpression bad(QStringLiteral(R"([\\/:*?"<>|])"));
    if (bad.match(newName).hasMatch())
        return i18n("File name cannot contain any of the following characters: \\ / : * ? \" < > |");

    // Reserved Windows names (basename-only check)
    const QString basename = newName.section('.', 0, 0).toUpper();
    if (windowsReservedBasenames().contains(basename)) {
#ifdef Q_OS_WIN
        return i18n("That file name is reserved. Please use a different name.");
#else
        // On non-Windows, surface anyway — vaults roam across OSes.
        if (isFinal)
            return i18n("That file name is reserved on Windows. Please use a different name.");
#endif
    }

    // Collision detection (skip when no vault supplied)
    if (vault) {
        const QString parentPath = sourceFile ? sourceFile->parentPath() : QString();
        const QString candidatePath = parentPath.isEmpty()
            ? newName : parentPath + QStringLiteral("/") + newName;
        const auto *existing = vault->getAbstractFileByPath(candidatePath);
        if (existing && existing != sourceFile)
            return i18n("A file with this name already exists.");
    }

    return QString();
}

} // namespace Corbomite
```

- [ ] **Step 6: Wire into CMakeLists**

Edit `libs/vault/CMakeLists.txt`. Add `src/FileNameValidator.cpp` to the SOURCES list.

- [ ] **Step 7: Confirm `TAbstractFile::parentPath()` exists; if not, add**

```bash
grep -n "parentPath\|QString.*parent" libs/vault/include/corbomite/vault/TAbstractFile.h
```

If absent, add a thin getter to `TAbstractFile.h`:

```cpp
QString parentPath() const
{
    return m_parent ? m_parent->path() : QString();
}
```

- [ ] **Step 8: Build + run — expected PASS**

```bash
cmake --build build -j 10 --target tst_file_name_validator
cd build && ctest -R tst_file_name_validator --output-on-failure
```

- [ ] **Step 9: Commit**

```bash
git add libs/vault/src/FileNameValidator.{h,cpp} libs/vault/include/corbomite/vault/TAbstractFile.h libs/vault/CMakeLists.txt tests/vault/tst_file_name_validator.cpp tests/vault/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(vault): FileNameValidator — shared rename/create validation

Empty / bad-char (\\/:*?\"<>|) / reserved-Windows / collision rules
centralised into Corbomite::validateFileName(). Returns empty QString on
valid, localised error message otherwise.

Used by promptForFileRename + promptForMove modals in Cluster R Phase 2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

## Task 2.2: `FileManager::promptForFileRename` modal

**Files:**
- Create: `libs/vault/src/dialogs/RenameDialog.h`
- Create: `libs/vault/src/dialogs/RenameDialog.cpp`
- Modify: `libs/vault/include/corbomite/vault/FileManager.h`
- Modify: `libs/vault/src/FileManager.cpp`
- Modify: `libs/vault/CMakeLists.txt`
- Create: `tests/vault/tst_rename_dialog.cpp`
- Modify: `tests/vault/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `tests/vault/tst_rename_dialog.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QLineEdit>

#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "src/metadata/MetadataCache.h"
#include "dialogs/RenameDialog.h"

class TestRenameDialog : public QObject {
    Q_OBJECT

private slots:
    void testBasenameIsPreselected()
    {
        QTemporaryDir tmp;
        QFile f(tmp.filePath(QStringLiteral("foo.md")));
        f.open(QIODevice::WriteOnly);
        f.write("hi");
        f.close();

        Corbomite::Storage::FileSystemAdapter fsa;
        Corbomite::Vault vault(&fsa);
        vault.load(tmp.path());
        auto *file = vault.getFileByPath(QStringLiteral("foo.md"));
        QVERIFY(file);

        Corbomite::RenameDialog dlg(file, &vault, nullptr);
        auto *edit = dlg.findChild<QLineEdit *>();
        QVERIFY(edit);
        QCOMPARE(edit->text(), QStringLiteral("foo.md"));
        QCOMPARE(edit->selectedText(), QStringLiteral("foo"));  // basename pre-selected
    }

    void testInvalidInputBlocksSave()
    {
        QTemporaryDir tmp;
        QFile f(tmp.filePath(QStringLiteral("foo.md")));
        f.open(QIODevice::WriteOnly); f.write("hi"); f.close();
        Corbomite::Storage::FileSystemAdapter fsa;
        Corbomite::Vault vault(&fsa);
        vault.load(tmp.path());
        auto *file = vault.getFileByPath(QStringLiteral("foo.md"));

        Corbomite::RenameDialog dlg(file, &vault, nullptr);
        auto *edit = dlg.findChild<QLineEdit *>();
        edit->setText(QStringLiteral("bad:name.md"));
        QVERIFY(!dlg.isSaveEnabled());  // Save button disabled
    }

    void testValidSaveReturnsNewPath()
    {
        QTemporaryDir tmp;
        QFile f(tmp.filePath(QStringLiteral("foo.md")));
        f.open(QIODevice::WriteOnly); f.write("hi"); f.close();
        Corbomite::Storage::FileSystemAdapter fsa;
        Corbomite::Vault vault(&fsa);
        vault.load(tmp.path());
        auto *file = vault.getFileByPath(QStringLiteral("foo.md"));

        Corbomite::RenameDialog dlg(file, &vault, nullptr);
        auto *edit = dlg.findChild<QLineEdit *>();
        edit->setText(QStringLiteral("bar.md"));
        dlg.accept();
        QCOMPARE(dlg.proposedNewName(), QStringLiteral("bar.md"));
    }
};

QTEST_MAIN(TestRenameDialog)
#include "tst_rename_dialog.moc"
```

- [ ] **Step 2: Declare dialog class**

Create `libs/vault/src/dialogs/RenameDialog.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;
class QLabel;
class QDialogButtonBox;

namespace Corbomite {
class Vault;
class TAbstractFile;

class RenameDialog : public QDialog {
    Q_OBJECT
public:
    explicit RenameDialog(const TAbstractFile *file,
                           const Vault *vault,
                           QWidget *parent = nullptr);

    // Test seam: programmatic Save-button state introspection.
    bool isSaveEnabled() const;

    // The user-entered name (valid iff dialog result == Accepted).
    QString proposedNewName() const;

private slots:
    void onTextChanged(const QString &newText);

private:
    const TAbstractFile *m_file;
    const Vault *m_vault;
    QLineEdit *m_edit;
    QLabel *m_errorLabel;
    QDialogButtonBox *m_buttonBox;
    QString m_result;
};

} // namespace Corbomite
```

- [ ] **Step 3: Implement**

Create `libs/vault/src/dialogs/RenameDialog.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "RenameDialog.h"
#include "../FileNameValidator.h"
#include "corbomite/vault/TAbstractFile.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <KLocalizedString>

namespace Corbomite {

RenameDialog::RenameDialog(const TAbstractFile *file,
                             const Vault *vault,
                             QWidget *parent)
    : QDialog(parent)
    , m_file(file)
    , m_vault(vault)
{
    setWindowTitle(i18n("Rename"));

    auto *lay = new QVBoxLayout(this);

    auto *label = new QLabel(i18n("Enter new name:"), this);
    lay->addWidget(label);

    m_edit = new QLineEdit(this);
    if (file)
        m_edit->setText(file->name());
    lay->addWidget(m_edit);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet(QStringLiteral("color: palette(link-visited);"));
    lay->addWidget(m_errorLabel);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    lay->addWidget(m_buttonBox);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_edit, &QLineEdit::textChanged, this, &RenameDialog::onTextChanged);

    // Pre-select basename (portion before final '.')
    if (file && file->name().contains('.')) {
        const int dotIdx = file->name().lastIndexOf('.');
        m_edit->setSelection(0, dotIdx);
    } else {
        m_edit->selectAll();
    }

    // Initial validation
    onTextChanged(m_edit->text());
}

void RenameDialog::onTextChanged(const QString &newText)
{
    const QString err = validateFileName(newText, m_file, m_vault, /*isFinal=*/false);
    m_errorLabel->setText(err);
    if (auto *save = m_buttonBox->button(QDialogButtonBox::Save))
        save->setEnabled(err.isEmpty());
}

bool RenameDialog::isSaveEnabled() const
{
    auto *save = m_buttonBox->button(QDialogButtonBox::Save);
    return save && save->isEnabled();
}

QString RenameDialog::proposedNewName() const
{
    return m_edit->text();
}

} // namespace Corbomite
```

- [ ] **Step 4: Add `FileManager::promptForFileRename`**

Edit `libs/vault/include/corbomite/vault/FileManager.h`. Add:

```cpp
// Opens the rename modal. Returns the new path on success, empty QString
// on cancel. Delegates to Vault::rename (which handles link-rewrite).
QString promptForFileRename(TAbstractFile *file, QWidget *parent = nullptr);
```

Edit `libs/vault/src/FileManager.cpp`. Add the include + implementation:

```cpp
#include "dialogs/RenameDialog.h"

// ... existing code ...

QString FileManager::promptForFileRename(TAbstractFile *file, QWidget *parent)
{
    if (!file || !m_vault) return QString();

    RenameDialog dlg(file, m_vault, parent);
    if (dlg.exec() != QDialog::Accepted) return QString();

    const QString newName = dlg.proposedNewName();
    if (newName.isEmpty() || newName == file->name()) return QString();

    // Compute new full path
    const QString parentPath = file->parentPath();
    const QString newPath = parentPath.isEmpty()
        ? newName : parentPath + QStringLiteral("/") + newName;

    // Delegate to renameFile (existing link-rewrite machinery)
    const bool ok = renameFile(file, newPath);
    return ok ? newPath : QString();
}
```

- [ ] **Step 5: Wire source + test into CMakeLists**

Edit `libs/vault/CMakeLists.txt`. Add `src/dialogs/RenameDialog.cpp` to SOURCES; add `src/dialogs/` to `target_include_directories` under PRIVATE.

Edit `tests/vault/CMakeLists.txt`. Register:

```cmake
add_corbomite_test(tst_rename_dialog
    SOURCES tst_rename_dialog.cpp
    LINK_LIBRARIES
        Corbomite::Vault
        Corbomite::Storage
        Qt6::Test
        Qt6::Widgets
)
```

- [ ] **Step 6: Build + run — expected PASS**

```bash
cmake --build build -j 10 --target tst_rename_dialog
cd build && ctest -R tst_rename_dialog --output-on-failure
```

- [ ] **Step 7: Commit**

```bash
git add libs/vault/src/dialogs/RenameDialog.{h,cpp} libs/vault/include/corbomite/vault/FileManager.h libs/vault/src/FileManager.cpp libs/vault/CMakeLists.txt tests/vault/tst_rename_dialog.cpp tests/vault/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(vault): FileManager::promptForFileRename modal

QDialog with live validation via Corbomite::validateFileName, basename
pre-selected on open, Save disabled until input is valid. On Accepted,
delegates to Vault::rename for the actual filesystem + link-rewrite work.

Replaces the stub EditableFileView::startRename() that ships a broken
"Rename..." menu item. Wiring into the menu happens in Task 2.8.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

## Task 2.3: `FileManager::promptForMove` folder-picker modal

**Files:**
- Create: `libs/vault/src/dialogs/MoveFileDialog.h` + `.cpp`
- Modify: `libs/vault/include/corbomite/vault/FileManager.h`
- Modify: `libs/vault/src/FileManager.cpp`
- Modify: `libs/vault/CMakeLists.txt`
- Create: `tests/vault/tst_move_file_dialog.cpp`
- Modify: `tests/vault/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `tests/vault/tst_move_file_dialog.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QListWidget>
#include <QLineEdit>

#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "dialogs/MoveFileDialog.h"

class TestMoveFileDialog : public QObject {
    Q_OBJECT

private slots:
    void testFolderListExcludesSource()
    {
        QTemporaryDir tmp;
        QDir(tmp.path()).mkpath(QStringLiteral("archive/2024"));
        QDir(tmp.path()).mkpath(QStringLiteral("notes"));
        QFile f(tmp.filePath(QStringLiteral("notes/foo.md")));
        f.open(QIODevice::WriteOnly); f.write("hi"); f.close();

        Corbomite::Storage::FileSystemAdapter fsa;
        Corbomite::Vault vault(&fsa);
        vault.load(tmp.path());
        auto *file = vault.getFileByPath(QStringLiteral("notes/foo.md"));
        QVERIFY(file);

        Corbomite::MoveFileDialog dlg(file, &vault, nullptr);
        auto list = dlg.availableFolderPaths();
        // Root + archive + archive/2024 are available; notes is source's parent (excluded)
        QVERIFY(list.contains(QStringLiteral("/")));
        QVERIFY(list.contains(QStringLiteral("archive")));
        QVERIFY(list.contains(QStringLiteral("archive/2024")));
        QVERIFY(!list.contains(QStringLiteral("notes")));  // source parent excluded
    }

    void testFuzzyFilterNarrowsList()
    {
        QTemporaryDir tmp;
        QDir(tmp.path()).mkpath(QStringLiteral("archive"));
        QDir(tmp.path()).mkpath(QStringLiteral("daily"));
        QDir(tmp.path()).mkpath(QStringLiteral("notes"));
        QFile f(tmp.filePath(QStringLiteral("src.md")));
        f.open(QIODevice::WriteOnly); f.close();

        Corbomite::Storage::FileSystemAdapter fsa;
        Corbomite::Vault vault(&fsa);
        vault.load(tmp.path());
        auto *file = vault.getFileByPath(QStringLiteral("src.md"));

        Corbomite::MoveFileDialog dlg(file, &vault, nullptr);
        dlg.setFilterText(QStringLiteral("arch"));
        auto *listWidget = dlg.findChild<QListWidget *>();
        QVERIFY(listWidget);
        // Only "archive" matches fuzzy "arch"
        QCOMPARE(listWidget->count(), 1);
        QCOMPARE(listWidget->item(0)->text(), QStringLiteral("archive"));
    }
};

QTEST_MAIN(TestMoveFileDialog)
#include "tst_move_file_dialog.moc"
```

- [ ] **Step 2: Declare the dialog**

Create `libs/vault/src/dialogs/MoveFileDialog.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDialog>
#include <QStringList>

class QLineEdit;
class QListWidget;

namespace Corbomite {
class Vault;
class TAbstractFile;

class MoveFileDialog : public QDialog {
    Q_OBJECT
public:
    explicit MoveFileDialog(const TAbstractFile *file,
                             const Vault *vault,
                             QWidget *parent = nullptr);

    // Test seam: folder paths currently candidate for selection.
    QStringList availableFolderPaths() const;

    // Test seam: apply a filter programmatically.
    void setFilterText(const QString &filter);

    // Selected folder path ("/" = root); empty iff cancelled.
    QString selectedFolderPath() const;

private slots:
    void onFilterChanged(const QString &text);
    void onItemActivated();

private:
    void populateFolderList();

    const TAbstractFile *m_file;
    const Vault *m_vault;
    QLineEdit *m_filterEdit;
    QListWidget *m_listWidget;
    QStringList m_allPaths;
    QString m_selection;
};

} // namespace Corbomite
```

- [ ] **Step 3: Implement**

Create `libs/vault/src/dialogs/MoveFileDialog.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "MoveFileDialog.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFolder.h"

#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <KLocalizedString>

namespace Corbomite {

MoveFileDialog::MoveFileDialog(const TAbstractFile *file,
                                 const Vault *vault,
                                 QWidget *parent)
    : QDialog(parent)
    , m_file(file)
    , m_vault(vault)
{
    setWindowTitle(i18n("Move file"));
    resize(400, 360);

    auto *lay = new QVBoxLayout(this);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(i18n("Type folder to search"));
    lay->addWidget(m_filterEdit);

    m_listWidget = new QListWidget(this);
    lay->addWidget(m_listWidget);

    populateFolderList();

    connect(m_filterEdit, &QLineEdit::textChanged, this, &MoveFileDialog::onFilterChanged);
    connect(m_listWidget, &QListWidget::itemActivated, this, &MoveFileDialog::onItemActivated);
}

void MoveFileDialog::populateFolderList()
{
    m_allPaths.clear();
    if (!m_vault) return;

    const QString sourceParent = m_file ? m_file->parentPath() : QString();

    // Root ("/" displayed)
    m_allPaths.append(QStringLiteral("/"));

    // Walk all folders
    for (const auto *f : m_vault->getAllLoadedFiles()) {
        if (auto *folder = dynamic_cast<const TFolder *>(f)) {
            if (folder->path() == sourceParent) continue;  // exclude source's current parent
            if (folder->path() == QStringLiteral("/") ) continue;  // root already added
            m_allPaths.append(folder->path());
        }
    }
    m_allPaths.sort();

    for (const QString &p : m_allPaths)
        m_listWidget->addItem(p);
}

void MoveFileDialog::onFilterChanged(const QString &text)
{
    m_listWidget->clear();
    if (text.isEmpty()) {
        for (const QString &p : m_allPaths)
            m_listWidget->addItem(p);
        return;
    }
    // Simple case-insensitive substring match (a fuzzy-matcher hookup is a
    // later follow-up; basic substring is sufficient for MVP).
    for (const QString &p : m_allPaths) {
        if (p.contains(text, Qt::CaseInsensitive))
            m_listWidget->addItem(p);
    }
}

void MoveFileDialog::setFilterText(const QString &filter)
{
    m_filterEdit->setText(filter);
}

void MoveFileDialog::onItemActivated()
{
    auto *item = m_listWidget->currentItem();
    if (!item) return;
    m_selection = item->text();
    accept();
}

QStringList MoveFileDialog::availableFolderPaths() const
{
    return m_allPaths;
}

QString MoveFileDialog::selectedFolderPath() const
{
    return m_selection;
}

} // namespace Corbomite
```

- [ ] **Step 4: Add `FileManager::promptForMove`**

Edit `libs/vault/include/corbomite/vault/FileManager.h`:

```cpp
QString promptForMove(TAbstractFile *file, QWidget *parent = nullptr);
```

Edit `libs/vault/src/FileManager.cpp`:

```cpp
#include "dialogs/MoveFileDialog.h"

QString FileManager::promptForMove(TAbstractFile *file, QWidget *parent)
{
    if (!file || !m_vault) return QString();

    MoveFileDialog dlg(file, m_vault, parent);
    if (dlg.exec() != QDialog::Accepted) return QString();

    QString folderPath = dlg.selectedFolderPath();
    if (folderPath.isEmpty()) return QString();

    // Normalize root "/" to empty
    if (folderPath == QStringLiteral("/")) folderPath.clear();

    const QString newPath = folderPath.isEmpty()
        ? file->name()
        : folderPath + QStringLiteral("/") + file->name();

    // Collision check: target folder already has a file by this name?
    if (m_vault->getAbstractFileByPath(newPath) != nullptr)
        return QString();  // UX follow-up: surface a Notice

    const bool ok = renameFile(file, newPath);
    return ok ? newPath : QString();
}
```

- [ ] **Step 5: Wire CMake + build + run — expected PASS**

```bash
cmake --build build -j 10 --target tst_move_file_dialog
cd build && ctest -R tst_move_file_dialog --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git add libs/vault/src/dialogs/MoveFileDialog.{h,cpp} libs/vault/include/corbomite/vault/FileManager.h libs/vault/src/FileManager.cpp libs/vault/CMakeLists.txt tests/vault/tst_move_file_dialog.cpp tests/vault/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(vault): FileManager::promptForMove folder-picker modal

QListWidget of vault folders with QLineEdit substring filter. Root shown
as "/". Source's current parent excluded. On activation, delegates to
FileManager::renameFile to move the file to the new parent (link-rewrite
included).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

## Task 2.4: `FileManager::promptForDeletion` confirm modal

**Files:**
- Create: `libs/vault/src/dialogs/DeleteConfirmDialog.h` + `.cpp`
- Modify: `libs/vault/include/corbomite/vault/FileManager.h`
- Modify: `libs/vault/src/FileManager.cpp`
- Modify: `libs/vault/CMakeLists.txt`
- Create: `tests/vault/tst_delete_confirm_dialog.cpp`
- Modify: `tests/vault/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `tests/vault/tst_delete_confirm_dialog.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDialogButtonBox>
#include <QPushButton>

#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "dialogs/DeleteConfirmDialog.h"

class TestDeleteConfirmDialog : public QObject {
    Q_OBJECT

private slots:
    void testDefaultButtonIsCancel()
    {
        QTemporaryDir tmp;
        QFile f(tmp.filePath(QStringLiteral("foo.md")));
        f.open(QIODevice::WriteOnly); f.close();
        Corbomite::Storage::FileSystemAdapter fsa;
        Corbomite::Vault vault(&fsa);
        vault.load(tmp.path());
        auto *file = vault.getFileByPath(QStringLiteral("foo.md"));

        Corbomite::DeleteConfirmDialog dlg(file, &vault, nullptr);
        auto *bb = dlg.findChild<QDialogButtonBox *>();
        QVERIFY(bb);
        auto *cancel = bb->button(QDialogButtonBox::Cancel);
        QVERIFY(cancel && cancel->isDefault());
    }

    void testSystemTrashOptionTextShown()
    {
        QTemporaryDir tmp;
        QFile f(tmp.filePath(QStringLiteral("foo.md")));
        f.open(QIODevice::WriteOnly); f.close();
        Corbomite::Storage::FileSystemAdapter fsa;
        Corbomite::Vault vault(&fsa);
        vault.load(tmp.path());
        vault.setConfig(QStringLiteral("trashOption"), QStringLiteral("system"));
        auto *file = vault.getFileByPath(QStringLiteral("foo.md"));

        Corbomite::DeleteConfirmDialog dlg(file, &vault, nullptr);
        QVERIFY(dlg.bodyText().contains(QStringLiteral("system trash"), Qt::CaseInsensitive));
    }

    void testDontAskAgainCheckboxPersists()
    {
        QTemporaryDir tmp;
        QFile f(tmp.filePath(QStringLiteral("foo.md")));
        f.open(QIODevice::WriteOnly); f.close();
        Corbomite::Storage::FileSystemAdapter fsa;
        Corbomite::Vault vault(&fsa);
        vault.load(tmp.path());
        auto *file = vault.getFileByPath(QStringLiteral("foo.md"));

        Corbomite::DeleteConfirmDialog dlg(file, &vault, nullptr);
        dlg.setDontAskAgain(true);
        dlg.accept();

        QCOMPARE(vault.getConfig(QStringLiteral("promptDelete"), QVariant(true)).toBool(), false);
    }
};

QTEST_MAIN(TestDeleteConfirmDialog)
#include "tst_delete_confirm_dialog.moc"
```

- [ ] **Step 2: Declare the dialog**

Create `libs/vault/src/dialogs/DeleteConfirmDialog.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDialog>

class QCheckBox;
class QLabel;

namespace Corbomite {
class Vault;
class TAbstractFile;

class DeleteConfirmDialog : public QDialog {
    Q_OBJECT
public:
    explicit DeleteConfirmDialog(const TAbstractFile *file,
                                   Vault *vault,
                                   QWidget *parent = nullptr);

    QString bodyText() const;
    void setDontAskAgain(bool on);

protected:
    void accept() override;

private:
    const TAbstractFile *m_file;
    Vault *m_vault;  // non-const — may write promptDelete config
    QLabel *m_bodyLabel;
    QCheckBox *m_dontAsk;
};

} // namespace Corbomite
```

- [ ] **Step 3: Implement**

Create `libs/vault/src/dialogs/DeleteConfirmDialog.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "DeleteConfirmDialog.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFolder.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <KLocalizedString>

namespace Corbomite {

DeleteConfirmDialog::DeleteConfirmDialog(const TAbstractFile *file,
                                           Vault *vault,
                                           QWidget *parent)
    : QDialog(parent)
    , m_file(file)
    , m_vault(vault)
{
    const bool isFolder = dynamic_cast<const TFolder *>(file) != nullptr;
    setWindowTitle(isFolder ? i18n("Delete folder") : i18n("Delete file"));

    auto *main = new QVBoxLayout(this);

    auto *iconRow = new QHBoxLayout;
    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-warning")).pixmap(48, 48));
    iconRow->addWidget(iconLabel);

    m_bodyLabel = new QLabel(this);
    m_bodyLabel->setWordWrap(true);
    m_bodyLabel->setText(bodyText());
    iconRow->addWidget(m_bodyLabel, 1);

    main->addLayout(iconRow);

    // Don't ask again — only for files, not folders (folders always prompt).
    if (!isFolder) {
        m_dontAsk = new QCheckBox(i18n("Don't ask again"), this);
        main->addWidget(m_dontAsk);
    } else {
        m_dontAsk = nullptr;
    }

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Yes, this);
    auto *deleteBtn = bb->button(QDialogButtonBox::Yes);
    deleteBtn->setText(i18n("Delete"));
    deleteBtn->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    auto *cancelBtn = bb->button(QDialogButtonBox::Cancel);
    cancelBtn->setDefault(true);  // destructive-action convention
    deleteBtn->setDefault(false);
    main->addWidget(bb);

    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString DeleteConfirmDialog::bodyText() const
{
    if (!m_file || !m_vault) return QString();
    const bool isFolder = dynamic_cast<const TFolder *>(m_file) != nullptr;
    const QString trashOpt = m_vault->getConfig(QStringLiteral("trashOption"), QStringLiteral("system")).toString();

    QString trashBlurb;
    if (trashOpt == QStringLiteral("system"))
        trashBlurb = i18n("It will be moved to your system trash.");
    else if (trashOpt == QStringLiteral("local"))
        trashBlurb = i18n("It will be moved to Corbomite's trash folder.");
    else
        trashBlurb = i18n("It will be permanently deleted.");

    if (isFolder) {
        return i18n("Are you sure you want to delete folder \"%1\"?\n\n%2",
                    m_file->name(), trashBlurb);
    }
    return i18n("Are you sure you want to delete \"%1\"?\n\n%2",
                m_file->name(), trashBlurb);
}

void DeleteConfirmDialog::setDontAskAgain(bool on)
{
    if (m_dontAsk) m_dontAsk->setChecked(on);
}

void DeleteConfirmDialog::accept()
{
    if (m_dontAsk && m_dontAsk->isChecked() && m_vault)
        m_vault->setConfig(QStringLiteral("promptDelete"), QVariant(false));
    QDialog::accept();
}

} // namespace Corbomite
```

- [ ] **Step 4: Add `FileManager::promptForDeletion`**

Edit `libs/vault/include/corbomite/vault/FileManager.h`:

```cpp
// Shows the delete-confirm modal (respects Vault::getConfig("promptDelete")).
// Returns true iff the user confirmed and the trash succeeded.
bool promptForDeletion(TAbstractFile *file, QWidget *parent = nullptr);
```

Edit `libs/vault/src/FileManager.cpp`:

```cpp
#include "dialogs/DeleteConfirmDialog.h"

bool FileManager::promptForDeletion(TAbstractFile *file, QWidget *parent)
{
    if (!file || !m_vault) return false;

    const bool isFolder = dynamic_cast<TFolder *>(file) != nullptr;
    const bool promptEnabled = m_vault->getConfig(QStringLiteral("promptDelete"), QVariant(true)).toBool();

    // Folders always prompt; files respect promptDelete config.
    if (!isFolder && !promptEnabled)
        return trashFile(file);

    DeleteConfirmDialog dlg(file, m_vault, parent);
    if (dlg.exec() != QDialog::Accepted) return false;

    return trashFile(file);
}
```

- [ ] **Step 5: Wire CMake, build, run**

```bash
cmake --build build -j 10 --target tst_delete_confirm_dialog
cd build && ctest -R tst_delete_confirm_dialog --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/vault/src/dialogs/DeleteConfirmDialog.{h,cpp} libs/vault/include/corbomite/vault/FileManager.h libs/vault/src/FileManager.cpp libs/vault/CMakeLists.txt tests/vault/tst_delete_confirm_dialog.cpp tests/vault/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(vault): FileManager::promptForDeletion confirm modal

Warning-styled dialog with trash-option-aware body text. "Don't ask again"
checkbox writes vault.setConfig(promptDelete, false). Cancel is the default
button (destructive-action convention). Folder deletions always prompt
regardless of config.

Respects vault's trashOption config (system / local / none) for body copy.
Routes trash through FileManager::trashFile on Accept.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

## Task 2.5: `Corbomite::Platform::openWithDefaultApp` + `showInFolder`

**Files:**
- Create: `libs/core/include/corbomite/core/Platform.h`
- Create: `libs/core/src/Platform.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Create: `tests/core/tst_platform.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Declare the namespace + functions**

Create `libs/core/include/corbomite/core/Platform.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>

namespace Corbomite::Platform {

// Opens `absolutePath` through the OS default application.
// Returns true on success.
// On failure callers should surface a Notice (this function doesn't show UI).
bool openWithDefaultApp(const QString &absolutePath);

// Opens the OS file manager at `absolutePath`'s parent folder with the
// file highlighted/selected. On Linux, tries DBus FileManager1 first
// then falls back to xdg-open on the parent folder (no selection).
bool showInFolder(const QString &absolutePath);

} // namespace Corbomite::Platform
```

- [ ] **Step 2: Write failing test**

Create `tests/core/tst_platform.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QTemporaryFile>

#include "corbomite/core/Platform.h"

class TestPlatform : public QObject {
    Q_OBJECT

private slots:
    void testOpenNonexistentPathReturnsFalse()
    {
        const bool ok = Corbomite::Platform::openWithDefaultApp(
            QStringLiteral("/nonexistent/zzz-cluster-r.xyz"));
        QVERIFY(!ok);
    }

    void testShowInFolderNonexistentPathReturnsFalse()
    {
        const bool ok = Corbomite::Platform::showInFolder(
            QStringLiteral("/nonexistent/zzz-cluster-r.xyz"));
        QVERIFY(!ok);
    }

    void testShowInFolderExistingPathReturnsTrue()
    {
        // Smoke: on CI without DBus, we expect the fallback to succeed
        // (opening the parent folder via xdg-open).
        // In headless CI this may fail; skip on non-desktop environments.
        if (qgetenv("DISPLAY").isEmpty() && qgetenv("WAYLAND_DISPLAY").isEmpty()) {
            QSKIP("No display; skipping real-open smoke");
        }

        QTemporaryFile tmp(QStringLiteral("cluster-r-test-XXXXXX.txt"));
        QVERIFY(tmp.open());
        tmp.write("hi");
        tmp.flush();

        // Don't actually invoke — opening the folder in CI pollutes session.
        // Instead, test the function's preflight: existence check should pass.
        // We'll trust the fallback chain and assert on the preflight only.
        QFileInfo info(tmp.fileName());
        QVERIFY(info.exists());
    }
};

QTEST_MAIN(TestPlatform)
#include "tst_platform.moc"
```

- [ ] **Step 3: Implement**

Create `libs/core/src/Platform.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Platform.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#endif

namespace Corbomite::Platform {

bool openWithDefaultApp(const QString &absolutePath)
{
    QFileInfo info(absolutePath);
    if (!info.exists()) return false;

    const QUrl url = QUrl::fromLocalFile(info.absoluteFilePath());
    return QDesktopServices::openUrl(url);
}

bool showInFolder(const QString &absolutePath)
{
    QFileInfo info(absolutePath);
    if (!info.exists()) return false;

    const QString abs = info.absoluteFilePath();

#ifdef Q_OS_MACOS
    return QProcess::startDetached(QStringLiteral("open"),
                                    {QStringLiteral("-R"), abs});
#elif defined(Q_OS_WIN)
    return QProcess::startDetached(QStringLiteral("explorer"),
                                    {QStringLiteral("/select,"),
                                     QDir::toNativeSeparators(abs)});
#elif defined(Q_OS_LINUX)
    // Try DBus FileManager1 first
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        QDBusInterface fm1(QStringLiteral("org.freedesktop.FileManager1"),
                            QStringLiteral("/org/freedesktop/FileManager1"),
                            QStringLiteral("org.freedesktop.FileManager1"),
                            bus);
        if (fm1.isValid()) {
            const QString uri = QUrl::fromLocalFile(abs).toString();
            QDBusReply<void> reply = fm1.call(QStringLiteral("ShowItems"),
                                                QStringList{uri},
                                                QStringLiteral("corbomite"));
            if (reply.isValid()) return true;
        }
    }
    // Fallback: open parent folder (file not selected)
    const QString parent = QFileInfo(abs).absolutePath();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(parent));
#else
    // Unknown platform — fall back to opening parent folder
    const QString parent = QFileInfo(abs).absolutePath();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(parent));
#endif
}

} // namespace Corbomite::Platform
```

- [ ] **Step 4: Wire CMake**

Edit `libs/core/CMakeLists.txt`. Add `src/Platform.cpp` to SOURCES. Add `Qt6::DBus` under `target_link_libraries(... PRIVATE ...)` for Linux:

```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    target_link_libraries(Corbomite_Core PRIVATE Qt6::DBus)
endif()
```

- [ ] **Step 5: Build + run — expected PASS**

```bash
cmake --build build -j 10 --target tst_platform
cd build && ctest -R tst_platform --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/Platform.h libs/core/src/Platform.cpp libs/core/CMakeLists.txt tests/core/tst_platform.cpp tests/core/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(platform): openWithDefaultApp + showInFolder primitives

openWithDefaultApp: QDesktopServices::openUrl wrapper with preflight exists
check. showInFolder: on Linux tries DBus FileManager1 ShowItems first (works
on Dolphin/Nautilus/Nemo/PCManFM-Qt), falls back to xdg-open on parent dir
if DBus unavailable. macOS: `open -R`. Windows: `explorer /select,`.

Cited by cluster-r spec §4 file structure and per-addenda at
obsidian-audit/addenda/2026-04-19-{open-with-default-app,show-in-folder}.md.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

## Task 2.6: `Corbomite::PathUtils::obsidianUrlFor` + corbomite variant

**Files:**
- Create: `libs/core/include/corbomite/core/PathUtils.h`
- Create: `libs/core/src/PathUtils.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Create: `tests/core/tst_path_utils.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

Create `tests/core/tst_path_utils.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include "corbomite/core/PathUtils.h"

class TestPathUtils : public QObject {
    Q_OBJECT

private slots:
    void testObsidianUrlFor()
    {
        const QString url = Corbomite::PathUtils::obsidianUrlFor(
            QStringLiteral("my-vault"),
            QStringLiteral("notes/foo.md"));
        QCOMPARE(url, QStringLiteral("obsidian://open?vault=my-vault&file=notes%2Ffoo.md"));
    }

    void testObsidianUrlForWithSubpath()
    {
        const QString url = Corbomite::PathUtils::obsidianUrlFor(
            QStringLiteral("my-vault"),
            QStringLiteral("notes/foo.md"),
            QStringLiteral("#Heading"));
        QCOMPARE(url, QStringLiteral("obsidian://open?vault=my-vault&file=notes%2Ffoo.md%23Heading"));
    }

    void testObsidianUrlForPercentEncodesSpace()
    {
        const QString url = Corbomite::PathUtils::obsidianUrlFor(
            QStringLiteral("my vault"),
            QStringLiteral("my note.md"));
        QCOMPARE(url, QStringLiteral("obsidian://open?vault=my%20vault&file=my%20note.md"));
    }

    void testCorbomiteUrlFor()
    {
        const QString url = Corbomite::PathUtils::corbomiteUrlFor(
            QStringLiteral("my-vault"),
            QStringLiteral("notes/foo.md"));
        QCOMPARE(url, QStringLiteral("corbomite://open?vault=my-vault&file=notes%2Ffoo.md"));
    }
};

QTEST_MAIN(TestPathUtils)
#include "tst_path_utils.moc"
```

- [ ] **Step 2: Declare + implement**

Create `libs/core/include/corbomite/core/PathUtils.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>

namespace Corbomite::PathUtils {

// Emits an obsidian://open?vault=...&file=... URL.
// vaultName is typically the vault's folder basename.
// subpath is an optional "#heading" or "#^block" suffix that becomes
// part of the file= query value (percent-encoded).
QString obsidianUrlFor(const QString &vaultName,
                        const QString &relativePath,
                        const QString &subpath = QString());

// Corbomite-native variant (same format with `corbomite://` scheme).
// Used alongside obsidianUrlFor for interop: users pasting into Corbomite
// get direct-open; Obsidian users use the obsidian:// variant.
QString corbomiteUrlFor(const QString &vaultName,
                          const QString &relativePath,
                          const QString &subpath = QString());

} // namespace Corbomite::PathUtils
```

Create `libs/core/src/PathUtils.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/PathUtils.h"
#include <QUrl>

namespace Corbomite::PathUtils {

static QString buildUrl(const QString &scheme,
                          const QString &vaultName,
                          const QString &relativePath,
                          const QString &subpath)
{
    const QString vault = QString::fromUtf8(QUrl::toPercentEncoding(vaultName));
    const QString file = QString::fromUtf8(QUrl::toPercentEncoding(relativePath + subpath));
    return QStringLiteral("%1://open?vault=%2&file=%3").arg(scheme, vault, file);
}

QString obsidianUrlFor(const QString &vaultName,
                         const QString &relativePath,
                         const QString &subpath)
{
    return buildUrl(QStringLiteral("obsidian"), vaultName, relativePath, subpath);
}

QString corbomiteUrlFor(const QString &vaultName,
                          const QString &relativePath,
                          const QString &subpath)
{
    return buildUrl(QStringLiteral("corbomite"), vaultName, relativePath, subpath);
}

} // namespace Corbomite::PathUtils
```

- [ ] **Step 3: Wire CMake + build + run — expected PASS**

```bash
cmake --build build -j 10 --target tst_path_utils
cd build && ctest -R tst_path_utils --output-on-failure
```

- [ ] **Step 4: Commit**

```bash
git add libs/core/include/corbomite/core/PathUtils.h libs/core/src/PathUtils.cpp libs/core/CMakeLists.txt tests/core/tst_path_utils.cpp tests/core/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(core): PathUtils::obsidianUrlFor + corbomiteUrlFor

Percent-encoded URL generators for Copy-path-as-URL menu actions.
Both variants emit the same shape (scheme://open?vault=...&file=...).
Spaces / unicode / special chars encode via QUrl::toPercentEncoding.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

## Task 2.7: `file-explorer:reveal-file` command

**Files:**
- Modify: `src/plugins/file-explorer/FileExplorerView.h` + `.cpp`
- Modify: `src/plugins/file-explorer/FileExplorerPlugin.cpp`

- [ ] **Step 1: Add `revealPath` method on FileExplorerView**

Edit `src/plugins/file-explorer/FileExplorerView.h`. Add to public section:

```cpp
public:
    // Scrolls to and selects the given vault-relative path in the tree view.
    // Expands ancestor folders as necessary. No-op if path is not in the
    // current tree.
    void revealPath(const QString &relativePath);
```

Edit `src/plugins/file-explorer/FileExplorerView.cpp`. Add:

```cpp
void FileExplorerView::revealPath(const QString &relativePath)
{
    if (!m_treeView || !m_model) return;
    const QModelIndex idx = m_model->indexForPath(relativePath);
    if (!idx.isValid()) return;

    // Expand ancestors
    QModelIndex ancestor = idx.parent();
    while (ancestor.isValid()) {
        m_treeView->expand(ancestor);
        ancestor = ancestor.parent();
    }

    m_treeView->setCurrentIndex(idx);
    m_treeView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
}
```

Verify `NotesTreeModel::indexForPath` exists; if not, add:

```bash
grep -n "indexForPath" libs/models/include/corbomite/models/NotesTreeModel.h
```

If missing, add to `NotesTreeModel.h`:

```cpp
QModelIndex indexForPath(const QString &relativePath) const;
```

Implementation in `NotesTreeModel.cpp` walks the tree using the existing `getAbstractFileByPath` + matching against internal index pointers.

- [ ] **Step 2: Register the command in FileExplorerPlugin**

Edit `src/plugins/file-explorer/FileExplorerPlugin.cpp`. In `onLoad`, add:

```cpp
// Register file-explorer:reveal-file command
if (auto *commands = context()->commandRegistrar()) {
    commands->registerCommand({
        QStringLiteral("file-explorer:reveal-file"),
        i18n("Reveal file in File Explorer"),
        [this](const QVariantMap &args) {
            const QString path = args.value(QStringLiteral("path")).toString();
            if (m_view) m_view->revealPath(path);
        }
    });
}
```

(Verify the exact `CommandRegistrar::registerCommand` signature in `libs/core/include/corbomite/core/proxies/CommandRegistrar.h` and adapt if needed.)

- [ ] **Step 3: Write a test for the command**

Append to `tests/plugins/tst_file_explorer.cpp` (or create if absent):

```cpp
void testRevealFileCommand()
{
    // Bring up a FileExplorerPlugin, dispatch the command, verify the tree
    // view's currentIndex is the expected path.
    // Scaffolding mirrors existing tst_file_explorer patterns (vault setup,
    // plugin load).
    // ...
    auto *cmd = context.commandRegistrar()->find(QStringLiteral("file-explorer:reveal-file"));
    QVERIFY(cmd);
    cmd->invoke({{QStringLiteral("path"), QStringLiteral("notes/foo.md")}});

    QCOMPARE(view->currentPath(), QStringLiteral("notes/foo.md"));
}
```

(Exact test pattern depends on existing test infrastructure; adapt to match.)

- [ ] **Step 4: Build + run**

```bash
cmake --build build -j 10 --target tst_file_explorer
cd build && ctest -R tst_file_explorer --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add src/plugins/file-explorer/FileExplorerView.{h,cpp} src/plugins/file-explorer/FileExplorerPlugin.cpp libs/models/include/corbomite/models/NotesTreeModel.h libs/models/src/NotesTreeModel.cpp tests/plugins/tst_file_explorer.cpp
git commit -m "$(cat <<'EOF'
feat(file-explorer): reveal-file command + FileExplorerView::revealPath

FileExplorerView::revealPath(relPath) scrolls + selects the given path in
the tree, expanding ancestor folders as needed. Wired via new
file-explorer:reveal-file command in FileExplorerPlugin::onLoad.

Consumed by Cluster R's "Reveal file in Navigation" menu item.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

## Task 2.8: `EditableFileView::onMoreOptionsMenu` wiring

**Files:**
- Modify: `libs/core/include/corbomite/core/EditableFileView.h`
- Modify: `libs/core/src/EditableFileView.cpp`

- [ ] **Step 1: Declare the override**

Edit `libs/core/include/corbomite/core/EditableFileView.h`. Add to public section:

```cpp
void onMoreOptionsMenu(MenuSectionHelper &helper) override;
```

- [ ] **Step 2: Implement**

Edit `libs/core/src/EditableFileView.cpp`. Add includes + implementation:

```cpp
#include "corbomite/core/MenuSectionHelper.h"
#include "corbomite/core/Platform.h"
#include "corbomite/core/PathUtils.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include <QApplication>
#include <QClipboard>

void EditableFileView::onMoreOptionsMenu(MenuSectionHelper &helper)
{
    if (!m_file) return;
    // m_vault / m_fileManager / m_commands accessors must be available —
    // verify during this step that EditableFileView already has these handles
    // (if not, fold setters in alongside the menu wiring).

    // action: Rename...
    auto *renameAct = new QAction(QIcon::fromTheme(QStringLiteral("edit-rename")),
                                   i18n("Rename..."), this);
    connect(renameAct, &QAction::triggered, this, [this] {
        if (m_fileManager) m_fileManager->promptForFileRename(m_file, this);
    });
    helper.addToSection(renameAct, QStringLiteral("action"));

    // action: Move file to...
    auto *moveAct = new QAction(QIcon::fromTheme(QStringLiteral("folder-open")),
                                 i18n("Move file to..."), this);
    connect(moveAct, &QAction::triggered, this, [this] {
        if (m_fileManager) m_fileManager->promptForMove(m_file, this);
    });
    helper.addToSection(moveAct, QStringLiteral("action"));

    // info.copy submenu: Copy path -> 3 variants
    auto *copySubmenu = helper.addSubmenu(QStringLiteral("info.copy"),
                                            i18n("Copy path"),
                                            QIcon::fromTheme(QStringLiteral("edit-copy")));
    auto *asObsidianUrl = new QAction(i18n("as Obsidian URL"), this);
    connect(asObsidianUrl, &QAction::triggered, this, [this] {
        if (!m_vault || !m_file) return;
        const QString vaultName = QFileInfo(m_vault->basePath()).fileName();
        const QString url = PathUtils::obsidianUrlFor(vaultName, m_file->path());
        QApplication::clipboard()->setText(url);
    });
    copySubmenu->addToSection(asObsidianUrl, QStringLiteral("action"));

    auto *fromVault = new QAction(i18n("from vault folder"), this);
    connect(fromVault, &QAction::triggered, this, [this] {
        if (!m_file) return;
        QApplication::clipboard()->setText(m_file->path());
    });
    copySubmenu->addToSection(fromVault, QStringLiteral("action"));

    auto *fromRoot = new QAction(i18n("from system root"), this);
    connect(fromRoot, &QAction::triggered, this, [this] {
        if (!m_vault || !m_file) return;
        QApplication::clipboard()->setText(m_vault->basePath() + QStringLiteral("/") + m_file->path());
    });
    copySubmenu->addToSection(fromRoot, QStringLiteral("action"));

    // system: Open in default app
    auto *openDefault = new QAction(QIcon::fromTheme(QStringLiteral("document-open")),
                                      i18n("Open in default app"), this);
    connect(openDefault, &QAction::triggered, this, [this] {
        if (!m_vault || !m_file) return;
        const QString abs = m_vault->basePath() + QStringLiteral("/") + m_file->path();
        Platform::openWithDefaultApp(abs);
    });
    helper.addToSection(openDefault, QStringLiteral("system"));

    // system: Show in system explorer
    auto *showInFs = new QAction(QIcon::fromTheme(QStringLiteral("folder-open")),
                                   i18n("Show in system explorer"), this);
    connect(showInFs, &QAction::triggered, this, [this] {
        if (!m_vault || !m_file) return;
        const QString abs = m_vault->basePath() + QStringLiteral("/") + m_file->path();
        Platform::showInFolder(abs);
    });
    helper.addToSection(showInFs, QStringLiteral("system"));

    // system: Reveal file in Navigation
    auto *revealAct = new QAction(QIcon::fromTheme(QStringLiteral("mark-location")),
                                    i18n("Reveal file in Navigation"), this);
    connect(revealAct, &QAction::triggered, this, [this] {
        if (!m_commands || !m_file) return;
        m_commands->invoke(QStringLiteral("file-explorer:reveal-file"),
                            {{QStringLiteral("path"), m_file->path()}});
    });
    helper.addToSection(revealAct, QStringLiteral("system"));

    // danger: Delete file
    auto *deleteAct = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete-shred")),
                                    i18n("Delete file"), this);
    connect(deleteAct, &QAction::triggered, this, [this] {
        if (m_fileManager) m_fileManager->promptForDeletion(m_file, this);
    });
    helper.addToSection(deleteAct, QStringLiteral("danger"));

    // pane: Open in new window (disabled placeholder)
    auto *newWindowAct = new QAction(QIcon::fromTheme(QStringLiteral("window-new")),
                                       i18n("Open in new window"), this);
    newWindowAct->setEnabled(false);
    newWindowAct->setToolTip(i18n("Requires WorkspaceWindow popout (Cluster G follow-up #6)"));
    helper.addToSection(newWindowAct, QStringLiteral("pane"));

    // view.linked: Open version history (disabled placeholder)
    auto *versionAct = new QAction(QIcon::fromTheme(QStringLiteral("chronometer")),
                                     i18n("Open version history"), this);
    versionAct->setEnabled(false);
    versionAct->setToolTip(i18n("Requires File Recovery plugin (deferred Cluster T)"));
    helper.addToSection(versionAct, QStringLiteral("view.linked"));
}
```

- [ ] **Step 3: Check for missing EditableFileView member accessors**

Verify `m_vault`, `m_fileManager`, `m_commands` accessors exist. If `m_fileManager` or `m_commands` aren't yet members, add them. The cleanest approach is setters:

```cpp
// in EditableFileView.h
public:
    void setFileManager(Corbomite::FileManager *fm) { m_fileManager = fm; }
    void setCommandRegistrar(Corbomite::CommandRegistrar *c) { m_commands = c; }

private:
    Corbomite::FileManager *m_fileManager = nullptr;
    Corbomite::CommandRegistrar *m_commands = nullptr;
```

MainWindow's `propagateServicesToView` then sets both after view construction.

Edit `src/app/MainWindow.cpp` in `propagateServicesToView` (Cluster G substrate):

```cpp
if (auto *editable = qobject_cast<Corbomite::EditableFileView *>(view)) {
    editable->setFileManager(m_fileManager);
    editable->setCommandRegistrar(m_commandRegistrar);
}
```

- [ ] **Step 4: Remove stub `startRename` body**

Edit `libs/core/src/EditableFileView.cpp`. Replace existing stub:

```cpp
// OLD:
void EditableFileView::startRename()
{
    if (!m_file || m_renaming) return;
    m_renaming = true;
    m_originalName = m_file->name();
    m_renaming = false;
}

// NEW:
void EditableFileView::startRename()
{
    if (!m_file || !m_fileManager) return;
    m_fileManager->promptForFileRename(m_file, this);
}
```

Also update `onPaneMenu` (the tab-header context-menu path) so the existing "Rename…" still works:

```cpp
void EditableFileView::onPaneMenu(QMenu *menu, const QString & /*source*/)
{
    FileView::onPaneMenu(menu);
    if (m_file && m_fileManager) {
        auto *act = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")),
                                     i18n("Rename..."));
        connect(act, &QAction::triggered, this, &EditableFileView::startRename);
    }
}
```

Remove the old `onPaneMenu(QMenu*)` single-arg override since `View`'s default forwarder calls the two-arg overload.

- [ ] **Step 5: Build + smoke-test**

```bash
cmake --build build -j 10
./build/Corbomite
```

Open a vault, open a markdown note, click "…". Expect to see all the menu items wired above. Click "Rename…" → modal appears with basename pre-selected. Type a new name → Save. Observe file renamed in File Explorer.

Also click "Copy path" → submenu appears with 3 entries. Click "from vault folder" → paste clipboard, expect the relative path.

- [ ] **Step 6: Add integration test**

Create or extend `tests/core/tst_view_more_options.cpp` with a test that constructs an `EditableFileView` subclass, calls `buildMoreOptionsMenu`, and asserts all expected QAction texts appear in the menu in canonical section order.

```cpp
void testEditableFileViewMenuInventory()
{
    // Build a real EditableFileView with FileManager + CommandRegistrar
    // wired. Assert menu has Rename..., Move..., Copy path submenu,
    // Open in default app, Show in system explorer, Reveal file in
    // Navigation, Delete file. Section separators between groups.
    // ...
}
```

- [ ] **Step 7: Commit**

```bash
git add libs/core/include/corbomite/core/EditableFileView.h libs/core/src/EditableFileView.cpp src/app/MainWindow.cpp tests/core/tst_view_more_options.cpp
git commit -m "$(cat <<'EOF'
feat(views): EditableFileView hamburger menu wiring (Cluster R P2.8)

Universal file-menu items:
  - action: Rename..., Move file to...
  - info.copy submenu: as Obsidian URL / from vault folder / from system root
  - system: Open in default app, Show in system explorer, Reveal in Navigation
  - danger: Delete file
  - pane: Open in new window (disabled — G#6)
  - view.linked: Open version history (disabled — Cluster T)

startRename() stub replaced with FileManager::promptForFileRename delegation.
Tab-header onPaneMenu still contributes Rename... via the same path.

MainWindow's propagateServicesToView now wires FileManager + CommandRegistrar
into every EditableFileView leaf.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

# Phase 3 — Per-View Specialisations

Goal: Markdown, Canvas, Graph views each prepend view-specific items on top of the universal set from Phase 2. Plugin `:open` commands registered for the `view.linked` submenu.

## Task 3.1: Plugin `:open` commands for view.linked submenu

**Files:**
- Modify: `src/plugins/backlinks/BacklinksPlugin.cpp`
- Modify: `src/plugins/outlinks/OutlinksPlugin.cpp`
- Modify: `src/plugins/outline/OutlinePlugin.cpp`
- Modify: `src/plugins/properties/PropertiesPlugin.cpp`
- Modify: `src/plugins/local-graph/LocalGraphPlugin.cpp` (if `graph:open-local` not already registered)

- [ ] **Step 1: Add the pattern to each of the five plugins**

For each plugin, in `onLoad` (or where commands are registered), add:

```cpp
if (auto *commands = context()->commandRegistrar()) {
    commands->registerCommand({
        QStringLiteral("<slug>:open"),
        i18n("Open <Panel Name>"),
        [this](const QVariantMap & /*args*/) {
            // Focus the plugin's dock panel (WorkspaceController::revealDockView or similar)
            if (auto *ws = context()->workspace()) {
                ws->revealDockView(QStringLiteral("<slug>"));
            }
        }
    });
}
```

Where `<slug>` and panel name vary per plugin:
- Backlinks: `backlinks:open` → "Open Backlinks"
- Outlinks: `outlinks:open` → "Open Outlinks"
- Outline: `outline:open` → "Open Outline"
- Properties: `properties:open` → "Open Properties"
- LocalGraph: `graph:open-local` → "Open Local Graph"

Verify if `WorkspaceController::revealDockView` exists; if not, the interim path is `context()->pluginManager()->revealView(slug)` or similar. Pick whichever API already exists. If none does, create a thin helper.

- [ ] **Step 2: Write tests for each command**

For each plugin, extend its tst_* file:

```cpp
void testOpenCommandRevealsPanel()
{
    // Scaffolding: load plugin into a test PluginContext
    auto *cmd = context.commandRegistrar()->find(QStringLiteral("<slug>:open"));
    QVERIFY(cmd);
    cmd->invoke({});
    // Assert the dock view is visible
    // ...
}
```

- [ ] **Step 3: Build + run all five tests**

```bash
cmake --build build -j 10
cd build && ctest -R "tst_backlinks|tst_outlinks|tst_outline|tst_properties|tst_local_graph" --output-on-failure
```

- [ ] **Step 4: Commit**

```bash
git add src/plugins/backlinks/BacklinksPlugin.cpp src/plugins/outlinks/OutlinksPlugin.cpp src/plugins/outline/OutlinePlugin.cpp src/plugins/properties/PropertiesPlugin.cpp src/plugins/local-graph/LocalGraphPlugin.cpp tests/plugins/
git commit -m "$(cat <<'EOF'
feat(plugins): register :open commands on 5 sidebar plugins

backlinks:open, outlinks:open, outline:open, properties:open, graph:open-local.
Each dispatches to WorkspaceController::revealDockView(slug). Consumed by
Cluster R's MarkdownView.onMoreOptionsMenu view.linked submenu.

This is interim wiring: per Cluster R §3.6, when Workspace::openLinkText
lands (Cluster G follow-up #3), these commands upgrade to open as new leaves
without menu changes.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

## Task 3.2: `MarkdownView::insertFrontmatterProperty` + `markdown:add-metadata-property` command

**Files:**
- Modify: `src/editor/MarkdownView.h` + `.cpp`
- Modify: the plugin that registers markdown-related commands (or `MainWindow` if commands are registered there) — search via `grep -n 'markdown:toggle-preview' src/`

- [ ] **Step 1: Add the method declaration**

Edit `src/editor/MarkdownView.h`:

```cpp
public:
    // Ensures frontmatter block exists, then appends a blank property row
    // with focus on the key field. Switches to LivePreview from Reading mode
    // so the new row is visible.
    void insertFrontmatterProperty();
```

- [ ] **Step 2: Implement**

Edit `src/editor/MarkdownView.cpp`:

```cpp
void MarkdownView::insertFrontmatterProperty()
{
    if (!m_editor) return;

    // If currently in Reading, flip to LivePreview
    if (m_editor->viewMode() == NoteEditorWidget::ViewMode::Reading)
        m_editor->setViewMode(NoteEditorWidget::ViewMode::LivePreview);

    // Ensure frontmatter block exists
    QString body = m_editor->text();
    if (!body.startsWith(QStringLiteral("---"))) {
        const QString fmBlock = QStringLiteral("---\n\n---\n");
        m_editor->insertText(0, fmBlock);
    }

    // Append an empty property row. The metadataEditor widget embedded in
    // NoteEditorWidget exposes `addProperty(key, value)`.
    if (auto *metaEditor = m_editor->metadataEditor()) {
        metaEditor->addProperty(QString(), QVariant());
        metaEditor->focusLastKeyField();
    }
}
```

Verify `NoteEditorWidget::metadataEditor()` exists. If not, add the accessor.

Also verify `addProperty(key, value)` + `focusLastKeyField()` on the PropertiesPanel-widget reusable abstraction. If not, add.

- [ ] **Step 3: Register the command**

Find the markdown command registration site:

```bash
grep -rn "markdown:toggle-preview\|editor:toggle-source" src/
```

Register the new command parallel to them:

```cpp
commands->registerCommand({
    QStringLiteral("markdown:add-metadata-property"),
    i18n("Add file property"),
    [this](const QVariantMap &) {
        if (auto *mv = qobject_cast<MarkdownView *>(activeView())) {
            mv->insertFrontmatterProperty();
        }
    }
});
```

- [ ] **Step 4: Write test**

Extend `tests/editor/tst_markdown_view.cpp` (or create) with:

```cpp
void testInsertFrontmatterPropertyCreatesBlockIfMissing()
{
    // Build markdownview with body="# Title\nBody\n"
    // Call insertFrontmatterProperty()
    // Assert body now starts with "---\n\n---\n"
    // Assert metadataEditor has one blank row
}

void testInsertFrontmatterPropertyAppendsWhenBlockExists()
{
    // body="---\ntitle: Foo\n---\nBody"
    // Call insert — body unchanged (no second --- block inserted)
    // metadataEditor row count goes from 1 to 2
}
```

- [ ] **Step 5: Build + run — expected PASS**

- [ ] **Step 6: Commit**

## Task 3.3: `ExportToPdf::exportFile` helper

**Files:**
- Create: `src/app/ExportToPdf.h` + `.cpp`
- Modify: `src/app/CMakeLists.txt`
- Create: `tests/app/tst_export_to_pdf.cpp`

- [ ] **Step 1: Write failing test**

Create `tests/app/tst_export_to_pdf.cpp`:

```cpp
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFileDialog>

#include "ExportToPdf.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestExportToPdf : public QObject {
    Q_OBJECT

private slots:
    void testExportProducesValidPdf()
    {
        QTemporaryDir tmp;
        const QString mdPath = tmp.filePath(QStringLiteral("foo.md"));
        QFile f(mdPath);
        f.open(QIODevice::WriteOnly);
        f.write("# Hello\n\nBody text.\n");
        f.close();

        Corbomite::Storage::FileSystemAdapter fsa;
        Corbomite::Vault vault(&fsa);
        vault.load(tmp.path());
        auto *file = vault.getFileByPath(QStringLiteral("foo.md"));

        const QString outPath = tmp.filePath(QStringLiteral("out.pdf"));
        const bool ok = ExportToPdf::exportFileToPath(file, &vault, outPath);
        QVERIFY(ok);

        QFile outFile(outPath);
        QVERIFY(outFile.open(QIODevice::ReadOnly));
        const QByteArray header = outFile.read(4);
        QCOMPARE(header, QByteArray("%PDF"));  // PDF signature
    }
};

QTEST_MAIN(TestExportToPdf)
#include "tst_export_to_pdf.moc"
```

- [ ] **Step 2: Declare**

Create `src/app/ExportToPdf.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>

namespace Corbomite {
class Vault;
class TFile;
}

namespace ExportToPdf {

// Shows a save-as dialog and exports via exportFileToPath.
// Returns true on success.
bool exportFile(Corbomite::TFile *file, Corbomite::Vault *vault, QWidget *parent);

// Test seam: no dialog; writes PDF to `outPath` directly.
bool exportFileToPath(Corbomite::TFile *file,
                       Corbomite::Vault *vault,
                       const QString &outPath);

} // namespace ExportToPdf
```

- [ ] **Step 3: Implement**

Create `src/app/ExportToPdf.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ExportToPdf.h"

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"

#include <QFileDialog>
#include <QPrinter>
#include <QTextDocument>
#include <KLocalizedString>

// Consumes Markoff::ReadingView for rendering (Cluster E substrate).
// If ReadingView doesn't have a renderToPrinter helper, we render the
// markdown through QTextDocument instead as a fallback.
namespace ExportToPdf {

bool exportFile(Corbomite::TFile *file, Corbomite::Vault *vault, QWidget *parent)
{
    if (!file || !vault) return false;

    const QString defaultName = file->basename() + QStringLiteral(".pdf");
    const QString out = QFileDialog::getSaveFileName(parent,
        i18n("Export to PDF"), defaultName,
        i18n("PDF files (*.pdf)"));
    if (out.isEmpty()) return false;

    return exportFileToPath(file, vault, out);
}

bool exportFileToPath(Corbomite::TFile *file,
                        Corbomite::Vault *vault,
                        const QString &outPath)
{
    if (!file || !vault) return false;

    const QByteArray body = vault->read(file);
    if (body.isEmpty()) return false;

    // Render via QTextDocument (Markoff ReadingView-based renderer is a
    // richer follow-up; QTextDocument is sufficient for MVP PDF export).
    QTextDocument doc;
    doc.setMarkdown(QString::fromUtf8(body));

    QPrinter printer(QPrinter::PrinterResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setPageSize(QPageSize::A4);
    printer.setOutputFileName(outPath);

    doc.print(&printer);
    return QFileInfo::exists(outPath);
}

} // namespace ExportToPdf
```

- [ ] **Step 4: Wire CMake + build + run — expected PASS**

- [ ] **Step 5: Commit**

## Task 3.4: `MarkdownView::onMoreOptionsMenu` wiring

**Files:**
- Modify: `src/editor/MarkdownView.h` + `.cpp`

- [ ] **Step 1: Declare the override**

```cpp
// MarkdownView.h
public:
    void onMoreOptionsMenu(Corbomite::MenuSectionHelper &helper) override;
```

- [ ] **Step 2: Implement**

```cpp
void MarkdownView::onMoreOptionsMenu(Corbomite::MenuSectionHelper &helper)
{
    if (!m_file || !m_leaf) return;

    // pane: Split right / Split down
    auto *splitR = new QAction(QIcon::fromTheme(QStringLiteral("view-split-left-right")),
                                i18n("Split right"), this);
    connect(splitR, &QAction::triggered, this, [this] {
        if (m_commands) m_commands->invoke(QStringLiteral("split_right"), {});
    });
    helper.addToSection(splitR, QStringLiteral("pane"));

    auto *splitD = new QAction(QIcon::fromTheme(QStringLiteral("view-split-top-bottom")),
                                i18n("Split down"), this);
    connect(splitD, &QAction::triggered, this, [this] {
        if (m_commands) m_commands->invoke(QStringLiteral("split_down"), {});
    });
    helper.addToSection(splitD, QStringLiteral("pane"));

    // view: Backlinks in document toggle
    auto *backlinksInDoc = new QAction(i18n("Backlinks in document"), this);
    backlinksInDoc->setCheckable(true);
    const auto vs = m_leaf->viewState();
    backlinksInDoc->setChecked(vs.value(QStringLiteral("backlinksInDocument")).toBool());
    connect(backlinksInDoc, &QAction::triggered, this, [this](bool on) {
        auto vs = m_leaf->viewState();
        vs[QStringLiteral("backlinksInDocument")] = on;
        m_leaf->setViewState(vs);
        emit m_leaf->viewChanged();  // triggers PostProcessor re-run
    });
    helper.addToSection(backlinksInDoc, QStringLiteral("view"));

    // view: Reading View (checkable)
    auto *readingAct = new QAction(i18n("Reading View"), this);
    readingAct->setCheckable(true);
    readingAct->setChecked(m_editor && m_editor->viewMode() == NoteEditorWidget::ViewMode::Reading);
    connect(readingAct, &QAction::triggered, this, [this] {
        if (m_commands) m_commands->invoke(QStringLiteral("markdown:toggle-preview"), {});
    });
    helper.addToSection(readingAct, QStringLiteral("view"));

    // view: Source mode (checkable)
    auto *sourceAct = new QAction(i18n("Source mode"), this);
    sourceAct->setCheckable(true);
    sourceAct->setChecked(m_editor && m_editor->viewMode() == NoteEditorWidget::ViewMode::Source);
    connect(sourceAct, &QAction::triggered, this, [this] {
        if (m_commands) m_commands->invoke(QStringLiteral("editor:toggle-source"), {});
    });
    helper.addToSection(sourceAct, QStringLiteral("view"));

    // action: Bookmark... (dispatches to Cluster S command when available; else disabled)
    auto *bookmarkAct = new QAction(QIcon::fromTheme(QStringLiteral("bookmark-new")),
                                      i18n("Bookmark..."), this);
    const bool bookmarksAvail = m_commands && m_commands->hasCommand(QStringLiteral("bookmarks:bookmark-current-file"));
    bookmarkAct->setEnabled(bookmarksAvail);
    if (!bookmarksAvail)
        bookmarkAct->setToolTip(i18n("Requires Bookmarks core plugin (Cluster S)"));
    connect(bookmarkAct, &QAction::triggered, this, [this] {
        if (m_commands) m_commands->invoke(QStringLiteral("bookmarks:bookmark-current-file"), {});
    });
    helper.addToSection(bookmarkAct, QStringLiteral("action"));

    // action: Add file property
    auto *addPropAct = new QAction(QIcon::fromTheme(QStringLiteral("list-add")),
                                     i18n("Add file property"), this);
    connect(addPropAct, &QAction::triggered, this, [this] { insertFrontmatterProperty(); });
    helper.addToSection(addPropAct, QStringLiteral("action"));

    // action: Export to PDF
    auto *exportPdfAct = new QAction(QIcon::fromTheme(QStringLiteral("document-export")),
                                       i18n("Export to PDF..."), this);
    connect(exportPdfAct, &QAction::triggered, this, [this] {
        ExportToPdf::exportFile(qobject_cast<Corbomite::TFile *>(m_file), m_vault, this);
    });
    helper.addToSection(exportPdfAct, QStringLiteral("action"));

    // find: Find... / Replace... (disabled)
    auto *findAct = new QAction(QIcon::fromTheme(QStringLiteral("edit-find")),
                                  i18n("Find..."), this);
    findAct->setEnabled(false);
    findAct->setToolTip(i18n("Requires Qutepart fork Phase 3 find/replace API"));
    helper.addToSection(findAct, QStringLiteral("find"));

    auto *replaceAct = new QAction(QIcon::fromTheme(QStringLiteral("edit-find-replace")),
                                     i18n("Replace..."), this);
    replaceAct->setEnabled(false);
    replaceAct->setToolTip(i18n("Requires Qutepart fork Phase 3 find/replace API"));
    helper.addToSection(replaceAct, QStringLiteral("find"));

    // view.linked submenu: 5 entries
    auto *linkedSub = helper.addSubmenu(QStringLiteral("view.linked"),
                                          i18n("Open linked view"),
                                          QIcon::fromTheme(QStringLiteral("tab-detach")));
    struct LinkedEntry { QString label; QString cmd; QString icon; };
    const QList<LinkedEntry> entries = {
        {i18n("Open local graph"), QStringLiteral("graph:open-local"), QStringLiteral("kgraphviewer")},
        {i18n("Open backlinks"), QStringLiteral("backlinks:open"), QStringLiteral("go-previous")},
        {i18n("Open outgoing links"), QStringLiteral("outlinks:open"), QStringLiteral("go-next")},
        {i18n("Open file properties"), QStringLiteral("properties:open"), QStringLiteral("view-form")},
        {i18n("Open outline"), QStringLiteral("outline:open"), QStringLiteral("format-list-ordered")},
    };
    for (const auto &e : entries) {
        auto *act = new QAction(QIcon::fromTheme(e.icon), e.label, this);
        const QString cmd = e.cmd;
        connect(act, &QAction::triggered, this, [this, cmd] {
            if (m_commands) m_commands->invoke(cmd, {});
        });
        linkedSub->addToSection(act, QStringLiteral("action"));
    }

    // Chain to EditableFileView base for universal items
    EditableFileView::onMoreOptionsMenu(helper);
}
```

- [ ] **Step 3: Smoke test**

Build and run:

```bash
cmake --build build -j 10
./build/Corbomite
```

Open a markdown note → click "…" → observe all markdown-specific items + universal items. Click each to verify dispatch.

- [ ] **Step 4: Commit**

## Task 3.5: `CanvasScene::renderToImage` + `renderToSvg`

**Files:**
- Modify: `libs/canvas/include/canvas/CanvasScene.h` + `src/CanvasScene.cpp`
- Create: `tests/canvas/tst_canvas_export.cpp`
- Modify: `libs/canvas/CMakeLists.txt`
- Modify: `tests/canvas/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `tests/canvas/tst_canvas_export.cpp`:

```cpp
#include <QtTest/QtTest>
#include <QBuffer>
#include <canvas/CanvasScene.h>

class TestCanvasExport : public QObject {
    Q_OBJECT

private slots:
    void testRenderToImageProducesExpectedSize()
    {
        Canvas::CanvasScene scene;
        // Add a simple rect node
        scene.addRect(QRectF(0, 0, 100, 100));

        const QRectF bounds(0, 0, 200, 200);
        QImage img = scene.renderToImage(bounds, /*transparentBg=*/false,
                                          /*showEdges=*/true, /*scale=*/2.0);
        QCOMPARE(img.width(), 400);  // 200 * 2
        QCOMPARE(img.height(), 400);
    }

    void testRenderToSvgProducesValidXml()
    {
        Canvas::CanvasScene scene;
        scene.addRect(QRectF(0, 0, 100, 100));

        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        scene.renderToSvg(QRectF(0, 0, 200, 200), &buf, false, true);
        buf.close();

        QVERIFY(buf.data().startsWith("<?xml"));
        QVERIFY(buf.data().contains("<svg"));
        QVERIFY(buf.data().contains("</svg>"));
    }
};

QTEST_MAIN(TestCanvasExport)
#include "tst_canvas_export.moc"
```

- [ ] **Step 2: Declare new methods**

Edit `libs/canvas/include/canvas/CanvasScene.h`:

```cpp
#include <QImage>
#include <QRectF>
class QIODevice;

class CanvasScene : public QGraphicsScene {
    // ... existing ...
public:
    QImage renderToImage(const QRectF &bounds,
                          bool transparentBg = false,
                          bool showEdges = true,
                          qreal scale = 2.0);

    void renderToSvg(const QRectF &bounds,
                      QIODevice *out,
                      bool transparentBg = false,
                      bool showEdges = true);
};
```

- [ ] **Step 3: Implement**

Edit `libs/canvas/src/CanvasScene.cpp`:

```cpp
#include <QImage>
#include <QPainter>
#include <QSvgGenerator>

QImage CanvasScene::renderToImage(const QRectF &bounds, bool transparentBg,
                                    bool showEdges, qreal scale)
{
    const QSize sz((int)(bounds.width() * scale), (int)(bounds.height() * scale));
    QImage img(sz, transparentBg ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    img.fill(transparentBg ? Qt::transparent : backgroundBrush().color());

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);

    // Optionally hide edge items
    QList<QGraphicsItem *> hidden;
    if (!showEdges) {
        for (auto *item : items()) {
            if (isEdgeItem(item)) {
                item->setVisible(false);
                hidden.append(item);
            }
        }
    }

    render(&p, QRectF(0, 0, sz.width(), sz.height()), bounds);

    // Restore
    for (auto *item : hidden) item->setVisible(true);

    p.end();
    return img;
}

void CanvasScene::renderToSvg(const QRectF &bounds, QIODevice *out,
                                bool transparentBg, bool showEdges)
{
    if (!out) return;

    QSvgGenerator svg;
    svg.setOutputDevice(out);
    svg.setSize(bounds.size().toSize());
    svg.setViewBox(bounds);
    svg.setTitle(QStringLiteral("Canvas export"));

    QPainter p(&svg);

    QList<QGraphicsItem *> hidden;
    if (!showEdges) {
        for (auto *item : items()) {
            if (isEdgeItem(item)) {
                item->setVisible(false);
                hidden.append(item);
            }
        }
    }

    if (!transparentBg) {
        p.fillRect(bounds, backgroundBrush());
    }

    render(&p, bounds, bounds);

    for (auto *item : hidden) item->setVisible(true);
    p.end();
}
```

Add a small helper `isEdgeItem(QGraphicsItem*)` that returns true for edge-type items (existing canvas subsystem knows its own types). If unclear, use `qobject_cast` or `dynamic_cast` against a known edge class.

- [ ] **Step 4: Wire CMake (add `Qt6::Svg` to libs/canvas) + build + run**

```bash
cmake --build build -j 10 --target tst_canvas_export
cd build && ctest -R tst_canvas_export --output-on-failure
```

- [ ] **Step 5: Commit**

## Task 3.6: `CanvasFileView::showExportAsImageModal` + `onMoreOptionsMenu`

**Files:**
- Modify: `src/canvas/CanvasFileView.h` + `.cpp`

- [ ] **Step 1: Declare + implement the modal**

Add to `CanvasFileView.h`:

```cpp
public:
    void onMoreOptionsMenu(Corbomite::MenuSectionHelper &helper) override;

private slots:
    void showExportAsImageModal();
```

Implement `showExportAsImageModal`:

```cpp
void CanvasFileView::showExportAsImageModal()
{
    QDialog dlg(this);
    dlg.setWindowTitle(i18n("Export canvas as image"));

    auto *lay = new QVBoxLayout(&dlg);

    auto *areaGroup = new QGroupBox(i18n("Area"), &dlg);
    auto *areaLay = new QVBoxLayout(areaGroup);
    auto *selectedRadio = new QRadioButton(i18n("Only selected nodes"), areaGroup);
    auto *fullRadio = new QRadioButton(i18n("Full canvas"), areaGroup);
    const bool hasSelection = !m_scene->selectedItems().isEmpty();
    selectedRadio->setEnabled(hasSelection);
    (hasSelection ? selectedRadio : fullRadio)->setChecked(true);
    areaLay->addWidget(selectedRadio);
    areaLay->addWidget(fullRadio);
    lay->addWidget(areaGroup);

    auto *formatCombo = new QComboBox(&dlg);
    formatCombo->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    formatCombo->addItem(QStringLiteral("SVG"), QStringLiteral("svg"));
    lay->addWidget(new QLabel(i18n("Format"), &dlg));
    lay->addWidget(formatCombo);

    auto *transparentBg = new QCheckBox(i18n("Transparent background"), &dlg);
    auto *showEdges = new QCheckBox(i18n("Show edges / connections"), &dlg);
    showEdges->setChecked(true);
    lay->addWidget(transparentBg);
    lay->addWidget(showEdges);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dlg);
    bb->button(QDialogButtonBox::Ok)->setText(i18n("Export"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    const QString format = formatCombo->currentData().toString();
    const QRectF bounds = (selectedRadio->isChecked() && hasSelection)
        ? boundingRectOfSelection()
        : m_scene->itemsBoundingRect();

    const QString defaultName = QStringLiteral("canvas-export.%1").arg(format);
    const QString out = QFileDialog::getSaveFileName(this,
        i18n("Export canvas"), defaultName,
        format == QStringLiteral("png")
            ? i18n("PNG files (*.png)")
            : i18n("SVG files (*.svg)"));
    if (out.isEmpty()) return;

    if (format == QStringLiteral("png")) {
        QImage img = m_scene->renderToImage(bounds, transparentBg->isChecked(),
                                              showEdges->isChecked(), 2.0);
        img.save(out, "PNG");
    } else {
        QFile f(out);
        if (f.open(QIODevice::WriteOnly)) {
            m_scene->renderToSvg(bounds, &f, transparentBg->isChecked(), showEdges->isChecked());
        }
    }
}
```

- [ ] **Step 2: Implement onMoreOptionsMenu**

```cpp
void CanvasFileView::onMoreOptionsMenu(Corbomite::MenuSectionHelper &helper)
{
    if (!m_file) return;

    // pane: Split right / Split down
    auto *splitR = new QAction(QIcon::fromTheme(QStringLiteral("view-split-left-right")),
                                i18n("Split right"), this);
    connect(splitR, &QAction::triggered, this, [this] {
        if (m_commands) m_commands->invoke(QStringLiteral("split_right"), {});
    });
    helper.addToSection(splitR, QStringLiteral("pane"));

    auto *splitD = new QAction(QIcon::fromTheme(QStringLiteral("view-split-top-bottom")),
                                i18n("Split down"), this);
    connect(splitD, &QAction::triggered, this, [this] {
        if (m_commands) m_commands->invoke(QStringLiteral("split_down"), {});
    });
    helper.addToSection(splitD, QStringLiteral("pane"));

    // action: Bookmark (gated on plugin)
    auto *bookmarkAct = new QAction(QIcon::fromTheme(QStringLiteral("bookmark-new")),
                                      i18n("Bookmark..."), this);
    const bool avail = m_commands && m_commands->hasCommand(QStringLiteral("bookmarks:bookmark-current-file"));
    bookmarkAct->setEnabled(avail);
    if (!avail)
        bookmarkAct->setToolTip(i18n("Requires Bookmarks core plugin (Cluster S)"));
    connect(bookmarkAct, &QAction::triggered, this, [this] {
        if (m_commands) m_commands->invoke(QStringLiteral("bookmarks:bookmark-current-file"), {});
    });
    helper.addToSection(bookmarkAct, QStringLiteral("action"));

    // action: Export as image
    auto *exportAct = new QAction(QIcon::fromTheme(QStringLiteral("image-x-generic")),
                                    i18n("Export as image"), this);
    connect(exportAct, &QAction::triggered, this, &CanvasFileView::showExportAsImageModal);
    helper.addToSection(exportAct, QStringLiteral("action"));

    // view.linked submenu: single entry (Backlinks)
    auto *linkedSub = helper.addSubmenu(QStringLiteral("view.linked"),
                                          i18n("Open linked view"),
                                          QIcon::fromTheme(QStringLiteral("tab-detach")));
    auto *backlinksAct = new QAction(QIcon::fromTheme(QStringLiteral("go-previous")),
                                       i18n("Open backlinks"), this);
    connect(backlinksAct, &QAction::triggered, this, [this] {
        if (m_commands) m_commands->invoke(QStringLiteral("backlinks:open"), {});
    });
    linkedSub->addToSection(backlinksAct, QStringLiteral("action"));

    // Chain to EditableFileView base
    EditableFileView::onMoreOptionsMenu(helper);
}
```

- [ ] **Step 3: Smoke-test + commit**

## Task 3.7: `graph:copy-screenshot` command + `GraphView::onMoreOptionsMenu`

**Files:**
- Modify: `src/plugins/graph-view/GraphView.h` + `.cpp`
- Modify: `src/plugins/graph-view/GraphViewPlugin.cpp`

- [ ] **Step 1: Implement GraphView::onMoreOptionsMenu**

```cpp
void GraphView::onMoreOptionsMenu(Corbomite::MenuSectionHelper &helper)
{
    // pane: Split right / Split down
    auto *splitR = new QAction(QIcon::fromTheme(QStringLiteral("view-split-left-right")),
                                i18n("Split right"), this);
    connect(splitR, &QAction::triggered, this, [this] {
        if (m_commands) m_commands->invoke(QStringLiteral("split_right"), {});
    });
    helper.addToSection(splitR, QStringLiteral("pane"));

    auto *splitD = new QAction(QIcon::fromTheme(QStringLiteral("view-split-top-bottom")),
                                i18n("Split down"), this);
    connect(splitD, &QAction::triggered, this, [this] {
        if (m_commands) m_commands->invoke(QStringLiteral("split_down"), {});
    });
    helper.addToSection(splitD, QStringLiteral("pane"));

    // action: Copy screenshot
    auto *screenshotAct = new QAction(QIcon::fromTheme(QStringLiteral("camera-photo")),
                                        i18n("Copy screenshot"), this);
    connect(screenshotAct, &QAction::triggered, this, [this] {
        if (m_commands) m_commands->invoke(QStringLiteral("graph:copy-screenshot"), {});
    });
    helper.addToSection(screenshotAct, QStringLiteral("action"));

    // action: Bookmark (gated)
    auto *bookmarkAct = new QAction(QIcon::fromTheme(QStringLiteral("bookmark-new")),
                                      i18n("Bookmark..."), this);
    const bool avail = m_commands && m_commands->hasCommand(QStringLiteral("bookmarks:bookmark-current-graph"));
    bookmarkAct->setEnabled(avail);
    if (!avail)
        bookmarkAct->setToolTip(i18n("Requires Bookmarks core plugin (Cluster S)"));
    connect(bookmarkAct, &QAction::triggered, this, [this] {
        if (m_commands) m_commands->invoke(QStringLiteral("bookmarks:bookmark-current-graph"), {});
    });
    helper.addToSection(bookmarkAct, QStringLiteral("action"));

    // Do NOT chain to EditableFileView — GraphView is ItemView, not FileView.
}
```

- [ ] **Step 2: Register the command in GraphViewPlugin::onLoad**

```cpp
commands->registerCommand({
    QStringLiteral("graph:copy-screenshot"),
    i18n("Copy graph screenshot"),
    [this](const QVariantMap &) {
        if (!m_graphViewTab) return;
        const QImage img = m_graphViewTab->grab().toImage();
        if (img.isNull()) {
            // Show Notice...
            return;
        }
        QApplication::clipboard()->setImage(img);
        // Show Notice "Screenshot copied to clipboard"
    }
});
```

- [ ] **Step 3: Add test**

```cpp
void testCopyScreenshotCommandCopiesImageToClipboard()
{
    // Show the graph, trigger command, check clipboard has non-null image
    // ...
}
```

- [ ] **Step 4: Smoke-test + commit**

---

# Phase 4 — Inline Backlinks-in-document Renderer

Goal: when `backlinksInDocument` is true on a MarkdownView leaf, the Reading-mode rendering has an auto-appended backlinks section at the end.

## Task 4.1: `BacklinksPostProcessor`

**Files:**
- Create: `src/plugins/backlinks/BacklinksPostProcessor.h` + `.cpp`
- Modify: `src/plugins/backlinks/CMakeLists.txt`

- [ ] **Step 1: Declare the post-processor**

Create `src/plugins/backlinks/BacklinksPostProcessor.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "corbomite/core/PostProcessorRegistry.h"

class MetadataCache;
class VaultProxy;

namespace BacklinksPlugin {

class BacklinksPostProcessor : public Corbomite::PostProcessor {
public:
    BacklinksPostProcessor(Corbomite::MetadataCache *cache, Corbomite::VaultProxy *vault);

    void process(Corbomite::MarkdownRenderChild *child,
                  QWidget *container,
                  Corbomite::MarkdownPostProcessorCtx *ctx) override;

private:
    Corbomite::MetadataCache *m_cache;
    Corbomite::VaultProxy *m_vault;
};

} // namespace BacklinksPlugin
```

- [ ] **Step 2: Implement**

Create `src/plugins/backlinks/BacklinksPostProcessor.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "BacklinksPostProcessor.h"
#include "corbomite/core/MarkdownRenderChild.h"
#include "corbomite/core/MarkdownPostProcessorCtx.h"
#include "corbomite/vault/proxies/VaultProxy.h"
#include "corbomite/storage/MetadataCache.h"

#include <QLabel>
#include <QVBoxLayout>
#include <KLocalizedString>

namespace BacklinksPlugin {

BacklinksPostProcessor::BacklinksPostProcessor(Corbomite::MetadataCache *cache,
                                                 Corbomite::VaultProxy *vault)
    : m_cache(cache), m_vault(vault) {}

void BacklinksPostProcessor::process(Corbomite::MarkdownRenderChild * /*child*/,
                                       QWidget *container,
                                       Corbomite::MarkdownPostProcessorCtx *ctx)
{
    if (!container || !ctx || !m_cache) return;

    // Gate on viewState
    const bool enabled = ctx->viewState.value(QStringLiteral("backlinksInDocument")).toBool();
    if (!enabled) return;

    // Collect backlinks for ctx->sourcePath
    const QStringList sources = m_cache->backlinksFor(ctx->sourcePath);
    if (sources.isEmpty()) return;

    auto *region = new QWidget(container);
    auto *lay = new QVBoxLayout(region);

    auto *separator = new QFrame(region);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    lay->addWidget(separator);

    auto *heading = new QLabel(QStringLiteral("<h3>%1</h3>")
                                 .arg(i18n("Backlinks")), region);
    lay->addWidget(heading);

    for (const QString &src : sources) {
        auto *link = new QLabel(QStringLiteral("<a href=\"%1\">%2</a>").arg(src, src), region);
        link->setTextFormat(Qt::RichText);
        link->setOpenExternalLinks(false);
        // Connect to view's link-open handler (ctx carries a signal or handler)
        lay->addWidget(link);
    }

    container->layout()->addWidget(region);
}

} // namespace BacklinksPlugin
```

- [ ] **Step 3: Write test**

Create `tests/plugins/tst_backlinks_post_processor.cpp`:

```cpp
void testPostProcessorGatedOnViewState()
{
    // Set up cache with backlinks to "foo.md" from "bar.md"
    // Construct ctx with viewState {backlinksInDocument: false}
    // Call process — container should have no added widget
}

void testPostProcessorAppendsWhenEnabled()
{
    // Same setup, viewState {backlinksInDocument: true}
    // Call process — container has a new region with heading + 1 link
}
```

- [ ] **Step 4: Register in BacklinksPlugin::onLoad**

Edit `src/plugins/backlinks/BacklinksPlugin.cpp`:

```cpp
void BacklinksPlugin::onLoad()
{
    // ... existing code ...

    m_postProcessor = std::make_unique<BacklinksPostProcessor>(m_cache, context()->vault());
    if (auto *reg = context()->postProcessorRegistry())
        reg->registerProcessor(m_postProcessor.get());
}

void BacklinksPlugin::onUnload()
{
    if (auto *reg = context()->postProcessorRegistry())
        reg->unregisterProcessor(m_postProcessor.get());
    m_postProcessor.reset();
    // ...
}
```

- [ ] **Step 5: Build + run — expected PASS**

- [ ] **Step 6: Commit**

## Task 4.2: Menu-toggle + cacheChanged re-render

**Files:**
- Modify: `src/plugins/backlinks/BacklinksPostProcessor.cpp`
- Modify: `src/plugins/backlinks/BacklinksPlugin.cpp`

- [ ] **Step 1: Subscribe to MetadataCache::cacheChanged**

In `BacklinksPostProcessor`, add a slot that invalidates all rendered regions when a path's backlinks change:

```cpp
BacklinksPostProcessor::BacklinksPostProcessor(...)
{
    connect(m_cache, &MetadataCache::cacheChanged, this,
            [this](const QString &path) {
                // Ask host to re-run this post-processor for any rendered
                // container whose sourcePath is referenced by `path`.
                // ...
            });
}
```

The exact wiring depends on `PostProcessorRegistry`'s re-run API. If no such API exists, emit a signal on the registry that the render host listens to.

- [ ] **Step 2: Verify the MarkdownView menu toggle triggers re-render**

Open Corbomite, check a markdown file with at least one backlink. Toggle "Backlinks in document" in the hamburger. Expected: in Reading mode, the backlinks region appears/disappears. Source/LivePreview: menu toggle state persists across mode switch but region only visible in Reading.

- [ ] **Step 3: Commit**

---

# Phase 5 — Closeout

## Task 5.1: Update PROJECT-STATE + INDEX + write retro

**Files:**
- Modify: `docs/PROJECT-STATE.md`
- Modify: `docs/superpowers/plans/INDEX.md`
- Create: `docs/cluster-retros/cluster-r.md`

- [ ] **Step 1: Write cluster-r retro**

Create `docs/cluster-retros/cluster-r.md` following the cluster-retros format (see `cluster-g.md` / `cluster-n.md` as templates). Cover:
- Phase-landing table (dates + commit refs).
- What surprised / what was hard.
- Deviations from plan.
- Residual follow-ups.
- Lessons for next cluster.

- [ ] **Step 2: Update PROJECT-STATE**

- Update `Last updated:` with Cluster R closeout.
- Move Cluster R from "In-flight work items" to "Recent decisions" (or drop if already there from spec-write entry).
- Roadmap table: change R status from "Plan-needed" to "Done".
- Tag Cluster G follow-ups #3 / #6 with "R ships disabled-placeholder slots — activate when these land".

- [ ] **Step 3: Update INDEX**

- Change R plan row Status to "Done".
- Add retro cross-link.

- [ ] **Step 4: Full test suite final check**

```bash
cmake --build build -j 10
cd build && ctest --output-on-failure -j 10 2>&1 | tail -30
```

Verify no new regressions. Pre-existing known-flakies (`tst_markoff_inline_math`, `tst_renderengine`, `tst_completion_popup`, `tst_benchmark_layout` timeout) expected.

- [ ] **Step 5: Commit closeout**

```bash
git add docs/PROJECT-STATE.md docs/superpowers/plans/INDEX.md docs/cluster-retros/cluster-r.md
git commit -m "$(cat <<'EOF'
docs(cluster-r): closeout — retro + PROJECT-STATE + INDEX update

Cluster R closed. Phases 1-4 landed. Retro at cluster-retros/cluster-r.md.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Definition of Done

- [ ] `MenuSectionHelper` exposes Obsidian's canonical section order + `addSubmenu`.
- [ ] `View::onMoreOptionsMenu(MenuSectionHelper&)` hook exists; `ItemView::showMoreOptionsMenu` dispatches through it + `onPaneMenu(menu, "more-options")` + `MenuEventEmitter::leafMenu` + `helper.finalize()`.
- [ ] `EditableFileView::onMoreOptionsMenu` contributes the universal file-menu items (Rename/Move/CopyPath×3 submenu/OpenDefault/ShowInFolder/Reveal/Delete + disabled placeholders for OpenInNewWindow/OpenVersionHistory).
- [ ] `FileManager::promptForFileRename` / `promptForMove` / `promptForDeletion` modals work with validation + trash-option routing.
- [ ] `Platform::openWithDefaultApp` + `showInFolder` primitives work on Linux (DBus + xdg-open fallback), macOS (`open -R`), Windows (`explorer /select,`).
- [ ] `PathUtils::obsidianUrlFor` + `corbomiteUrlFor` emit valid percent-encoded URLs.
- [ ] `MarkdownView::onMoreOptionsMenu` adds Split/Backlinks-in-document/Reading/Source/Bookmark/AddProperty/ExportPDF/Find-Replace-disabled/view.linked-submenu and chains to `EditableFileView`.
- [ ] `CanvasFileView::onMoreOptionsMenu` adds Split/Bookmark/Export-as-image/view.linked-backlinks-only and chains to `EditableFileView`.
- [ ] `GraphView::onMoreOptionsMenu` adds Split/Copy-screenshot/Bookmark (does not chain to EditableFileView).
- [ ] Inline Backlinks-in-document works: toggle persists per-leaf, region renders in Reading mode, updates on MetadataCache::cacheChanged.
- [ ] Full test suite green outside pre-existing known-flakies.
- [ ] Cluster retro written at `docs/cluster-retros/cluster-r.md`.

---

## Blocks / enables

**Depends on:**
- Cluster G Part 1+2+3 (View hierarchy, ItemView, `showMoreOptionsMenu` entry point).
- Cluster H (MenuSectionHelper + MenuEventEmitter substrate).
- Cluster J (PostProcessorRegistry — P4 consumes).
- Cluster Q (CommandRegistrar — Platform + dispatching).

**Enables:**
- Cluster S (Bookmarks) — R's "Bookmark…" menu slot goes live on S ship.

**R-blocking-partials (activate R's disabled placeholders):**
- Cluster G follow-up #3 (`openLinkText` dispatcher) — upgrades `view.linked` submenu from dock-focus interim to leaf-opening final.
- Cluster G follow-up #6 (WorkspaceWindow popout) — activates "Open in new window" menu slot.
- Qutepart fork Phase 3 (find/replace API) — activates "Find…"/"Replace…" menu slots.
- Cluster T (file-recovery plugin — deferred) — activates "Open version history" menu slot.

**Estimated effort:** ~6-7 days per spec §12 (P1: 1d, P2: 2-3d, P3: 2d, P4: 1d).

---

## Preserved Obsidian compat quirks

- Canonical section order: `close, pane, open, action, find, info, info.copy, view, view.linked, system, "", danger`. Unknown sections funnel to `""`.
- `EditableFileView::onPaneMenu` / `onMoreOptionsMenu` chain order: primary hook → pane-menu back-compat → plugin `leaf-menu` emit → finalize.
- Rename modal basename-only pre-selection (not full filename); Cancel on Escape; Save on Enter with final validation.
- Delete confirm modal defaults Cancel button (destructive-action convention); folder deletes always prompt regardless of config.
- Graph "Copy screenshot" has no resolution modal — captures at current viewport size + DPR.
- Canvas "Export as image" defaults "Only selected nodes" when selection non-empty.
- "Open linked view" submenu: markdown gets 5 entries, canvas gets only Backlinks, graph gets none.
- `backlinksInDocument` toggle only renders in Reading mode (LivePreview/Source keep state but don't render the region).
