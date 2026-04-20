# Graph View — Blocked Features & Prerequisites

Features that are ready to implement in the graph view once their prerequisites are built by other sessions.

## Blockers (work for other sessions)

### 1. Multi-Window Support
**Status:** Not started
**What's needed:** Ability to open a second MainWindow instance on the same vault.
**Unlocks:**
- Graph context menu: "Open in new window"

### 2. File Move Dialog
**Status:** Not started
**What's needed:** A dialog that lets the user pick a destination folder within the vault and moves the file (updating all wikilinks that reference it via `SQLiteIndex::repairLinks`).
**Unlocks:**
- Graph context menu: "Move file to..."

### 3. Bookmark System
**Status:** Not started
**What's needed:** A persistent list of bookmarked notes (stored in vault config or session), with a sidebar panel to browse them.
**Unlocks:**
- Graph context menu: "Bookmark..."

### 4. Note Merge UI
**Status:** Not started
**What's needed:** A dialog/workflow to merge the content of one note into another, with conflict resolution or append mode.
**Unlocks:**
- Graph context menu: "Merge entire file with..."

### 5. Tag Nodes in GraphDataBuilder
**Status:** Not started
**What's needed:** `GraphDataBuilder::buildGlobalGraph` should optionally create nodes for tags (from `SQLiteIndex::allTags`) and edges from notes to their tags. Requires a node type field or color convention to distinguish tag nodes from note nodes.
**Unlocks:**
- Graph controls: "Tags" filter toggle (show/hide tag nodes)
- Richer graph topology showing tag relationships

### 6. Attachment Node Support
**Status:** Not started
**What's needed:** `VaultScanner` should track non-markdown files (images, PDFs) as attachments. `GraphDataBuilder` should optionally create nodes for attachments referenced via `![[embed]]` links.
**Unlocks:**
- Graph controls: "Attachments" filter toggle (show/hide attachment nodes)

### 7. Local Graph as Tab
**Status:** Partially done (LocalGraphPanel exists as sidebar widget)
**What's needed:** Ability to open a local graph centered on a specific note as a full editor tab (like the global graph tab), not just the sidebar panel. Probably means `LocalGraphTab` analogous to `GraphViewTab` but taking a center note path.
**Unlocks:**
- Graph context menu: "Open linked view"

## Completed

- **Hover preview** — rich tooltip on node hover: bold title, link count, tags (italic). Built in GraphDataBuilder from index data.
- **Smooth animations** — animated zoomToFit/zoomToNode (300ms InOutCubic via QTimeLine), hover enlargement 1.1x, connected edge brightening (alpha 180, width 1.5x).
- **Node styling by type** — NodeType enum (Regular/Hub/Orphan/Unresolved/DailyNote). Hub nodes get a soft glow halo. Unresolved links get dashed borders. Daily notes (YYYY-MM-DD) get teal color.
- **Theme-aware colors** — colorForType() reads QPalette to detect dark/light theme and selects appropriate colors. No more hardcoded color values in GraphDataBuilder.
- **Semantic zoom** — high-degree node labels appear first at medium zoom; lower-degree labels fade in progressively.
- **Parallax text** — label font grows at 3/4 the rate of graph zoom, creating a layered depth effect.
- **Search zoom** — search field zooms to first matching node instead of just dimming.
- **Edge styling** — lower default opacity (alpha 60), dimmed at alpha 20. Three-tier system: highlighted/normal/dimmed.
- **Debug cleanup** — removed graph dump debug output from GraphViewTab.
- **Division-by-zero guard** — text fade threshold clamped to minimum 0.01.

## Remaining (no blockers)

- **Register graph keybindings as KDE actions** — Home (zoom to fit), +/= (zoom in), - (zoom out) are currently hardcoded in `ForceGraphView::keyPressEvent`. They should be proper `QAction`s registered via the KXmlGui action collection so they appear in the KDE Keyboard Shortcuts dialog, can be customized by the user, and can be added to toolbars. Create the actions in `GraphViewTab`, wire them to `ForceGraphView` methods.
- **Color-blind safe palette** — test with deuteranopia simulation, ensure 3:1 contrast ratio against background for all node colors, 4.5:1 for text (WCAG AA).
- **Filter fade transitions** — animate node opacity when filters add/remove nodes (fade in 300ms, fade out 200ms) instead of instant rebuild.
