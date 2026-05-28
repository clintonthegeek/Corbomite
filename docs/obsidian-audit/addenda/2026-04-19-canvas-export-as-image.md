# Canvas internal plugin — "Export as image" menu action (and preamble)

**Date:** 2026-04-19
**Discovered during:** Cluster R (View-header menus) scoping. Canvas is an Obsidian internal plugin. The core audit never covered it as a dedicated domain — it's name-dropped in `domains/views.md` (via `.canvas` extension → `CanvasView`) and in `VAULT-FORMAT.md` (`.canvas` JSON schema) but no `domains/canvas.md` exists. This addendum surfaces the plugin shell + its menu contributions; deeper Canvas behaviour (the node graph, edge routing, groupings) remains out of scope for this addendum and should land as a dedicated `domains/canvas.md` in a future audit expansion. **Done 2026-05-27 — the full canvas domain audit now lives at [`../domains/canvas.md`](../domains/canvas.md)** (node/edge/index/viewport internals, the `.canvas` disk contract, the Graffodil library-vs-consumer split, and the HTML/CSS free-features tradeoffs); this addendum's menu-action detail stands.
**Supersedes / extends:** Touches `domains/views.md` (`.canvas` extension mapping), `VAULT-FORMAT.md §Canvas`. No prior coverage as a dedicated plugin domain.
**Relevant cluster plans:** `superpowers/plans/2026-04-19-cluster-r-view-header-menus.md` (R P3 ships the "Export as image" menu item + underlying render-to-image primitive).

---

## 1. Canvas plugin preamble

**Plugin id:** `canvas`. Enabled by default. Non-removable (core plugin).

**View registration:** registers `CanvasView` for the `.canvas` extension. `CanvasView extends TextFileView` — the `.canvas` file is a JSON text file (schema covered in `VAULT-FORMAT.md`), so it rides the standard text-file lifecycle (debounced save, three-way merge, etc.) even though its render is a graphical node canvas.

**Commands registered:**

| Command ID | Purpose |
|---|---|
| `canvas:new-file` | Creates a new `.canvas` file in the new-file-parent folder. |
| `canvas:export-as-image` | Triggers the export-as-image modal for the active canvas. |
| `canvas:jump-to-group` | Opens a suggester to jump viewport to a named group. |
| `canvas:convert-to-file` | Converts a text-card node into a standalone `.md` file with an embed. |

**Hamburger menu items** (markdown-view menu minus markdown-specific ones, plus canvas-specific ones). The full list is enumerated in Corbomite's `docs/superpowers/specs/2026-04-19-cluster-r-view-header-menus-design.md` §"Per-view menu inventory — Canvas".

---

## 2. "Export as image" modal

Opened from:
- `canvas:export-as-image` command.
- Canvas hamburger menu → "Export as image" (section `action`).

**Modal shape:**

```
┌────────────────────────────────────────────┐
│  Export canvas as image               [X]  │
├────────────────────────────────────────────┤
│  Area                                      │
│  (●) Only selected nodes                   │  ← radio
│  ( ) Full canvas                           │
│                                            │
│  Format                                    │
│  [▼ PNG          ]                         │  ← combo: PNG, SVG
│                                            │
│  [ ] Transparent background                │
│  [ ] Show edges / connections              │
│                                            │
│                  [ Cancel ]  [ Export ]    │
└────────────────────────────────────────────┘
```

- **Area:** "Only selected nodes" is enabled only when the canvas has a non-empty selection; otherwise forced to "Full canvas".
- **Format:** PNG rasters at 2× density (Retina-friendly); SVG preserves vector for printable/scalable output.
- **Transparent background:** when unchecked, fills with the canvas's theme-appropriate background colour.
- **Show edges / connections:** when unchecked, renders only nodes (no edge paths).
- **Export:** opens a save-as dialog, writes the image to the chosen location (outside the vault by default; in-vault is allowed but unusual).
- **Cancel:** closes without write.

---

## 3. Rendering pipeline

**PNG path:**
1. Compute bounding box: union of selected-node positions + sizes (or all nodes if area=full).
2. Apply edge padding (20px).
3. Render canvas viewport to an offscreen HTML Canvas 2D context at 2× density — reuses Obsidian's existing canvas-node draw functions but against an offscreen context.
4. Embedded nodes (markdown, image, PDF) are rendered via their respective view's `export` path if available; a plain `.md` embed renders its ReadingView to HTML then pipes through the same HTML→image pipeline used for the main canvas view.
5. `canvas.toBlob({type:'image/png'})` → save.

**SVG path:**
1. Same bounding box computation.
2. Emits SVG with one `<g>` per node, edges as `<path>`, text as `<text>` or embedded-HTML-via-foreignObject when rich content is present.
3. Backgrounds and shadows as SVG filters.
4. `<svg>` serialised → save.

Both paths are synchronous from the user's perspective; a progress bar appears only if the rendering exceeds ~500 ms (large canvases with many embeds).

---

## 4. Implementation hints for Corbomite

- `CanvasFileView::onMoreOptionsMenu` adds "Export as image" to section `action`.
- New primitive on `Corbomite::Canvas::CanvasScene` (libs/canvas): `QImage renderToImage(const QRectF &bounds, bool transparentBg, bool showEdges, qreal scale)` — uses `QGraphicsScene::render(QPainter*)` over a `QImage` constructed with `QImage::Format_ARGB32` or `QImage::Format_RGB32`.
- SVG variant: `void renderToSvg(const QRectF &bounds, QIODevice *output, bool transparentBg, bool showEdges)` using `QSvgGenerator`.
- Save dialog via `QFileDialog::getSaveFileName` with PNG/SVG filter.
- Modal widget: custom `QDialog` with radio buttons + combo + checkboxes.
- No frontmatter or vault-adjacent state required — `.canvas` file itself isn't modified.
