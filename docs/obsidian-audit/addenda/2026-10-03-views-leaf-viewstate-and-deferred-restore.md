# `views` — leaf↔view binding, deferred restore, and the three-way-merge that isn't

**Corrects:** `domains/views.md` §1 (`ItemView`, `View.onTabMenu`), §2 (`EphemeralState.rename`), §4/§7 (`isExtensionRegistered` consumers), §7 (built-in base classes, `registerViewWithExtensions`), §12 (three-way merge), §13 Q2/Q4/Q7.
**Extends:** `domains/views.md` §1 (`FileView`, `EditableFileView`, `TextFileView`, `View.open`), §2 (`ViewState`), §8 (invariants), §7 (registries).
**Date:** 2026-10-03
**Discovered during:** `views.md` verification pass against the restored Obsidian **1.13.7** corpus.
**Source:** `RE/obsidian/1.13.7/renamed/src/app/own/<stmt>.js` (the split corpus). Every citation below is `bundle/rel:line` into that tree. Raw minified confirmation quoted where it matters.

> **The 1.13.7 split corpus has no `views/` directory.** The audit's six `tree/obsidian/views/*.js` files
> are gone; the classes are now statements inside eight `app/own/*.js` files. Every
> `views/Foo.js:*` and `_internal.js:*` citation in `views.md` is dead. §0 below is the remap.
>
> **Two addenda already cover leaf-adjacent ground and are NOT restated here:**
> `2026-10-03-workspace-missing-event-and-small-items.md` (§4 `setViewState` completion tail,
> §6 `pushUndoHistory`, and its CONFIRMED table for `pushState`/`canNavigate`/`canPin`/`setPinned`/
> `setGroup`/`recordHistory`/`detach`) and `2026-06-10-workspace-serialization-corrections.md`
> (`group` placement, `pinned` at two levels, window-bounds flattening).

---

## 0. Symbol remap for 1.13.7

| `views.md` symbol | 1.13.7 location | split-corpus name |
|---|---|---|
| `View` | `app/own/0754-wy.js:2` | `wy` |
| `ItemView` | `app/own/0754-wy.js:104` | `ky` |
| `FileView` | `app/own/1204-gI.js:874` | `xI` |
| `EditableFileView` | `app/own/1809-yZ.js:2` | `yZ` |
| `TextFileView` | `app/own/1813-CZ.js:2` | `CZ` |
| `MarkdownView` | `app/own/1961-Q3.js:868` (`n5 = "markdown"` at `:867`) | `i5` |
| `BasesView` | `app/own/1813-CZ.js:101` (`EZ = "bases"` at `:100`) | `MZ` |
| ImageView | `app/own/1835-t1.js:231` (`a1 = "image"` at `:230`) | `s1` |
| AudioView | `app/own/1835-t1.js:200` (`r1 = "audio"` at `:199`) | `o1` |
| VideoView | `app/own/1840-x1.js:754` (`P1 = "video"` at `:753`) | `I1` |
| PdfView | `app/own/1840-x1.js:645` (`VIEW_TYPE = "pdf"` at `:703`) | `D1` |
| ReleaseNotesView | `app/own/1923-D6.js:2`, aliased `app/own/1924-A6.js:2` (`M6 = "release-notes"` at `app/own/1921-l6.js:493`) | `D6`/`A6` |
| `eD` deferred stub | `app/own/1129-UD.js:716` | `QD` |
| `tD` empty pane | `app/own/1129-UD.js:756` | `$D` |
| `nD` unknown-type fallback | `app/own/1129-UD.js:822` | `ZD` |
| `ViewRegistry` | `app/own/1925-P6.js:2` | `P6` |
| *(undocumented)* `EmbedRegistry` | `app/own/1844-R1.js:107` | `q1` |
| `WorkspaceLeaf` | `app/own/1209-jI.js:502` | `_I` |
| Leaf history | `app/own/1209-jI.js:445` | `UI` |
| `WorkspaceItem` (leaf/split base) | `app/own/1206-OI.js:6` | `FI` |
| `FX` (claimed 3-way merge) | `app/own/1810-bZ.js:2` | `bZ` |
| diff-match-patch | `app/own/1217-mL.js:2` | `mL` |
| `debounce` | `app/own/0355-Ml.js:2` | `Ml` |
| selection/rename helper | `app/own/0655-sm.js:2` | `sm` |
| extension literals | `app/own/0794-hb.js:2-11` | `hb`/`pb`/`db`/`fb`/`vb`/`mb`/`gb`/`yb` |

---

## 1. CORRECTION (headline) — there is no three-way merge; overlapping user edits are silently discarded

`views.md` §1 (External-modify) and §12 both assert that `FX(previousLastSaved, currentViewData, freshDiskData)`
is a "three-way merge" and that the user "**does not lose their in-progress edits**".

**What it actually is** (`app/own/1810-bZ.js:2-8`, verbatim):

```js
function bZ(e, t, n) {
  var i = new mL(),
    r = i.diff_main(e, t, !0, 0);
  r.length > 2 && (i.diff_cleanupSemantic(r), i.diff_cleanupEfficiency(r));
  var o = i.patch_make(e, r);
  return i.patch_apply(o, n)[0];
}
```

