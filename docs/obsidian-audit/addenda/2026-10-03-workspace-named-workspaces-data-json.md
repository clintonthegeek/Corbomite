# There is no `workspaces.json` — named workspaces live in `.obsidian/plugins/workspaces/data.json`

**Date:** 2026-10-03
**Discovered during:** `workspace.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/workspace.md` §3 (`.obsidian/workspaces.json`), §13 Q5,
and the Pass-1 summary line "**On-disk contracts:** persisted layout in
`.obsidian/workspace.json` (default) and per-named-workspace in
`.obsidian/workspaces.json`".
**Relevant cluster plans:** `superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md`
(Phase L2 — persistence tiering)

## Finding

`workspace.md` §3 documents a second on-disk file, `.obsidian/workspaces.json`, and §13 Q5
leaves its schema as "Likely shape: `Record<workspaceName, LayoutJson>`… Confirm in
`plugin/` audit". Both are wrong about the location, and the shape is a wrapper object
rather than a bare record.

**The string `workspaces.json` does not occur anywhere in the 1.13.7 bundle.** The Workspaces
core plugin uses the standard plugin data file instead:

```js
// app/own/2048-Fne.js:111  (Nne = WorkspacesPlugin constructor)
// this.id = "workspaces", this.name = Rne.name(), this.description = Rne.desc(), this.workspaces = {}, this.activeWorkspace = "";
```

```js
// app/own/2048-Fne.js:144  (onEnable)
// e = i.sent() || {}, this.workspaces = e.workspaces || {}, this.activeWorkspace = e.active || ""
```

```js
// app/own/2048-Fne.js:257-266  (saveData)
// e.prototype.saveData = function() {
//   return y(this, void 0, void 0, function() {
//     return b(this, function(e) {
//       switch (e.label) {
//         case 0:
//           return [4, this.plugin.saveData({ workspaces: this.workspaces, active: this.activeWorkspace })];
//         case 1:
//           return e.sent(), [2];
//       }
//     });
//   });
// },
```

`plugin.saveData()` / `plugin.loadData()` are the standard internal-plugin data file, so
the real path is **`<configDir>/plugins/workspaces/data.json`** with shape:

```typescript
{
  workspaces: Record<string, LayoutJson & { mtime: string }>;
  active: string;                 // "" when nothing is active
}
```

Each snapshot is `getLayout()` **plus an `mtime`**:

```js
// app/own/2048-Fne.js:228-230  (saveWorkspace)
// e.prototype.saveWorkspace = function(e) {
//   var t = this.app.workspace.getLayout();
//   t.mtime = window.moment().format(), this.workspaces[e] = t, this.saveData();
// },
```

`window.moment().format()` with no argument is a locale-formatted human-readable stamp
(e.g. `Oct 3, 2026 2:15 PM`), not an ISO 8601 string — and it is only ever *written*, never
read back:

```js
// app/own/2048-Fne.js:231-245
// e.prototype.setActiveWorkspace = function(e) { this.activeWorkspace = e, this.saveData(); },
// e.prototype.loadWorkspace = function(e) {
//   … this.workspaces.hasOwnProperty(e) ? (this.activeWorkspace = e, [4, this.app.workspace.changeLayout(this.workspaces[e])]) : [3, 2];
```

`setLayout` ignores the extra `mtime` key, and `changeLayout` does not save — so loading a
named workspace does not rewrite `workspace.json` until the next mutation.

Two further facts worth having:

- Snapshots carry **no `lastOpenFiles`** (`getLayout()` does not include it; only
  `saveLayout()` appends it — `app/own/1932-r4.js:1173`). Loading a named workspace
  therefore leaves the recent-files list untouched.
- Switching is driven entirely through `workspace.changeLayout(snapshot)`
  (`app/own/2048-Fne.js:238`), which is `clearLayout()` + `setLayout()`.

## Why noticed now

`workspace.md` §3 is one of only two paragraphs in this domain that make an on-disk claim,
and it names a file that does not exist. A reader auditing `.obsidian/` against this domain
doc would look for `workspaces.json`, not find it, and conclude the file was deleted or
renamed. Cluster L2's persistence-split work enumerates exactly which keys Corbomite may
write where, so an invented filename in the spec is a live hazard there.

## Action taken

- No code change. Corbomite does not implement named workspaces (`workspace.md` §11 lists
  named-workspace switching as unconfirmed/absent), so there is nothing to correct.
- `workspaces.json` is additionally excluded from Obsidian Sync alongside
  `workspace.json` and `workspace-mobile.json`, which is a separate observation and
  corroborates that these files are machine-local — see the Sync allow-list at
  `app/own/2043-Xte.js:314`:
  `return "workspace.json" !== o && "workspace-mobile.json" !== o && (…)`.
- Linked from `addenda/README.md` and `00-taxonomy.md` §Addenda.

## Confirmed (table)

| Claim in `workspace.md` | Verdict | Location |
|---|---|---|
| Named workspaces are owned by the internal **Workspaces** plugin, not this domain | confirmed | `app/own/2048-Fne.js:109-111` (`this.id = "workspaces"`) |
| Switching calls `workspace.changeLayout(json)` | confirmed | `app/own/2048-Fne.js:238` |
| No other file in the vault is touched by this domain | **refuted** — `plugins/workspaces/data.json` is the plugin's, but `.obsidian/workspace-mobile.json` is written from the same `saveLayout` on mobile | `app/own/1932-r4.js:1175` |
| The two layout filenames are `workspace.json` / `workspace-mobile.json` | confirmed (constants `J6` / `e4`) | `app/own/1928-Y6.js:409-410` |