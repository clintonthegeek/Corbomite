# Graph View Controls Panel & Context Menu — Design Specification

## Overview

Add a floating controls panel to the graph view with three collapsible sections (Filters, Display, Forces) and a right-click context menu on graph nodes. Makes the graph view interactive and useful rather than a static visualization.

## Part 1: Controls Panel

A `QFrame` floating over the top-right corner of `ForceGraphView`, semi-transparent background, rounded corners, max-width ~220px.

### Structure

Three collapsible sections, all starting collapsed. Each section has a clickable header that toggles its content visibility.

**Panel header row:** Section title on the left, reset button (circular arrow icon — resets all controls to defaults and restarts simulation) and close X button on the right. Close hides the panel; a small toggle button on the graph view re-shows it.

### Filters Section

- **Search field:** `QLineEdit` with placeholder "Search files...". Filters nodes by name as the user types (300ms debounce). Non-matching nodes dim to alpha 0.15 (same dimming as the existing hover-highlight system). Edges to dimmed nodes also dim. Empty search shows everything.
- **Existing files only toggle:** `QCheckBox`. When checked, removes unresolved nodes (gray "linked but not created" nodes) and their edges from the scene entirely. Default: unchecked.
- **Orphans toggle:** `QCheckBox`. When checked, shows orphan nodes (0 connections). When unchecked, hides them. Default: checked (matching Obsidian).
- `// TODO: Tags toggle — show/hide tag nodes (requires tag node support in GraphDataBuilder)`
- `// TODO: Attachments toggle — show/hide attachment nodes (requires attachment node support in GraphDataBuilder)`

**Filter application:** Filters rebuild the visible node set via a post-processing step in `GraphViewTab` after `GraphDataBuilder` produces the full graph. Force layout re-runs after filter changes.

### Display Section

- **Arrows toggle:** `QCheckBox`. When checked, edges render with directional arrowheads at the target end. Default: unchecked.
- **Text fade threshold slider:** `QSlider` range 0.0–3.0, default 1.0. Controls the LOD level at which node labels appear. Maps to the existing LOD check in `ForceGraphNode::paint()`.
- **Node size slider:** `QSlider` range 0.5–3.0, default 1.0. Scale factor applied to all node radii.
- **Link thickness slider:** `QSlider` range 0.5–3.0, default 1.0. Scale factor applied to edge pen width.
- **Animate button:** `QPushButton`. Restarts the force simulation — re-randomizes node positions and re-runs layout.

### Forces Section

All sliders call existing setters on `ForceLayout` and restart the simulation.

- **Centre force:** `QSlider` range 0.0–0.05, default 0.01. Maps to `ForceLayout::setCenterForce()`.
- **Repel force:** `QSlider` range 0–5000, default 1500. Maps to `ForceLayout::setRepelForce()`.
- **Link force:** `QSlider` range 0.0–0.2, default 0.05. Maps to `ForceLayout::setLinkForce()`.
- **Link distance:** `QSlider` range 20–300, default 100. Maps to `ForceLayout::setLinkDistance()`.

### Implementation

**New class: `GraphControlsPanel`** — lives in `src/graph/`. A `QFrame` subclass with `QVBoxLayout` containing collapsible section widgets.

**Collapsible sections:** Each section is a `QWidget` with a clickable `QToolButton` header (arrow icon toggles) and a content `QWidget` that shows/hides. This is a common Qt pattern — no need for a custom framework.

**Positioning:** The panel is a child of `ForceGraphView` (not part of the scene). Anchored to the top-right via a layout or manual positioning in `resizeEvent`. Stays fixed relative to the view, not the scene.

**Toggle button:** When the panel is closed, a small button (gear or settings icon) appears in the top-right corner to re-show it.

**Wiring:**
- Force sliders → `ForceLayout` setters + `ForceLayout::start()`
- Display sliders → `ForceGraphScene` (node size scale, edge width scale, text threshold, arrows toggle)
- Filter changes → `GraphViewTab::applyFilters()` which rebuilds the visible graph
- Reset → restore all defaults, restart simulation

## Part 2: Right-Click Context Menu

A `QMenu` shown on right-click on a `ForceGraphNode`.

### Menu Structure

```
Note Name (disabled header)
─────────────────────────
Open in new tab
// TODO: Open in new window (requires multi-window support)
─────────────────────────
// TODO: Move file to... (requires file move dialog)
// TODO: Bookmark... (requires bookmark system)
// TODO: Merge entire file with... (requires merge UI)
─────────────────────────
Copy path                  ▸
  Copy vault path
  Copy absolute path
// TODO: Open linked view (requires local graph as tab)
─────────────────────────
Open in default app
Show in system explorer
Reveal file in navigation
─────────────────────────
Delete file
```

### Implemented Actions

- **Open in new tab:** Emits signal with note path. `GraphViewTab` forwards to MainWindow which splits the active pane and opens the note.
- **Copy vault path:** Copies the vault-relative path to `QClipboard`.
- **Copy absolute path:** Copies the full filesystem path to `QClipboard`.
- **Open in default app:** `QDesktopServices::openUrl(QUrl::fromLocalFile(absolutePath))`.
- **Show in system explorer:** `QDesktopServices::openUrl(QUrl::fromLocalFile(parentDirectory))`.
- **Reveal file in navigation:** Emits signal with note path. MainWindow selects the file in the FileExplorerPanel tree view.
- **Delete file:** Shows `KMessageBox` confirmation dialog, then deletes via `NoteService::deleteNote()`. Removes the node from the graph.

### Stubbed Actions (TODO comments)

Each unimplemented action is present as a commented-out `menu.addAction()` call with a TODO explaining what infrastructure it needs:
- Open in new window — requires multi-window support
- Move file to... — requires file move/rename dialog
- Bookmark... — requires bookmark system
- Merge entire file with... — requires merge UI
- Open linked view — requires opening local graph as a tab

### Signal Flow

```
ForceGraphScene::contextMenuEvent()
  → builds QMenu with node data
  → user selects action
  → emits signal (e.g., openNoteRequested, revealInTreeRequested, deleteNoteRequested)
    → ForceGraphView forwards
      → GraphViewTab forwards
        → MainWindow handles (open tab, reveal in tree, delete)
```

The scene doesn't know about vaults, files, or the app — it only knows node IDs and paths. All app-level logic happens in the signal handlers up the chain.

## What This Does NOT Include

- **Groups section** (color-coded query groups) — Tier 2 feature
- **Tag nodes / attachment nodes** — requires GraphDataBuilder changes, TODO stubs in filter toggles
- **Persistent layout** (save node positions) — Tier 3
- **Open in new window** — requires multi-window architecture
- **Move/Bookmark/Merge actions** — require dedicated UI features

## Testing

### Unit Tests

**tst_graphcontrolspanel.cpp (new):**
- Panel starts with all sections collapsed
- Clicking section header toggles content visibility
- Reset button restores all sliders to default values
- Close button hides panel, toggle button re-shows it
- Search field debounces (signal not emitted until 300ms after last keystroke)

### Integration Tests

- Force slider change → ForceLayout parameters updated, simulation restarts
- Search filter → matching nodes bright, non-matching dimmed
- "Existing files only" toggle → unresolved nodes removed/restored
- Orphans toggle → orphan nodes hidden/shown
- Node size slider → all nodes scale proportionally
- Right-click node → menu appears with correct note name
- "Open in new tab" → note opens in editor
- "Delete file" → confirmation dialog, node removed from graph
- "Reveal file in navigation" → file tree scrolls to and selects the file