A **two-sided diff followed by fuzzy patch application**: diff `base→user-view`, build patches
*against the base*, then re-apply them to the on-disk text and keep only element `[0]` of
`patch_apply`'s return. `patch_apply`'s second element (the per-patch success booleans) is
discarded, so a patch whose context was destroyed on disk is silently dropped. There is no
third input and no conflict representation.

**Executed evidence** (running the extracted `bZ` + `mL` verbatim):

```
base : "para one\npara two\npara three\n"
view : "para one\npara two USER EDIT\npara three\n"     (unsaved in-editor change)
disk : "para one\npara two DISK EDIT\npara three\n"     (external writer changed the same line)
bZ   => "para one USER EDIT\npara two DISK EDIT\npara three\n"
```

The user's edit is **discarded** *and* its text is **misplaced onto the preceding line**
(`para one USER EDIT`). Non-overlapping divergences are fine:

```
base : "alpha\nbravo\ncharlie\ndelta\necho\n"
view : "alpha\nBRAVO USER\ncharlie\ndelta\necho\n"
disk : "alpha\nbravo\ncharlie\nDELTA DISK\necho\n"
bZ   => "alpha\nBRAVO USER\ncharlie\nDELTA DISK\necho\n"
```

**Corbomite impact:** a faithful port must reproduce *this*, not a 3-way merge — or make a
deliberate, documented divergence. `views.md` §12's "this is a correctness gap in Corbomite" is
still directionally right, but the target behaviour is "Obsidian drops overlapping edits and
sometimes relocates them", which is worse to spec than "clobbers everything". Add a regression
test for the same-line-conflict case.

**Secondary finding — the diff deadline is pre-expired.** `bZ` passes `0` as `diff_main`'s 4th
argument. `diff_main` only applies its `diffTimeout` default when that argument is `undefined`
(`app/own/1217-mL.js:11`); the instance default is `diffTimeout = 1` (`app/own/1217-mL.js:4`).
With deadline `0`, `diff_bisect_`'s loop guard `!(Date.now() > n)` is true before the first
iteration (`app/own/1217-mL.js:390`) so the body never runs and it returns a whole-region
`DELETE`+`INSERT` (`app/own/1217-mL.js:412-415`); `diff_lineMode_` propagates the same `0`
recursively (`app/own/1217-mL.js:366`, `:379`, `:421-422`). Demonstrated:

```
diff_bisect_("…fox jumps over the lazy dog while singing loudly today",
             "…cat jumps over a lazy dog while shouting softly today", 0)
  => [[-1, <whole string>], [1, <whole string>]]
diff_bisect_(same, Number.MAX_VALUE)
  => 18 fine-grained hunks
```

**Honest limit:** in the realistic mid-document edits I executed (`bZ` vs. the same function
with `Number.MAX_VALUE`), outputs were **byte-identical** — line-mode diffing resolves before
bisect is reached. So this is a latent difference, not an observed data-loss path. Flag it so
nobody "fixes" the deadline and silently diverges.

---

## 2. CORRECTION — image / audio / video / pdf views extend `EditableFileView`, not `FileView`

`views.md` §7's built-in table gives `Base = FileView` for all four (and §11 calls them
"`QMediaPlayer`/`QLabel` trivialities"). They are one level deeper:

| Class | Closes with | Base |
|---|---|---|
| AudioView `o1` | `app/own/1835-t1.js:229` `}(yZ),` | `EditableFileView` |
| ImageView `s1` | `app/own/1835-t1.js:260` `}(yZ),` | `EditableFileView` |
| VideoView `I1` | `app/own/1840-x1.js:782` `}(yZ);` | `EditableFileView` |
| PdfView `D1` | `app/own/1840-x1.js:704` `}(yZ),` | `EditableFileView` |
| MarkdownView `i5` | `app/own/1961-Q3.js:1241` `}(CZ),` | `TextFileView` ✓ (doc right) |

**Consequence for Corbomite:** all four get breadcrumbs, inline title-rename, the
`vault.on('rename'|'delete')` reactions, and the Rename/Delete pane-menu items — i.e. exactly
the `FileView` + `EditableFileView` surface `views.md` §1 describes, one level further up. A
Qt port that makes `PdfView` a bare `QWidget` will diverge on rename-while-open and on the
right-click menu.

Each also narrows `canAcceptExtension` to its own array, which is what makes
`WorkspaceLeaf.openFile` reuse a live view instance (`app/own/1209-jI.js:570`):

```js
// app/own/1835-t1.js:207-208   (audio)      return pb.contains(e);
// app/own/1835-t1.js:238-239   (image)      return hb.contains(e);
// app/own/1840-x1.js:760-761   (video)      return db.contains(e);
```

---

## 3. CORRECTION — `EphemeralState.rename` has a third producer value, `"all"`, and it means "select the whole title"

`views.md` §2 types the key as `'start' | 'end'`, and §1 says the handler "focuses/blurs
`titleEl`". Neither is what the code does.

