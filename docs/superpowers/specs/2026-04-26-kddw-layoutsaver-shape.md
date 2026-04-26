# KDDW LayoutSaver JSON shape (v3)

**Probe source:** fixture 03 (3 leaves, vertical split with nested horizontal split inside the bottom child).
**KDDW package:** `kddockwidgets` 2.4.0-2.
**`serializationVersion`:** 3.

## Top-level

```json
{
  "serializationVersion": 3,
  "screenInfo": [...],
  "allDockWidgets": [...],   // flat list of every dock widget across mains+floats
  "closedDockWidgets": null,
  "floatingWindows": [],     // each entry has its own multiSplitterLayout (same shape)
  "mainWindows": [...]
}
```

## `mainWindows[i]`

```json
{
  "uniqueName": "test-probe",       // matches MainWindow::uniqueName()
  "geometry": {x,y,width,height},
  "isVisible": true,
  "multiSplitterLayout": {
    "frames": { "<id>": <Frame>, ... },   // flat dict of all groups, keyed by stringified numeric id
    "layout": <LayoutNode>                 // recursive split tree (the topology we care about)
  },
  "normalGeometry": {...},
  "options": 0,
  "screenIndex": 0,
  "screenSize": {...},
  "windowState": 0,
  "affinities": null
}
```

## `LayoutNode` (recursive)

Two flavors, discriminated by `isContainer`:

### Container (split node)

```json
{
  "isContainer": true,
  "isVisible": <bool>,
  "orientation": <int>,    // Qt::Horizontal=1, Qt::Vertical=2
  "children": [<LayoutNode>, ...],
  "sizingInfo": { ... }     // geometry + min/max + percentageWithinParent (split flex-grow)
}
```

`orientation: 2` (`Qt::Vertical`) → children laid out top-to-bottom → Obsidian `"direction": "vertical"`.
`orientation: 1` (`Qt::Horizontal`) → children laid out left-to-right → Obsidian `"direction": "horizontal"`.

### Leaf (group reference)

```json
{
  "isContainer": false,
  "isVisible": <bool>,
  "guestId": "<id>",        // string-form of the numeric Frame id; lookup in multiSplitterLayout.frames
  "sizingInfo": { ... }
}
```

## `Frame` (group)

Dict keyed by stringified numeric id (e.g. `"108"`). Each entry:

```json
{
  "id": "108",
  "objectName": "leaf02aaaaaaaaaa",   // == dockWidgets[0].uniqueName
  "currentTabIndex": 0,
  "dockWidgets": ["leaf02aaaaaaaaaa", ...],   // unique-name strings only
  "geometry": {...},
  "mainWindowUniqueName": "test-probe",
  "isNull": false,
  "options": 0
}
```

## `floatingWindows[i]`

Same shape as `mainWindows[i]` minus a few fields; carries its own `multiSplitterLayout` so the same recursive walker handles popouts symmetrically. Geometry comes from the FloatingWindow's own `geometry` block.

## Walker recipe

```
walk(LayoutNode node):
    if node.isContainer:
        emit SplitNode(direction = (node.orientation == 2) ? "vertical" : "horizontal")
        for child in node.children: recurse
    else:
        guestId = node.guestId
        frame = mainWindow.multiSplitterLayout.frames[guestId]
        emit TabsNode(currentTab = frame.currentTabIndex,
                      children = [LeafNode(id = uniqueName) for uniqueName in frame.dockWidgets])
```

For per-group `currentTab` we **read from the live `Core::Group::currentTabIndex()`** rather than `frame.currentTabIndex`, because the Frame's `currentTabIndex` field reflects the *serialized* state at the time of `LayoutSaver::serializeLayout()`, which can lag if the user just clicked a tab and we serialize without a flush. The live group value is authoritative.

## Field name quick-reference

| Purpose | Path |
|---|---|
| Main-area split tree root | `mainWindows[i].multiSplitterLayout.layout` |
| Frames dict (keyed lookup for `guestId`) | `mainWindows[i].multiSplitterLayout.frames` |
| Leaf node's unique name (string) | `frames[guestId].dockWidgets[k]` (k=0 for single-leaf groups) |
| Group current-tab index (serialized) | `frames[guestId].currentTabIndex` |
| Split orientation | `<container>.orientation` (1=horizontal, 2=vertical) |
| Split children | `<container>.children` |
| Floating window root | `floatingWindows[i].multiSplitterLayout.layout` |
| Floating window geometry | `floatingWindows[i].geometry` |
