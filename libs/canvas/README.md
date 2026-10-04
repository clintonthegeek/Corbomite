# libcanvas

A standalone Qt6/C++ canvas editing library compatible with JSON Canvas 1.0 (jsoncanvas.org).

## Features

- JSON Canvas 1.0 format read/write
- Text cards with inline markdown editing
- Groups with labeled containment
- Edges with directional arrows and labels
- QGraphicsView-based interactive editor
- Pan, zoom, resize, undo/redo
- Tool state machine (select, create card, create edge)
- `ConvertTextCardToFile()`: re-type a text card as a file card in one undo step
- `groupJumpTargets()` + `CanvasView::zoomToBbox()`: Obsidian's "Jump to group" camera

## On-disk format

`CanvasDocument::saveToFile()` writes byte-identical output to Obsidian 1.13.7:
tab indent, all-primitive containers collapsed onto one compact line, no space
after the colon, no trailing newline, `nodes`/`edges` key order matching each
class's `getData()`. `CanvasDocument::serializeObsidianStyle()` exposes those
bytes; `toJson()` returns the same document as a plain `QJsonObject`, whose key
order Qt sorts and which therefore cannot be used for byte comparison.

`.canvas` is a vault format, so the serializer is lossless by design:

- A node `type` this build does not model keeps its whole source object
  (`NodeType::Unknown`). Upstream deletes it on the next save.
- An edge with an unresolvable endpoint keeps its whole source object. Upstream
  deletes it on the next save.
- Keys Corbomite does not model are preserved, as are keys the source file
  spelled out even when they hold the default value (`"fromEnd":"none"`,
  `"color":""`, `"backgroundStyle":"cover"`, …).

The one deviation: a preserved payload arrives as a `QJsonObject`, and Qt sorts
its keys when parsing, so unmodelled keys come back alphabetically instead of in
file order. Every key Corbomite models is emitted in Obsidian's exact position.
Key order is stable from then on, so this never ping-pongs between the two apps.

Source of truth: `docs/obsidian-audit/addenda/2026-10-03-canvas-disk-contract-and-viewport-settings.md`.
Contract tests: `tests/tst_canvas_diskcontract.cpp`.

## License

GPL-3.0-or-later