```js
// app/own/1809-yZ.js:7-13  (EditableFileView.setEphemeralState)
t.prototype.setEphemeralState = function(t) {
  e.prototype.setEphemeralState.call(this, t);
  var n = t.rename;
  if (n) {
    var i = void 0;
    "start" === n ? i = !0 : "end" === n && (i = !1),
    this.titleEl.isShown() ? (rd.isMobileApp && (this.titleEl.contentEditable = "true"), sm(this.titleEl, i))
      : this.app.fileManager.promptForFileRename(this.file);
  }
}
```

`"all"` — emitted by the breadcrumb "New note" action (`app/own/1204-gI.js:921-923`, the only
producer) — matches neither branch, so `i` stays `undefined`. And `sm` only collapses the
selection when the flag is a boolean:

```js
// app/own/0655-sm.js:2-8
function sm(e, t) {
  var n = e.win, i = n.document.createRange();
  i.selectNodeContents(e), isBoolean(t) && i.collapse(t);
  var r = n.getSelection();
  r && (r.removeAllRanges(), r.addRange(i)), e.focus({ preventScroll: !0 }), …
```

So the three values are **selection states**, not focus states: `"start"` → caret collapsed to
the start, `"end"` → caret collapsed to the end, `"all"` (or any other truthy string) → range
covering the entire title, ready to be typed over. The `!titleEl.isShown()` fallback to the
modal is confirmed (`app/own/1809-yZ.js:12`).

## 4. NEW — `MarkdownView` intercepts `rename` before `EditableFileView` sees it

`views.md` §1 attributes all `rename` handling to `EditableFileView`. In 1.13.7 the markdown
view claims it first, and **deletes** the key so the header title is not also renamed:

```js
// app/own/1961-Q3.js:987-992
var n = t.rename;
if (n) {
  var i = void 0;
  if ("start" === n ? i = !0 : "end" === n && (i = !1), this.inlineTitleEl.isShown())
    (this.currentMode === this.editMode ? this.editMode.cm.scrollDOM.scrollTop : this.previewMode.getScroll()) < .5
      && (sm(this.inlineTitleEl, i), delete t.rename);
}
e.prototype.setEphemeralState.call(this, t);
```

Routing rule: inline title visible **and** content scrolled to the very top (< 0.5 px) →
rename the inline title; otherwise leave `t.rename` intact and let `EditableFileView` handle
the view-header title (or the modal). Both are `contentEditable` targets, and they are
different DOM elements.

## 5. NEW — `eState` keys `views.md` §2 omits

| Key | Where | Note |
|---|---|---|
| `cursor: {from:{line,ch}, to:{line,ch}}` | written `app/own/1961-Q3.js:719-728`, read `app/own/1961-Q3.js:733-736` | source-mode selection; setting it also forces `focus` and `scrollTo(current)` on desktop |
| `startLoc` / `endLoc` | `app/own/1961-Q3.js:996` | derived from `eState.subpath` via `PD(metadataCache.getFileCache(file), subpath)`; `endLoc` is `o.end \|\| null` |
| `focusOnMobile` | `app/own/1961-Q3.js:735` | `var r = e.focus && (!Platform.isMobile \|\| e.focusOnMobile)` — on mobile, `focus` alone is ignored without it |
| `match` | `app/own/1961-Q3.js:794-806` | shape `{content, matches}` confirmed: flat `[start, end]` offset pairs fed to `EO(content, flat)` |

`setEphemeralState({focus: …})` also **blurs and deselects whatever is focused outside the
leaf**, which `views.md` does not mention:

```js
// app/own/1209-jI.js:731-736
WorkspaceLeaf.prototype.setEphemeralState = function(e) {
  if (e && e.focus) {
    var t = this.containerEl.doc.activeElement;
    t && t.instanceOf(HTMLElement) && t !== document.body && this.containerEl !== t
      && !this.containerEl.contains(t) && this.app.workspace.layoutReady
      && (t.blur(), getSelection().removeAllRanges());
  }
  this.view.setEphemeralState(e);
}
```

## 6. NEW — `setViewState` **silently drops** concurrent calls

`views.md` §8's invariants do not mention the `working` latch:

```js
// app/own/1209-jI.js:655-656
if (this.working) return [2];
this.working = !0, h.label = 1;
// …
// app/own/1209-jI.js:690 (finally)
this.working = !1
```

A `setViewState` arriving while one is in flight returns immediately having done nothing — no
queueing, no error. The sibling `LeafHistory.go` turns the same latch into a user-visible
`msgTabBusy` notice (`app/own/1209-jI.js:482`), so the *view* path is the quieter of the two.

---

## 7. NEW — the deferred-stub decision, in full

`views.md` §1 (`eD`) says a stub is built "for any leaf not immediately visible". The actual
predicate is five-way (`app/own/1209-jI.js:663-669`):

