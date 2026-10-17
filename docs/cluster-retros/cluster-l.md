# Cluster L retro — workspace and KDDockWidgets

**Closed 2026-10-17.** L0–L4 had already landed. L5, the live soak, was the phase still open. Plan: [`../superpowers/plans/archive/2026-08-17-cluster-l-workspace-stabilization.md`](../superpowers/plans/archive/2026-08-17-cluster-l-workspace-stabilization.md). Closeout: [`../decisions-archive.md`](../decisions-archive.md), `2026-10-17 — L5 soak closed`.

## What changed

The compat rule from L0 held: `workspace.json` stays Obsidian's schema, and the in-memory layout is KDDockWidgets. L1–L4 made teardown, the `floating` key, tab commands, and sidebar width real. The 2026-08-18 round trip fixed a nested-split scramble and left five scenarios unrun. The 2026-10-17 eyeball ran those scenarios on `/tmp` copies, then opened one rearranged vault in Obsidian and back.

## What surprised

A root `horizontal` split is saved as the user built it and reloaded as side-by-side, because load forces the root direction to `vertical`. That is pinned Obsidian parity, and it is still surprising in the window.

Deferral only inspects the main window's tab groups. A popout whose notes are not the active leaf comes back as titled tabs with no editor and no back/forward header. A drag inside the float builds the views.

A vault with no `workspace.json` does not open one empty tab. The layout-ready gate swallows the focus of the first placeholder, and a second dock is added with the raw leaf id as its title. Quit can also write an empty `tabs` node the loaded file did not contain. Obsidian dropped that node.

Undo close remembers a sibling, not the split. The last tab in a pane does not bring the pane back.

## What it unblocks

Further `workspace.json` changes are no longer waiting on the soak. The findings stay on the punch list as `[cluster-l]`. The next trajectory action is the two frontmatter scanners, not another soak pass and not Cluster M.
