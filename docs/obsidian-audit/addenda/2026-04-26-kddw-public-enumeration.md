# Audit addendum — KDDW 2.4 public layout-enumeration API

**Corrects:** `docs/audit-2026-04-26/workspace.md:14-15` and `libs/core/include/corbomite/core/Workspace.h:267-271`.

**Date:** 2026-04-26
**Source:** `kddockwidgets` 2.4.0-2 headers under `/usr/include/kddockwidgets-qt6/`.

The audit (and code comments echoing it) claim KDDW exposes no public Group/Frame enumeration API. This is **stale**; KDDW 2.4 ships the following as public interfaces:

| API | Header | Returns |
|---|---|---|
| `KDDockWidgets::Core::MainWindow::layout()` | `core/MainWindow.h:251` | `Core::Layout *` |
| `KDDockWidgets::Core::Layout::groups()` | `core/Layout.h:171` | `Vector<Core::Group *>` |
| `KDDockWidgets::Core::Layout::rootItem()` | `core/Layout.h:194` | `Core::ItemContainer *` (forward-declared; concrete header private) |
| `KDDockWidgets::Core::Layout::dockWidgets()` | `core/Layout.h:174` | `Vector<Core::DockWidget *>` |
| `KDDockWidgets::Core::DropArea::groups()` | `core/DropArea.h:65` | `Vector<Core::Group *>` |
| `KDDockWidgets::Core::Group::currentDockWidget()` | `core/Group.h:89` | `DockWidget *` |
| `KDDockWidgets::Core::Group::currentTabIndex()` | `core/Group.h:184` | `int` |
| `KDDockWidgets::Core::Group::dockWidgets()` | `core/Group.h:107` | `Vector<DockWidget *>` |
| `KDDockWidgets::Core::Group::dockWidgetAt(int)` | `core/Group.h:86` | `DockWidget *` |
| `KDDockWidgets::Core::Group::layoutItem()` | `core/Group.h:204` | `Core::Item *` (opaque pointer; `Core::Item` is forward-declared, no public header) |
| `KDDockWidgets::Core::Group::serialize()` | `core/Group.h:52` | `LayoutSaver::Group` |
| `KDDockWidgets::LayoutSaver::serializeLayout()` | `LayoutSaver.h:84` | `QByteArray` (KDDW-shape JSON document) |

`KDDockWidgets::QtWidgets::MainWindow` exposes the underlying controller via `mainWindow()` (inherited from `Core::MainWindowViewInterface::mainWindow() const`, defined at `core/views/MainWindowViewInterface.h:37`), so callers go: `qtWidgetsMain->mainWindow()->layout()->groups()`.

**Caveats:**

- `Core::Item` and `Core::ItemContainer` headers are not in the public include tree. For external consumers, `Layout::groups()` is the canonical enumeration of live groups; the *split topology* between groups is recovered by parsing `LayoutSaver::serializeLayout()` JSON output instead of walking `ItemContainer` directly.
- Calling `KDDockWidgets::QtWidgets::MainWindow::layout()` returns `QLayout *` (the QWidget method) rather than `Core::Layout *` — the latter requires the `Core::MainWindow *` indirection.

## KDDW LayoutSaver JSON schema

The `LayoutSaver::serializeLayout()` JSON shape is documented in [`docs/superpowers/specs/2026-04-26-kddw-layoutsaver-shape.md`](../../superpowers/specs/2026-04-26-kddw-layoutsaver-shape.md) (companion artifact captured during the workspace serializer consolidation). Key path: `mainWindows[i].multiSplitterLayout.layout` is the recursive split tree; `mainWindows[i].multiSplitterLayout.frames[<id>]` is the flat dict of group/frame metadata indexed by `guestId`. `floatingWindows[i]` mirrors the same shape with its own `multiSplitterLayout`.

## Implications for Corbomite

1. The audit's "no public enumeration" caveat does not block recursive split-tree introspection.
2. `m_tabGroupOf` lag-after-drag (audit `workspace.md` §"High severity" #1) is a follow-up addressable via `Layout::groups()` rather than an architectural blocker — tracked as a punch-list entry post-consolidation.
3. `WorkspaceSerializer` now uses `LayoutSaver::serializeLayout()` JSON as the source of truth for split topology, joined with `Workspace::findLeafById()` for per-leaf state — see [`docs/superpowers/specs/2026-04-26-workspace-serializer-consolidation-design.md`](../../superpowers/specs/2026-04-26-workspace-serializer-consolidation-design.md).
4. Per-group `currentTab` round-trips correctly because `Layout::groups()` exposes per-group `currentTabIndex()` directly.