```js
if (l = this.app.viewRegistry.getViewCreatorByType(e.type)) {
  if (a || r || !e.icon || void 0 === e.title || this.containerEl.isShown())
    n = l(this);
  else n = new QD(this, e.type, e.icon, e.title);
} else n = "string" != typeof e.type || "empty" === e.type ? this._empty : new ZD(this, e.type);
```

A `QD` stub is produced **only** when all of: the type is registered; the leaf had no prior
history (`a`); the current view is not already a `QD` (`r`); **both** `ViewState.icon` and
`ViewState.title` are present in the incoming payload; and the leaf's container is hidden. Any
one failing forces a real instantiation. This is why `icon`/`title` are cached into
`workspace.json` — they are not cosmetic, they are load-bearing for restore cost.

`QD`'s own state (`app/own/1129-UD.js:729-738`): `getState()` returns the stashed `state`,
`setState(state)` stashes it, `setEphemeralState(e)` stashes it as `this.estate`, and
`rerender()` replays `{type: this.viewType, state: this.state}` with that `estate` — i.e. the
stub round-trips both halves of the payload untouched.

## 8. NEW — why the `view-registered` handler must route through `$D`

`views.md` §4 says `Workspace` "rebuilds `nD` fallback leaves". The mechanism is subtler and
is the answer to §13 Q4 as well. `setViewState` short-circuits view creation when the incoming
type equals the live view's type:

```js
// app/own/1209-jI.js:662
if (!o && !r) return [3, 3];   // o = e.type !== current.getViewType(), r = current instanceof QD
```

So replaying a `ZD` unknown pane's own state after its plugin registers would be a no-op — the
`ZD` would survive. `Workspace`'s handler therefore swaps in a throwaway empty view first:

```js
// app/own/1932-r4.js:153-173  (one shared handler for BOTH events)
var a = function(e) {
  for (var t = function(e) {
      if (e.view instanceof QD) return "continue";
      var t = e.getViewState();
      y(i, void 0, void 0, function() { … return [4, e.open(new $D(e))]; … n.sent(), [4, e.setViewState(t)]; … });
    }, n = 0, r = i.getLeavesOfType(e); n < r.length; n++) t(r[n]);
};
i.app.viewRegistry.on("view-registered", a), i.app.viewRegistry.on("view-unregistered", a),
```

One handler serves both events, it is generic over view type (not `ZD`-specific), it skips
deferred leaves, and it constructs a **fresh `$D`** rather than reusing `leaf._empty`.

**§13 Q4 answered:** `_empty` is a per-leaf singleton, built once in the constructor
(`app/own/1209-jI.js:557`, `i.view = i._empty = new $D(i)`) and reused by `open(null)`
(`:625`), the `setViewState` `"empty"`-type fallback (`:669`) and `detach` (`:779`). Deferred
and unknown views get brand-new instances per swap, and the registry handler above allocates a
third kind — so `$D` is *not* a global singleton, and a Qt port must not make it one.

## 9. CORRECTION — `View.onTabMenu`'s close group is conditional and `ItemView` adds to it

`views.md` §1 and §9 describe the default tab menu as "Close / Close others / Close after /
Close all … Pinned tabs are excluded from Close-others/Close-all". Three omissions, all
citable from `app/own/0754-wy.js`:

- The **whole** close group is suppressed in the sidebar: `if (!this.app.workspace.isInSidebar(n)) {` (`:67`).
- "Close all" is additionally suppressed when the only unpinned leaf is this leaf: `(1 !== l.length || l[0] !== n)` (`:91`).
- "Close others" filters `!e.pinned && e !== n` (`:68-70`); "Close after" filters `!pinned` over `children.slice(indexOf(this) + 1)` (`:78-80`) — i.e. it is positional within the tab group.

`ItemView` then adds four more items in sections `pane` / `open`
(`app/own/0754-wy.js:209-229`), which `views.md` does not mention at all:

```js
t.prototype.onTabMenu = function(t) {
  e.prototype.onTabMenu.call(this, t);
  var n = this.app, i = this.leaf;
  i.canPin() && t.addItem(… pin/unpin, section "pane", icon ALWAYS "lucide-pin" …),
  i.group ? t.addItem(… "Unlink tab", "pane" …)
         : i.canPin() && t.addItem(… "Link tab", "pane", → workspace.onStartLink(leaf)),
  Platform.canPopoutWindow && t.addItem(… "Move to new window", "open", → workspace.moveLeafToPopout(leaf));
}
```

Note the pin item's icon is `lucide-pin` for **both** pin and unpin (`:214`); the pane-menu
equivalent does branch (`app/own/0754-wy.js:197`).

`ItemView` also overrides `onPaneMenu` (super-call **first**, `:187-208`), adding Split
Right / Split Down under `Platform.canSplit && leaf.canPin()`, Split Right disabled when
`workspace.isInSidebar(leaf)`.

## 10. CORRECTION — `ItemView.handleDrop`'s gate has a global modifier branch

`views.md` §1: "delegates to `leaf.handleDrop`; accepts drops onto the header unless
`canDropAnywhere = true`." The real gate is three-way:

```js
// app/own/0754-wy.js:157
if (this.canDropAnywhere || (Platform.isMacOS ? e.shiftKey : e.altKey) || this.headerEl.contains(e.target)) {
```

