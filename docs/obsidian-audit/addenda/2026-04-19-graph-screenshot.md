# Graph internal plugin — "Copy screenshot" menu action (and preamble)

**Date:** 2026-04-19
**Discovered during:** Cluster R (View-header menus) scoping. The Graph view is an Obsidian internal plugin (`graph` for the global graph, `backlink` / separate for local graph). The core audit mentions it in `PLUGIN-API-SKETCH.md` and `FEATURE-MATRIX.md` but doesn't have a dedicated `domains/graph.md`. This addendum covers the plugin's "Copy screenshot" menu contribution; broader graph behaviour (force layout, filters, groups) is out of scope for now.
**Supersedes / extends:** No prior coverage as a dedicated plugin domain. Touches `PLUGIN-API-SKETCH.md` §"Internal plugins", `FEATURE-MATRIX.md` §Graph.
**Relevant cluster plans:** `superpowers/plans/2026-04-19-cluster-r-view-header-menus.md` (R P3 ships "Copy screenshot" in the GraphView menu).

---

## 1. Graph plugin preamble

**Plugin id:** `graph` (global graph) — enabled by default, non-removable. **Local graph** is a separate feature (command `graph:open-local`) hosted in the same plugin.

**View registration:** registers `GraphView` for `viewType === "graph"`. Main-area view (not a dock panel). Not tied to a file; opens via the `graph:open` command or the ribbon icon.

**Commands:**

| Command ID | Purpose |
|---|---|
| `graph:open` | Opens the global graph in the active leaf. |
| `graph:open-local` | Opens a local graph view scoped to the active file. |
| `graph:animate` | Toggles time-based node reveal animation. |
| `graph:copy-screenshot` | Copies the current graph rendering to clipboard. |

**Hamburger menu** (very short — see user's message for the list):

```
Split right
Split down
---
Copy screenshot
Bookmark...
```

Noticeably sparse compared to markdown/canvas because GraphView isn't a FileView (no rename/move/delete/path-copy — nothing to rename).

---

## 2. "Copy screenshot" action

- Command: `graph:copy-screenshot`.
- Menu item: "Copy screenshot" in section `action`, icon `lucide-camera`, no confirmation modal.
- Fires synchronously: captures the graph canvas into a bitmap at **current viewport size × current DPI** (no scaling modal; unlike Canvas Export, this is a quick-capture action).

**Capture pipeline:**
1. Render the graph's WebGL or 2D canvas to an offscreen `HTMLCanvasElement` at window-DPI ratio.
2. `canvas.toBlob({type:'image/png'})`.
3. Copy the blob to clipboard via `navigator.clipboard.write([new ClipboardItem({'image/png': blob})])`.
4. Show Notice "Screenshot copied to clipboard.".

Failures (clipboard API rejection, WebGL context lost, etc.) show a Notice "Could not copy screenshot.".

**Background handling:** Uses the current theme's graph background. No transparent-background toggle (unlike Canvas export).

**Resolution:** No export-resolution modal — whatever the viewport is. Users wanting higher DPI resize the window first.

---

## 3. Differences from Canvas "Export as image"

| Axis | Canvas | Graph |
|---|---|---|
| Output | File save-as | Clipboard only |
| Resolution modal | Yes (PNG 2×, SVG vector) | No |
| Area selection | Selected nodes or full | Full viewport only |
| Formats | PNG + SVG | PNG only |
| Edges toggle | Yes | No |
| Background toggle | Yes | No |

Graph is a lower-investment capture path; Canvas Export is a full authoring feature.

---

## 4. Implementation hints for Corbomite

- `GraphView::onMoreOptionsMenu` adds "Copy screenshot" to section `action`.
- Capture: `QImage img = m_graphViewTab->grab().toImage();` — `QWidget::grab` is synchronous and respects current device pixel ratio.
  - Note: `grab()` captures the widget's paintEvent output; for our ForceGraph-based GraphView which paints via `QGraphicsView`, `grab()` already renders the scene. No need for an explicit `QGraphicsScene::render` call.
- Clipboard: `QApplication::clipboard()->setImage(img)`.
- Notice: use the existing `Notice` primitive from Cluster H.
- Failure mode: if `grab()` returns a null image (can happen when widget is not yet realized), show "Could not copy screenshot — graph not ready." Notice and bail.
- No new primitive on libs/forcegraph needed; everything composes from Qt-standard APIs.
- Menu item is always enabled when a GraphView is active; no disabled state.