Hold **Alt** (Shift on macOS) anywhere in the leaf and *any* drop target redirects to
`leaf.handleDrop` — the conventional modifier-drag-to-open escape hatch. A Corbomite port that
only handles the header will not offer it.

## 11. CORRECTION — `registerViewWithExtensions` is not atomic as a pair

`views.md` §7 calls it an "atomic convenience". It is a plain sequence
(`app/own/1925-P6.js:42-44`):

```js
t.prototype.registerViewWithExtensions = function(e, t, n) {
  this.registerView(t, n), this.registerExtensions(e, t);
}
```

`registerExtensions` throws on a duplicate extension *after* pre-scanning
(`app/own/1925-P6.js:26-35`), so a failure leaves the view type registered with no extensions —
an orphaned entry that `getViewCreatorByType` will hand out while `getTypeByExtension` never
routes to it. §8's "atomic across the array" invariant is right for the array; it does not
extend across the two calls.

## 12. CORRECTION — `ViewRegistry.isExtensionRegistered` has nothing to do with indexability

`views.md` §4 and §7 both cite `metadata/MetadataCache.js:994` as the consumer that
"re-evaluates indexability". In 1.13.7 there are three real consumers and none of them is
`MetadataCache`:

| Consumer | Location |
|---|---|
| File-explorer file item `isSupported()` → drives the `is-unsupported` class | `app/own/1204-gI.js:76-77` |
| `isSupportedFile(file)` on the vault file map | `app/own/1306-eR.js:593-595` |
| File suggester — picks `viewRegistry` vs `embedRegistry` by context | `app/own/1951-e3.js:65-67` |

```js
// app/own/1306-eR.js:593-595
t.prototype.isSupportedFile = function(e) {
  var t = this.app;
  return !!t.vault.getConfig("showUnsupportedFiles") || t.viewRegistry.isExtensionRegistered(e.extension);
}
```

Both feed **file-tree listing visibility**, not the metadata indexer.

## 13. NEW — a second, parallel registry: `app.embedRegistry`

`views.md` §7 documents one registry. There are two, constructed back-to-back on `App`:

```js
// app/own/2075-Vie.js:7
this.embedRegistry = new q1(), this.viewRegistry = new P6(),
```

`q1` (`app/own/1844-R1.js:107-143`) is a plain `Events` object with the same
`registerExtension`/`registerExtensions`/`unregisterExtension(s)`/`isExtensionRegistered`
surface and the **same duplicate-extension throw**, seeded from the **same literal arrays** but
without `canvas`/`base`:

```js
// app/own/1844-R1.js:110-120
t.registerExtensions(vb, …),   // ["md"]
t.registerExtensions(hb, …),   // image
t.registerExtensions(pb, …),   // audio
t.registerExtensions(db, …),   // video
t.registerExtensions(fb, …);   // ["pdf"]
```

The file suggester switches between them by UI context
(`u = "view" === t.context ? s : i`, `app/own/1951-e3.js:65`). Corbomite needs both: a
`Corbomite::ViewRegistry` alone will mis-handle the "open as embed" surface.

The literal module `app/own/0794-hb.js:2-11` also defines three **consumer-only** groupings
that are *not* registry seeds — `mb = ["canvas"]`, `gb = ["base"]`, `yb = ["json","css","js"]`,
`bb = ["md","canvas","base"]`, `wb = image+audio+video+pdf+md+canvas`. `yb` is Sync's
version-history renderer (`app/own/2040-Vee.js:3856`), `wb` is Publish's `isFileSupported`
(`app/own/2040-Vee.js:2708-2710`), `kb = e => !bb.includes(e)` (`app/own/0795-kb.js:2-4`).

## 14. NEW — `Plugin.registerView`'s teardown only detaches leaves when user-disabled

`views.md` §7 says the plugin wrappers "delegate with auto-unregister on plugin unload".
Precise version:

```js
// app/own/1938-c4.js:73-77
Plugin.prototype.registerView = function(e, t) {
  var n = this;
  this.app.viewRegistry.registerView(e, t), this.register(function() {
    n.app.viewRegistry.unregisterView(e), n._userDisabled && n.app.workspace.detachLeavesOfType(e);
  });
}
```

An ordinary unload (reload, disable-then-quit ordering) leaves the leaves in place, which then
become `ZD` unknown panes via the `view-unregistered` handler in §8. Only a user-initiated
disable detaches them. `Plugin.registerExtensions` (`:83-87`) has no such branch.

## 15. NEW — `TextFileView.save` details the pipeline section omits

```js
// app/own/1813-CZ.js:45-48
if (this.dirty = !1, n = (t = this).file, i = t.app, r = i.vault, !n || n.deleted) return [2];
if (this.saving) return e || (this.saveAgain = !0), [2];
if (this.saveAgain = !1, o = this.getViewData(), this.lastSavedData === o || null === this.lastSavedData) return [3, 6];
a = this.lastSavedData, e ? (this.data = null, this.lastSavedData = null, this.clear()) : (this.data = o, this.lastSavedData = o), this.saving = !0, …
```

- **`dirty` is cleared unconditionally at entry**, before every early-exit — including the
  `!file || file.deleted` bail and the no-op skip. `saveImmediately()` is `if (this.dirty)
  return this.save(!1)` (`:37-38`), so it becomes a no-op the moment any `save()` runs.
- **`saveAgain` is cleared *before* the skip check**, so a skip-write also eats a pending
  `saveAgain`.
- **The failure path re-throws** after backing up: `…, this.app.fileManager.storeTextFileBackup(n.path, o), s;`
  (`:56-59`). §1's numbered list stops at the backup call.

`loadFileInternal` has an ordering subtlety that matters for a faithful port
(`app/own/1813-CZ.js:83-92`):

```js
if (i = this.lastSavedData, this.lastSavedData = n, !t && i) {
  if (i === n) return [2];
  if (this.dirty) {
    if ((r = this.getViewData()) === n) return [2];
    r !== i && this.isPlaintext && (n = bZ(i, r, n), new Aw(bd.interface.msgFileChanged({ file: e.path })));
  }
}
return this.setData(n, t), [2];
```

`lastSavedData` is assigned the **raw disk text before** the merge decision, so after a merge
the baseline is the un-merged disk text, not what the user now sees. A second external modify
therefore re-merges against the same original baseline.

**§13 Q6 answered — yes, it is a real data-loss risk.** With `isPlaintext = false` (BasesView,
`app/own/1813-CZ.js:104`) the merge branch is skipped entirely and control falls through to
`this.setData(n, t)` with the fresh disk text, discarding the dirty buffer. The
`msgFileChanged` notice is inside the merge branch, so **no notice is shown at all** on that
path.

## 16. NEW — `EditableFileView.saveTitle`'s return value gates the blur

`views.md` §1 says the handler "delegates to `app.fileManager.renameFile`". The boolean return
is load-bearing and there are three distinct `true` outcomes:

```js
// app/own/1809-yZ.js:99-101
if (n !== i || n.deleted) return [2, !0];            // file changed identity mid-edit → treated as success, no rename
if (r = e.getText().trim(), o = HP(this.app, n, r, !0)) return gZ(e, o), [2, !1];
if (a = n.getNewPathAfterRename(r), n.path === a) return [2, !0];   // no-op rename → success
```

`onTitleKeydown` Enter/Tab only blurs `if (await this.saveTitle(i))`
(`app/own/1809-yZ.js:69-78`) — an invalid name keeps focus in the title with the inline error
from `gZ`. A rename error from `fileManager.renameFile` also returns `false` and renders
`s.message || s.code || s.toString()` (`:104-108`).

Also: `onTitleChange` has **two** distinct rejection paths, not one —
validator error via `HP(app, file, text, false)`, else an unsafe-character regex
`zD.test(n) ? gZ(e, msgUnsafeCharacters() + VD) : Qg()` (`app/own/1809-yZ.js:53-58`).

And the Enter/Tab eState is **not** `{focus: true}`:

```js
// app/own/1809-yZ.js:62-67
var n = function() { i.blur(), t.setEphemeralState({ focus: !0, focusOnMobile: i !== t.titleEl }); }
```

`focusOnMobile` is set only when the keydown target is *not* `titleEl` itself.

## 17. NEW — `FileView.getState` omits `file` when there is no file

```js
// app/own/1204-gI.js:965-968
FileView.prototype.getState = function() {
  var t = e.prototype.getState.call(this), n = this.file;
  return n && (t.file = n.path), t;
}
```

A file-less `FileView` emits `{…}` with **no `file` key at all**, not `{file: null}`. That is a
`workspace.json` byte-shape difference. `FileView.setState` (`:979-1002`) branches on
`hasOwnProperty("file")`, and `result.done = () => this.syncState()` is only attached when
`!state.sync && (result.layout || result.history)`.

## 18. NEW — lifecycle asymmetries worth mirroring

- **`View.open` does not await `load()`.** `app/own/0754-wy.js:13-15`:
  `(t = this.load()) && t.catch(function(e) { return console.error("View failed to load", e); }), [4, this.onOpen()]`.
  The load promise is fire-and-forget with a `console.error` catch, so **async work started in
  `onload` can interleave with `onOpen`**. `views.md` §8's invariant "`onload` fires before
  `onOpen`" is true only for the synchronous part.
- **`WorkspaceLeaf.open` skips awaiting `close()` for stub views.**
  `app/own/1209-jI.js:615`: `i = t.close(), t instanceof QD || t instanceof $D ? [3, 3] : [4, i]`.
  `close()` is *called* but its promise is dropped when the outgoing view is a deferred stub
  or an empty pane.
- **`ItemView` seeds the title in `load()`** (`app/own/0754-wy.js:155`,
  `this.titleEl.setText(this.getDisplayText())`) and calls `updateNavButtons()` from the
  **constructor** (`:122`), not only on `history-change`.
- **`EditableFileView.onOpen` sets `titleEl.tabIndex = -1`** (`app/own/1809-yZ.js:19`), which
  `views.md` does not mention, and on mobile apps installs a `touchstart` listener that sets
  `contentEditable` (`:19-21`) — on *every* touch, not just the first.
- **`updateHeader` marks unknown panes in the DOM:**
  `this.tabHeaderEl.toggleClass("mod-unknown", this.view && this.view instanceof ZD)`
  (`app/own/1209-jI.js:812`).

## 19. NEW — answers to `views.md` §13 open questions

| Q | Answer | Location |
|---|---|---|
| **Q2** `handleCut`/`handleCopy`/`handlePaste` — who overrides? | `MarkdownView` overrides all three, delegating to the properties/metadata editor when it has focus; paste is additionally gated on `e.clipboardData.getData("obsidian/properties")` | `app/own/1961-Q3.js:1234-1240` (defaults `app/own/0754-wy.js:102`) |
| **Q4** `_empty` lifetime | per-leaf singleton from the constructor; reused by `open(null)`, the `"empty"` fallback and `detach`; `Workspace`'s registry handler allocates a fresh `$D` | `app/own/1209-jI.js:557`, `:625`, `:669`, `:779`; `app/own/1932-r4.js:161` |
| **Q6** `isPlaintext = false` external-modify data loss | **Confirmed, and worse than the doc supposes** — disk text overwrites the dirty buffer with *no* notice | `app/own/1813-CZ.js:83-92` |
| **Q7** `View.onHeaderMenu` — where is it fired? | **Nowhere.** Declared once at `app/own/0754-wy.js:56` and never called anywhere in the app corpus — dead API | `app/own/0754-wy.js:56` |

---

## 20. Confirmed (table)

Batch re-confirmation of `views.md` claims that were right and needed no change. All verified
in 1.13.7:

| Claim | Verdict | Location |
|---|---|---|
| `View` ctor: `icon = 'lucide-file'`, `navigation = false`, `containerEl = leaf.containerEl.createDiv('workspace-leaf-content')` with `data-type` | confirmed | `app/own/0754-wy.js:5` |
| `View.getState/setState/getEphemeralState/setEphemeralState` default to `{}`/no-op | confirmed | `app/own/0754-wy.js:44-54` |
| `View.getIcon()` = `this.icon \|\| 'lucide-file'` | confirmed (`this.icon ? this.icon : "lucide-file"`) | `app/own/0754-wy.js:54-55` |
| `getSideTooltipPlacement()` maps sidedock side → opposite placement | confirmed | `app/own/0754-wy.js:99-101` |
| `ItemView` mobile sidebar-toggle vibrates 100 ms and toggles `leftSplit` | confirmed | `app/own/0754-wy.js:112-115` |
| `ItemView.addAction` prepends `clickable-icon.view-action`, `mousedown` preventDefault, fires on buttons 0 **and** 1 | confirmed | `app/own/0754-wy.js:161-167` |
| `onMoreOptions` section order `['close','pane','open','action','find','info','info.copy','view','view.linked','system','','danger']` + `info.copy`/`view.linked` submenus + `onPaneMenu(menu,'more-options')` + `leaf-menu` trigger | confirmed | `app/own/0754-wy.js:168-186` |
| `ItemView.load()` subscribes `group-change` and `history-change` | confirmed | `app/own/0754-wy.js:152-155` |
| `FileView` ctor: `allowNoFile = false`, `navigation = true` | confirmed | `app/own/1204-gI.js:875-878` |
| `FileView.loadFile` returns `false` on same-file, `true` after a swap; rolls `this.file` back to `null` and shows `msgFailedToLoadFile` on `onLoadFile` throw; then breadcrumbs/title/`requestActiveLeafEvents`/`updateHeader` | confirmed | `app/own/1204-gI.js:1014-1038` |
| `FileView.setState` sets `result.close` when `!file && !allowNoFile`, and `layout`+`history` on an actual swap | confirmed | `app/own/1204-gI.js:987-999` |
| `FileView.onDelete` steps back through history, else `leaf.open(null)`, then detaches when the result is the empty view and the tab group has siblings | confirmed (`t.parent.children.length > 1`) | `app/own/1204-gI.js:1057-1083` |
| `FileView.onRename` re-renders breadcrumbs + title + `onLayoutChange` + `updateHeader`, does not re-read the file | confirmed | `app/own/1204-gI.js:1051-1056` |
| `FileView.onClose` = `contentEl.empty()` then `loadFile(null)` | confirmed | `app/own/1204-gI.js:1003-1013` |
| Breadcrumbs: click → file-explorer `revealInFolder`, contextmenu → `file-menu` with source `"file-explorer-context-menu"`, drag → `dragFolder` | confirmed | `app/own/1204-gI.js:893-953` |
| `FileView` subscribes `vault.on('rename')` and `vault.on('delete')` in `onload` | confirmed | `app/own/1204-gI.js:961-964` |
| `TextFileView` 2000 ms **trailing-edge** debounce; `Ml` is lodash `debounce` and no `leading` arg is passed | confirmed (`Ml(n.save.bind(n), 2e3)`, `n = !1` default) | `app/own/1813-CZ.js:6`; `app/own/0355-Ml.js:2-3` |
| `save(immediate)`: `saving` re-entry sets `saveAgain`; skip on `lastSavedData === current` or `lastSavedData === null`; immediate mode nulls `data`/`lastSavedData` and calls `clear()`; `finally` re-runs `save()` once | confirmed | `app/own/1813-CZ.js:39-66` |
| Save failure: restore `lastSavedData`, `msgFailToSaveFile({filepath, message})`, `storeTextFileBackup(path, data)` | confirmed | `app/own/1813-CZ.js:56-59` |
| `onModify` gate is `!this.saving && file === this.file`; prefers `cachedRead` while the file is mid-save | confirmed | `app/own/1813-CZ.js:67-79` |
| `onUnloadFile` = `save(true)`; `saveImmediately()` = "save if dirty" | confirmed | `app/own/1813-CZ.js:15-25`, `:37-38` |
| `setData(data, clear)` short-circuits on `this.data === data` unless `clear` | confirmed | `app/own/1813-CZ.js:96-98` |
| `EditableFileView.onPaneMenu` adds Rename (`action`, `lucide-edit-3`) and Delete (`danger`, `lucide-trash-2`, warning) and fires `file-menu(menu, file, source, leaf)` | confirmed | `app/own/1809-yZ.js:114-126` |
| `ViewRegistry` ctor seeds exactly **six** entries (5 with extensions + `release-notes` type-only) | confirmed | `app/own/1925-P6.js:5-17`; `M6 = "release-notes"` at `app/own/1921-l6.js:493` |
| `release-notes` view extends `ItemView` | confirmed (`}(ky)`) | `app/own/1923-D6.js` tail; alias `app/own/1924-A6.js:2` |
| `registerView` throws on duplicate; `unregisterView` is a silent no-op if absent; both fire their events | confirmed | `app/own/1925-P6.js:19-25` |
| `registerExtensions` pre-scans then mutates, one `extensions-updated`; `unregisterExtensions` also one | confirmed | `app/own/1925-P6.js:26-41` |
| `viewRegistry` is constructed on `App`, after `embedRegistry` | confirmed | `app/own/2075-Vie.js:7` |
| `ViewState` written by `getViewState` is exactly `{type, state, pinned?, icon?, title?}` with `icon`/`title` `|| undefined`-dropped and `title` `.trim()`ed | confirmed | `app/own/1209-jI.js:641-648` |
| `ViewState.active` is written only by `openFile`, defaulted to `leaf === workspace.activeLeaf`, and is honoured on read | confirmed | `app/own/1209-jI.js:570-575`, `:686-688` |
| `popstate` is a real, transient `ViewState` key: set by leaf-history replay, suppresses history recording | confirmed | `app/own/1209-jI.js:455`, `:688` |
| `eD`/`tD`/`nD` semantics (deferred stub / empty pane / unknown-type fallback) and `nD`'s `layout-change` rebuild + conditional "Close all of this type" | confirmed | `app/own/1129-UD.js:716-754`, `:756-821`, `:822-888` |
| `nD` unknown pane shows `unknownPaneTitle`/`unknownPaneDesc({type})`, tooltip "This pane doesn't look like anything to me.", icon `lucide-ghost` | confirmed | `app/own/1129-UD.js:826-840` |
| `tD` empty pane actions (`file-explorer:new-file`, `switcher:open`, conditional `webviewer:open`, Close) each with a printed hotkey hint | confirmed | `app/own/1129-UD.js:773-819` |
| Leaf `id` is `Gd(16)`; group ids likewise | confirmed | `app/own/1206-OI.js:9`; `app/own/1209-jI.js:752` |
| Leaf history is **not** written to `workspace.json` (`WorkspaceItem.serialize` emits `{id, type, dimension?}`; the leaf adds `pinned?`, `state`, `group?`) | confirmed | `app/own/1206-OI.js:28-33`; `app/own/1209-jI.js:559-561` |

---

## 21. Not verified

- **Corbomite-side rows of `views.md` §11** (`EditorViewSpace.cpp:119`, `NoteEditorWidget`,
  `EditorViewManager::buildSessionState`, `EditorViewSpace::showTabContextMenu`). This pass
  read only the Obsidian corpus; the §11 "Status" column is unverified.
- **The audit's `_internal.js:461203` citation for `FX`** is dead — there is no
  `_internal.js` in the 1.13.7 tree. `bZ` at `app/own/1810-bZ.js` is the live symbol (§1).
- **`i18n` key strings** (`msgFileChanged`, `msgFailToSaveFile`, `unknownPaneDesc`, …) were
  confirmed only as call sites and parameter shapes; the English text was not read from the
  locale bundles.
- **`wx`/`Plugin` API surface beyond §14** — `Plugin.registerView`/
  `registerExtensions`/`registerHoverLinkSource` were read (`app/own/1938-c4.js:73-87`); the
  rest of the plugin API is deliberately out of scope per doctrine.