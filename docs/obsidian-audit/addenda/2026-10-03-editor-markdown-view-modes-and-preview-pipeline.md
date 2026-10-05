# The three-mode state machine is two config keys; the preview worker is single-flight; reading view and the editor disagree about fold encoding

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/editor-markdown.md` against the restored Obsidian 1.13.7 corpus
**Supersedes / extends:** `domains/editor-markdown.md` §1, §2, §3, §5, §6, §7, §8, §9, §10, §12, §13. Extends `addenda/2026-10-03-editor-wrapper-commands-and-keymaps.md` (the `Editor` wrapper half) and `addenda/2026-06-10-editor-timing-and-coverage-corrections.md` §1/§3 (the `Ml` throttle).
**Relevant cluster plans:** `superpowers/plans/archive/2026-08-20-cluster-n-rich-clipboard.md` (checkbox round-trip, clipboard verbs), the Cluster O context-sensitive-UI plan (`markdown-viewport-menu` sections), `superpowers/plans/2026-04-03-editor-three-modes.md`, `superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md` (`leaf.state.mode` / `state.source` round-trip)

## Finding

`editor-markdown.md` is structurally right and almost entirely uncitable: **every** `MarkdownView.js:*`, `MarkdownPreviewView.js:*`, `MarkdownPreviewRenderer.js:*`, `MarkdownRenderer.js:*` and `MarkdownRenderChild.js:*` line reference in it is dead. Those six files no longer exist in the split corpus; the classes live in four different `app/own/*.js` files under entirely different symbols (§0 below is the remap). Everything the doc asserts about behaviour mostly survived — the §8 invariant list is the strongest part of the document — but eleven specific claims are wrong, and four of them are wrong in ways that would produce a non-faithful port.

The three most load-bearing:

1. **"Live-preview is not a separate mode" (§8 invariant 2) is right, but the doc never says where the third state comes from.** It is two independent vault config keys, not a third value of `mode`: `mode: "source"|"preview"` selects the *renderer*, and `state.source` (⊂ `editMode.sourceMode`) is derived from the boolean `livePreview` (`app/own/1823-RJ.js:890`). Both have defaults, and both are user-facing settings the doc's §3 "On-disk contracts" omits entirely.
2. **The async preview parse is single-flight, and contention degrades to a blocking main-thread parse — it does not queue** (`app/own/1500-wW.js:11-13`). `submit()` returns `null` when a request is in flight; `parseAsync` then feeds that `null` into `parseFinish`, throws, and the `catch` runs `parseSync()`. So opening a second large note while a third is worker-parsing silently parses it on the UI thread. §13 Q3 asked whether the worker serialises; it does not, and the answer is worse than serialising.
3. **The whole markdown parser option surface is two keys, and one of them is hard-coded**: `{ breaks: true, commonmark: true }` (`app/own/1080-expressionstatement.js:2-5`). `breaks` is `strictLineBreaks` inverted; **`commonmark` is never written by any config** — Obsidian's markdown is permanently parsed in CommonMark mode with no user escape hatch. §13 Q1 is answered: the object is a module-level singleton that is *replaced wholesale* on the worker on every async parse.

Two more that change output bytes or user-visible state: the **2000 ms save is `Ml`, i.e. the non-resetting throttle** (so a save lands 2 s after the *first* keystroke of a burst, mid-typing — §3 item 1's "2000 ms debounce" is wrong in mechanism), and **reading view and the editor encode frontmatter-collapse fold differently** (`{from: 0, to: 0}` vs `{from: section.start.line, to: start.line + section.lines}`), and `setMode` hands one mode's foldInfo straight to the other mode's `applyFoldInfo` on every switch.

## Why noticed now

`editor-markdown.md` is the only Pass 2 domain doc for the markdown view that had **no** 2026-10-03 verification pass, and it is the doc that owns the three-mode encoding, the section-recycling pipeline, and the checkbox round-trip regex — the three largest Corbomite editor gaps. The 2026-10-03 wrapper addendum covered the *editor* half (`1823-RJ.js`) in depth and explicitly deferred the live-preview decoration engine, but nothing had yet checked the `MarkdownView` base class, `MarkdownPreviewView`, `MarkdownPreviewRenderer`, `MarkdownRenderer`, or `MarkdownPreviewSection` against 1.13.7.

## Action taken

No plan edits (rule 7: linkage is centralised by the caller). Two corrections should be promoted before any §11 mapping row is sized: the **`commonmark: true` hard-coding** (a faithful port must run CommonMark mode, and §14 #2's "worker-parse architecture" note needs the single-flight caveat) and the **`registerMarkdownCodeBlockProcessor` sortOrder** (§10's `-100` does not exist, so plugin code-block processors interleave with plain post-processors rather than running first). §13 Q1/Q3/Q7 are answered below; Q2/Q4/Q5/Q6/Q8 remain open.

---

## 0. Citation remap — the six extracted files are four corpus files

The doc's de-minifier note (§ preamble) describes a single `MarkdownView.js` region containing five classes. In 1.13.7 the six classes are split across **four** files, plus two embed-only subclasses:

| Audit name | Symbol | File | Public export |
|---|---|---|---|
| `MarkdownView` | `i5` | `app/own/1961-Q3.js:868-1241` | `0025-expressionstatement.js:45` |
| — its base | `CZ` = `TextFileView` | `app/own/1813-CZ.js:2` | `0025-expressionstatement.js:85` |
| `MarkdownPreviewView` | `$W` | `app/own/1533-QW.js:68-248` | `:42` |
| `MarkdownRenderer` | `YW` | `app/own/1531-YW.js:2-182` | `:44` |
| `MarkdownRenderChild` | `EW` | `app/own/1503-EW.js:2-8` | `:43` |
| `MarkdownRenderChild` (embed subclass) | `QW` | `app/own/1533-QW.js:2-67` | — |
| `MarkdownPreviewSection` | `MW` | `app/own/1503-EW.js:9-34` | `:41` |
| `MarkdownPreviewRenderer` | `AW` | `app/own/1506-TW.js:8-824` | `:40` |
| `MarkdownEditView` (`j6`, `type:"source"`) | `t5` | `app/own/1961-Q3.js:686-866` | — |
| — embed view base | `t1` | `app/own/1835-t1.js:2-115` | — |
| — embed view | `i1` | `app/own/1835-t1.js:116` | — |
| — editable markdown embed view | `j1` | `app/own/1844-R1.js:164-448` | — |
| — PDF-export modal (`G6`) | `o5` | `app/own/1961-Q3.js:1243-1408` | — |

Other doc symbols: `n5` = the view-type constant `"markdown"` (`1961-Q3.js:867`), `Pc` = `Tv` (`app/own/0629-Tv.js:2`), `cz` = `Qq`, `uq` = `GW`, `rT` = `JT` (`app/own/1093-JT.js:2`), `kI` = `SR` (`app/own/1318-SR.js:2`), `getFrontMatterInfo` = `_T`, `resolveSubpath` = `PD`, `Wz` = `TW` (`1506-TW.js:2`), `cc(16)` = `Gd(16)`, `Ux.globalOptions` = `VT.globalOptions` (`app/own/1079-VT.js:2` → `IE`), `Pc(fn, 0)`/`lz` search widget = `Xq`.

---

## 1. The three modes are two config keys — NEW (and §8 invariant 2's mechanism)

§8 invariant 2 ("Live-preview is NOT a separate mode: it's `{mode: "source", source: false}`") is **confirmed**. What the doc omits is where `source` comes from and what the defaults are:

```js
// app/own/1823-RJ.js:890
r.sourceMode = !r.app.vault.getConfig("livePreview");
```

```js
// app/own/0920-eE.js:1202, :1210
livePreview: !0,
defaultViewMode: "source",
```

| Vault config key | Type | Default | Consumed at |
|---|---|---|---|
| `livePreview` | boolean | `true` | `1823-RJ.js:890` (seeds `editMode.sourceMode`); `1823-RJ.js:1175-1181` (`is-live-preview` class + the live-preview CM plugin); `2062-uie.js:26` |
| `defaultViewMode` | `"source"` \| `"preview"` | `"source"` | `1961-Q3.js:886-887` |

```js
// app/own/1961-Q3.js:886-887  (MarkdownView constructor)
var a = n.app.vault.getConfig("defaultViewMode");
a && r.hasOwnProperty(a) || (a = "source"), n.currentMode = r[a];
```

Three consequences the doc does not state:

- **`defaultViewMode` picks the renderer; `livePreview` picks the sub-mode.** An unknown/absent value silently falls back to `"source"`; `livePreview: false` + `defaultViewMode: "source"` is the **raw source** mode, `livePreview: true` + `"source"` is live preview, and either `defaultViewMode` is reading view.
- There is a **command** that flips `defaultViewMode` and Notices it: `app:toggle-default-new-pane-mode` (`app/own/2075-Vie.js:815-821`, Notices `bd.interface.msgSwitchedToRead()` / `msgSwitchedToEdit()`). The doc's §6 command table does not list it.
- `propertiesInDocument` is a **three**-valued dropdown — `visible` / `hidden` / `source` (`app/own/2062-uie.js:70-81`) — and the two CSS-class predicates use **different** comparisons: `canShowProperties()` requires `"visible" === v` (`1961-Q3.js:1211`, `1844-R1.js:214`), while the reading view applies `show-properties` for `"hidden" !== v` (`1533-QW.js:192`) and the live-preview editor for `"visible" === v` (`1823-RJ.js:1826`). The `"source"` value is what routes the properties editor into the CM live-preview widget (`app/own/1259-vO.js:51`).

`toggleSource()` is three statements (`1823-RJ.js:1328-1332`):

```js
this.sourceMode = !this.sourceMode, this.updateOptions(), this.cm.dispatch({ effects: [FL.of()] });
```

---

## 2. `canToggleBacklinks()` is an `instanceof` test, so it is `true` in raw source too — CORRECTION

§1 says "true if preview mode or live-preview edit mode". Actual:

```js
// app/own/1961-Q3.js:1073-1074
t.prototype.canToggleBacklinks = function() {
  return "preview" === this.getMode() || this.editMode instanceof t5;
},
```

`this.editMode` is always `new t5(n)` in `MarkdownView` (`1961-Q3.js:871`), so the second disjunct is **unconditionally true** and `canToggleBacklinks()` always returns `true`. It is not a live-preview test — raw source mode passes. The `instanceof` shape exists because the embed subclasses (`t1`/`i1`, `app/own/1835-t1.js:137,151`) start with `editMode === null` and only build one on demand.

The `toggleBacklinks()` path the doc omits also forces a layout save:

```js
// app/own/1961-Q3.js:1075-1089
(e = this.getState()).backlinks = !e.backlinks,
[4, this.leaf.setViewState({ type: n5, state: e })];
… o.sent(), this.app.workspace.requestSaveLayout();
```

---

## 3. The async preview worker is single-flight; contention falls back to a blocking main-thread parse — CORRECTION, and §13 Q3 answered

§8 invariant 8 and §12 describe the worker as "lazy singleton `Wz`" and ask (§13 Q3) whether 20 simultaneous preview panes serialise. The client is:

```js
// app/own/1500-wW.js:11-13
e.prototype.submit = function(e, t) {
  return this.promise ? null : (this.promise = Lw(), this.worker.postMessage(e, t), this.promise.promise);
}
```

**`submit` returns `null` when a request is already in flight** — there is no queue and no request id (`onReceiveMessageFromWorker`, `:6-10`, resolves whatever promise is pending with `e.data`). The caller does not check:

```js
// app/own/1506-TW.js:147-154
return [4, TW.submit({ parseSections: t, options: VT.globalOptions })];
}, … n = a.sent(), i = this.previewEl, r = i.scrollTop, this.parseFinish(t, n), …
}, o = a.sent(), this.parsing = !1, console.error(o), this.parseSync(), this.queueRender(), [3, 7];
```

`n` is `null` → `parseFinish(text, null)` dereferences `null.sections` → the `try`'s `catch` logs and runs **`parseSync()`**, which parses the whole note on the main thread. `TW` is module-level (`1506-TW.js:2`), so this contention is **app-wide**: any two large preview panes (reading view, hover popovers, PDF export is sync anyway, Publish-site renderer) collide.

Porting note: a Corbomite thread pool with a queue would produce *different* observable behaviour — the second pane updates late rather than immediately-blocking. Neither is wrong, but the Obsidian-faithful choice is the fallback, and it is the reason a >10 KB note never blocks on the worker at all in practice.

Also: `CW(workerPath)` (`app/own/1502-CW.js:2-16`) resolves `new URL(path, document.baseURI)` and **falls back to a non-`Worker` path `kW(path)`** if the origin differs from `location.origin` or the `new Worker` constructor throws. With the literal `"worker.js"` (`1531-YW.js:13`) the origin always matches, so the fallback is unreachable in a normal desktop build.

---

## 4. The parser option surface is `{breaks, commonmark}` and `commonmark` is hard-coded `true` — NEW, §13 Q1 answered

```js
// app/own/1080-expressionstatement.js:2-5   (main thread; worker mirror: worker/own/0154-expressionstatement.js:2-5)
VT.globalOptions = {
  breaks: !0,
  commonmark: !0
};
```

`VT` is `IE`, the shared markdown-parser module (`app/own/1079-VT.js:2`). A corpus-wide search for `globalOptions` in `app/own/` returns exactly four sites: the initialiser, the per-render `setOptions` (`app/own/1082-WT.js:5`), the worker post (`1506-TW.js:149`), and one write of `.breaks` (`1533-QW.js:194`). **`commonmark` is never reassigned by any config, setting, or command.** Obsidian's markdown is permanently parsed in CommonMark mode; there is no "disable CommonMark" escape hatch, so a faithful Corbomite parser must default to it.

`breaks` is the only user-visible one, and it is a **process-global**, exactly as §13 Q1 suspected:

```js
// app/own/1533-QW.js:193-194
t.prototype.updateStrictLineBreaks = function() {
  VT.globalOptions.breaks = !this.app.vault.getConfig("strictLineBreaks"), this.rerender();
},
```

Because `updateStrictLineBreaks` is a *method on each preview mode* that mutates a shared object, **the last preview view constructed or reconfigured wins for every view in the app** — including panes whose own `strictLineBreaks` read would differ. Since `strictLineBreaks` is a single vault-wide config today this is benign, but it means the setting is *structurally* not per-view.

The worker inherits it by **replacement, not merge**:

```js
// worker/own/0169-expressionstatement.js:126
if (t.hasOwnProperty("parseSections")) return n = t.parseSections, r = t.options, lr.globalOptions = r || {}, void xr(wr(n));
```

So the sync path (`parseSync` → `Qq`) and the worker path (`TW.submit`) always see the same options object, and a Corbomite worker must copy the whole object per job rather than diffing it.

(One cross-contamination site, outside this domain but citable as proof the global is app-wide rather than view-scoped: the **Publish** site renderer writes it too — `app/own/1802-rZ.js:225`, `VT.globalOptions.breaks = !n.getConfig(GQ)`.)

---

## 5. Reading view and the editor use different frontmatter-collapse fold encodings — CORRECTION to §2 / §8 item 11

§2's `FoldInfo` block says `// {0,0} = frontmatter collapsed` and §8 item 11 restates it. That is the **editor's** encoding only.

Editor / live-preview (`app/own/1961-Q3.js:820-835`) — filters out any `from === 0` fold and re-adds the sentinel explicitly:

```js
return this.view.canShowProperties() && a.properties.length > 0 && (r = r.filter(function(e) {
  return 0 !== e.from;
}), a.collapsed && r.unshift({ from: 0, to: 0 })), { folds: r, lines: o };
```

Reading view (`app/own/1506-TW.js:442-455`) — derives it from the section that owns the properties block:

```js
var r = (h = i[n]).el.find(".metadata-container");
if (r) {
  r.hasClass("is-collapsed") && t.push({ from: h.start.line, to: h.start.line + h.lines });
  break;
}
```

Both share the same `foldManager` record, and `setMode` cross-applies them on every switch:

```js
// app/own/1961-Q3.js:1034
null !== i && e.setEphemeralState({ scroll: i }), e.applyFoldInfo(r);
```

Both `applyFoldInfo` implementations key restoration off a `Set` of **`from` lines only** (`1506-TW.js:499-509`, `1823-RJ.js:1229-1248`), so the `to` half round-trips approximately and the `from` half exactly — which is why the asymmetry does not visibly break, and why neither mode's `to` value can be trusted as byte-exact. `lines` is `this.text.match(/^/gm).length` in reading view (`1506-TW.js:495,499`) and `doc.lines` in the editor (`1823-RJ.js:1227`) — both are line counts, and a mismatch skips restore entirely, including the unfold (`1506-TW.js:499` / `1823-RJ.js:1231`).

The embed subclass adds a third variant: `j1.onMarkdownFold` can `unshift({from: 0, to: 0})` from a saved local copy (`app/own/1844-R1.js:440-447`).

---

## 6. The 2000 ms save is `Ml`, i.e. a non-resetting throttle, and `save()` has two silent early-outs — CORRECTION to §3 item 1

§3 item 1 says "`.md` file — `TextFileView.save()` → `vault.modify(file, text)` (2000 ms debounce)" and §15 repeats "inherits 2000 ms save-debounce". The literal:

```js
// app/own/1813-CZ.js:6-9
var i = Ml(n.save.bind(n), 2e3);
n.requestSave = function() { n.dirty = !0, i(); }
```

`Ml` is the leading-edge, non-resetting throttle documented in `2026-10-03-editor-wrapper-commands-and-keymaps.md` §3.4. So the first `requestSave()` in a window schedules the write at `+2000 ms` and **subsequent keystrokes inside that window are dropped, not pushed out**: sustained typing writes to disk every 2 s, mid-sentence, rather than 2 s after the last keystroke. Any Corbomite `QTimer::singleShot`-per-keystroke save would diverge observably (Obsidian writes *more* often while typing continues, and *never* writes at all if the user pauses-and-then-types inside a window).

Two early-outs in `save()` the doc does not mention (`1813-CZ.js:39-60`):

- `this.lastSavedData === o || null === this.lastSavedData` → return without writing. **The first save after a file is loaded never happens**, because `lastSavedData` is null until a save has succeeded.
- `if (this.saving) return e || (this.saveAgain = !0)` — a `save(true)` (the `clear` form used by `onUnloadFile`) issued mid-save is dropped entirely rather than queued.
- On failure: restore `lastSavedData`, `console.error`, `Notice(bd.interface.msgFailToSaveFile(...), 0)`, and **`this.app.fileManager.storeTextFileBackup(path, text)`** — the failed content is written to the file-recovery store.
- `save(clear = true)` (the unload path, `1813-CZ.js:20`) additionally sets `data = null; lastSavedData = null;` and calls `clear()`.

Markdown-specific callers of the *immediate* (un-throttled) form, which therefore **bypass** the 2 s window entirely:

| Caller | Site |
|---|---|
| `setMode` → `await this.save()` on leaving source | `1961-Q3.js:1030` |
| reading-view `edit(text)` → `await this.view.save()` — i.e. **one disk write per checkbox click** | `1533-QW.js:218-229` |
| `saveFrontmatter(obj)` (properties editor) | `1961-Q3.js:915` |

And the trigger chain that arms the throttled save (`app/own/1961-Q3.js:843-846`):

```js
t.prototype.onUpdate = function(t, n) {
  e.prototype.onUpdate.call(this, t, n);
  var i = t.transactions;
  n && (this.view.requestSave(), this.requestSaveFolds(), this.requestOnInternalDataChange()),
  (RL(i, fh) || RL(i, vh)) && this.requestSaveFolds();
}
```

`n` is the same `docChanged && isUserEvent` flag as the autocorrect gate (`1823-RJ.js:1055`), so **`userEvent === "set"` suppresses all three** — save, fold flush, and `quick-preview`. §12's "Per-keystroke `quick-preview` emission … via `editor.updateListener` + 10 ms debounce on the `requestOnInternalDataChange`" is confirmed; the two things it omits are (a) `requestSave` is on the *same* 10 ms path, and (b) both are `Ml`, so §12's "a linked preview pane re-renders ~100 times while typing a paragraph" is an upper bound — the real ceiling is **100 Hz**, and the 2 s save lands once mid-burst.

---

## 7. Ctrl+Wheel font zoom is a leading-edge throttle, and is disabled on macOS pinch — CORRECTION to §3 item 4 / §9

§3 item 4 says "500 ms debounce". §9 says "guarded on `baseFontSizeAction` config". Both halves need fixing:

```js
// app/own/1961-Q3.js:888-903
var s = 0;
n.containerEl.addEventListener("wheel", function(e) {
  if (n.app.vault.getConfig("baseFontSizeAction")) {
    var t = e.ctrlKey, i = Date.now();
    if (t && !(s > i - 500)) {
      if (t && 0 !== e.deltaY) {
        if (Zp && Number.isInteger(e.deltaY)) return;
        var r = n.app.vault.getConfig("baseFontSize");
        e.deltaY < 0 ? r++ : r--, r = Math.clamp(r, 10, 30), n.app.vault.setConfig("baseFontSize", r);
      }
    } else s = i;
  }
}, { passive: !0 });
```

- Not a debounce: `s` is only advanced by a *suppressed* event, so the **first** Ctrl+Wheel of a burst applies and the rest are dropped for 500 ms after each drop. (`s` starts at `0`, and `0 > Date.now() - 500` is false, so the very first event always takes the apply branch.)
- `Zp` is `isMacOS` (`app/own/0519-Qp.js:12`, `app/own/0521-rd.js:11`). **On macOS, a Ctrl+Wheel with an integer `deltaY` returns immediately** — that is the pinch-zoom signature macOS synthesises. So pinch-to-zoom never changes the font size; only a trackpad with fractional `deltaY` does.
- Wheel **up increases** the font size (`e.deltaY < 0 ? r++ : r--`), clamp `[10, 30]` confirmed, and every step is an immediate `vault.setConfig` write — no debounce on the config write itself.

---

## 8. The preview double-tap window is **500 ms**, not 300 ms — CORRECTION to §9 / §1

`§9` says "double-tap (touch) to toggle mode" and §1 says "double-tap within 300 ms / <30 px travel". The wrapper passes no thresholds:

```js
// app/own/1533-QW.js:88-91
ug(i, function() { var e = i.win.getSelection(); e && !e.isCollapsed || n.view.toggleMode(); });
```

```js
// app/own/0708-ug.js:2-7
function ug(e, t, n, i) { pg(e, 2, t, { maxDelay: n, maxDistance: i }); }
```

so the defaults apply:

```js
// app/own/0710-pg.js:5-6
a = null !== (r = i.maxDelay) && void 0 !== r ? r : 500,
s = null !== (o = i.maxDistance) && void 0 !== o ? o : 30,
```

**500 ms / 30 px.** Also undocumented: `pg` is touch-only (`0 === t.button && "touch" === t.pointerType` on `pointerdown`, `0710-pg.js:74-78`), requires exactly two taps (`c < t` with `t === 2`), and resets its counter on any non-collapsed DOM selection (`:35-38`) or ≥30 px cumulative pointer travel (`:50`). The `!selection.isCollapsed` re-check at the call site (`1533-QW.js:89-90`) is belt-and-braces on top of that.

The `contextmenu` guard is also wider than the doc's "not touch":

```js
// app/own/1533-QW.js:87
e.defaultPrevented || !e.isTrusted || e.instanceOf(PointerEvent) && "touch" === e.pointerType
  || rd.isMobile && window.getSelection() && !window.getSelection().isCollapsed
  || r.sizerEl.contains(e.targetNode)
  || n.app.workspace.trigger("markdown-viewport-menu", my.forEvent(e).addSections(["view", ""]), t, "preview", "gutter");
```

Six terms — so `markdown-viewport-menu` in reading view fires only on a trusted, non-touch, selection-free click **outside** the sizer. §9's "right-click on gutter/empty viewport" is right for reading view; §4's `:195` citation is dead.

---

## 9. `registerMarkdownCodeBlockProcessor`'s third argument is `sortOrder`; there is no `-100` — CORRECTION to §10

§10's table says code-block processors are registered via `createCodeBlockPostProcessor` + `registerPostProcessor(fn, -100)`, implying they run before everything. Actual:

```js
// app/own/1938-c4.js:93-98
Plugin.prototype.registerMarkdownCodeBlockProcessor = function(e, t, n) {
  var i = this, r = AW.createCodeBlockPostProcessor(e, t);
  return AW.registerPostProcessor(r, n), AW.registerCodeBlockPostProcessor(e, t),
  this.app.workspace.trigger("post-processor-change"), this.register(function() {
    AW.unregisterCodeBlockPostProcessor(e), AW.unregisterPostProcessor(r), i.app.workspace.trigger("post-processor-change");
  }), r;
},
```

The third parameter is passed **straight through as the sortOrder**. There is no default and no `-100`. With no third argument the wrapper sorts as `0`:

```js
// app/own/1506-TW.js:723-728
e.registerPostProcessor = function(e, t) {
  e.sortOrder = t;
  var n = this.postProcessors;
  n.push(e), n.sort(function(e, t) { return (e.sortOrder || 0) - (t.sortOrder || 0); });
}
```

Two consequences: (a) `sortOrder` is written as a **property onto the function object** and the array is fully re-sorted on every registration (`Array.prototype.sort` is stable, so equal orders keep registration order); (b) a plugin's code-block processor therefore **interleaves** with plain post-processors in registration order, and can run *after* a plain processor that would have rewritten the same `<code>` element. A Corbomite port that defaults code-block processors to "run first" would be *more* deterministic than Obsidian and would diverge on plugins that rely on ordering.

§4's note is also worth sharpening: the **static** `registerPostProcessor` does not fire `post-processor-change`; the **`Plugin` wrapper does**, on both register and unregister (`1938-c4.js:90,91`). The doc's "that happens at `workspace.registerEditorExtension`-style call elsewhere" is the wrong mechanism.

Also confirmed and worth having: `registerCodeBlockPostProcessor` throws with the verbatim message

```js
// app/own/1506-TW.js:737
throw new Error("Code block postprocessor for language ".concat(e, " is already registered"));
```

and `unregisterPostProcessor` / `unregisterRecycler` / `unregisterCodeBlockPostProcessor` all exist (`:729-730`, `:733-734`, `:739-740`), the last being `delete codeBlockPostProcessors[lang]`.

---

## 10. `usesFrontMatter` is a post-processor opt-in written onto the context, not a parse result — NEW

§1's recycle algorithm and §12's "Frontmatter-diff re-render" describe `usesFrontMatter` as if the renderer derived it. It does not. It is initialised `false` on every section (`app/own/1503-EW.js:11`) and the **only** writer in the entire corpus is the copy-back from the post-processor context:

```js
// app/own/1531-YW.js:79  (instance postProcess)
e.usesFrontMatter = !!r.usesFrontMatter, this.resolveLinks(e.el);
```

A corpus-wide grep for `usesFrontMatter` returns five hits: three consumers, two copy-backs (`1531-YW.js:79`, `1796-K$.js:58`), and the initialiser. **Nothing in Obsidian's own bundle sets it** — it is purely a plugin-API contract (`ctx.usesFrontMatter = true` inside a post-processor). So the §12 two-level-invalidation story is right for Dataview/Templater-class plugins and inert for everything else; the renderer is not doing frontmatter analysis at all.

The invalidation consumer:

```js
// app/own/1506-TW.js:191-195
var D = JSON.stringify(this.frontmatter) === JSON.stringify(i);
if (this.frontmatter = i, D)
  for (var A = 0, P = d; A < P.length; A++) {
    (k = P[A]).usesFrontMatter && (k.rendered = !1, k.el.empty());
  }
```

Note the guard is `D` = **unchanged**, so force-re-render happens only when the frontmatter did *not* change — i.e. a frontmatter change relies on the recycle key (`html`) already differing. §12 is correct here.

---

## 11. `postProcess` also resolves `code.language-query` — and `render()` does not — NEW

`MarkdownRenderer.postProcess` has a built-in first pass the doc omits entirely:

```js
// app/own/1531-YW.js:94-100
t.postProcess = function(e, t) {
  for (var n = t.sourcePath, i = t.promises, r = t.el, o = t.displayMode, a = 0, s = r.findAll("code.language-query"); a < s.length; a++) {
    var l = s[a], c = FW(l).trim(), u = l.parentElement;
    t.addChild(XW(e, c, u, n));
  }
```

A ```` ```query ```` fenced block becomes a Bases embed child, before any plugin post-processor runs, in **reading view and live preview**. The one-shot `MarkdownRenderer.render(...)` path does **not** do this — it goes straight to the `postProcessors` loop (`1531-YW.js:157-160`). So ```` ```query ```` renders as an embed in a preview pane and as inert highlighted text inside a plugin's ad-hoc `MarkdownRenderer.render` call.

Also: the static `postProcess` **mutates and returns the ctx** (`return t`, `:122`), and the instance wrapper reads `r.usesFrontMatter` off that same object — which is the mechanism in §10. `resolveLinks(el)` is also invoked per-section from the instance wrapper (`:79`), not only from the 500 ms `requestUpdateLinks` sweep.

---

## 12. `ctx.replaceCode` is guarded by an optimistic-concurrency check — NEW

§1 says `replaceCode(newSrc)` "rewrites source at the code-block's line range". The rewrite only fires if the source is still what the processor saw:

```js
// app/own/1506-TW.js:751-761
s.replaceCode = function(e) {
  var t = i.getSectionInfo(a);
  if (t) {
    var o = t.text.substring(am(t.text, t.lineStart).start, am(t.text, t.lineEnd).end),
      s = parseInt(n.dataset.line, 10);
    isNaN(s) && (s = 0);
    var l = am(o, s + 1).start, c = l + r.length;
    o.substring(l, c) === r && (i.replace(o.substring(0, l) + e + o.substring(c), !0), OW.set(n, e), r = e);
  }
};
```

The offset is the `<code>` element's own **`data-line`, 0-based within its section** (`+1` to get the 1-based `am()` line), and the replaced span is exactly `r.length` — the source the processor was handed. A processor that resolves slowly and fires after the user has edited that block **silently no-ops** rather than corrupting the file. Any Corbomite `replaceCode` without this guard is a data-corruption surface.

Companion detail: `getSectionInfo` returns the **whole document** as `text`, not the section's slice:

```js
// app/own/1506-TW.js:64-71
e.prototype.getSectionInfo = function(e) {
  if (!this.belongsToMe(e)) return null;
  var t = this.getSectionForElement(e);
  return t ? { text: this.lastText || "", lineStart: t.start.line, lineEnd: t.end.line } : null;
}
```

which is why `replaceCode` has to substring by `lineStart`/`lineEnd` itself, and why `ctx.replace`'s splice (`1531-YW.js:65-75`) uses `section.start.offset` / `section.end.offset` rather than the info object.

`ctx.replace`'s in-place HTML swap — §2's "if `rerender=true` AND the new section renders as one block, in-place HTML swap skips a full re-parse" — is confirmed, with the exact condition:

```js
// app/own/1531-YW.js:65-75
replace: function(t, n) {
  var r = i.renderer.text, o = e.start, a = e.end,
    s = r.slice(0, o.offset) + t + r.slice(a.offset);
  if (n) { var l = Qq(t); 1 === l.sections.length && (e.html = l.sections[0].html); }
  i.edit(s);
}
```

Note `Qq(t)` re-parses the *replacement text alone*, so the one-section test is against the fragment, not the document.

---

## 13. Virtual-scroll constants and three undocumented budgets — NEW

§1 step 4 and §12 give `renderExtra = 1` / `renderExtraMinPx = 500` and the selection-extension rule; both confirmed. §1's "A 10000-line note ends up with ~20 sections in the DOM at a time" is an estimate with no source. The real numbers:

```js
// app/own/1506-TW.js:12 (constructor)
this.renderExtra = 1, this.renderExtraMinPx = 500, this.topSpace = 0, this.parsing = !1, this.scrolling = !1
```

```js
// app/own/1506-TW.js:298
var p = n.clientHeight, d = a + p, f = Math.min(a, n.scrollHeight - p),
    v = Math.max(p * this.renderExtra, this.renderExtraMinPx), m = f - v, g = d + v
```

So the window is `[scrollTop − max(viewportH, 500), scrollTop + viewportH + max(viewportH, 500)]` — **500 px of overscan on each side, or one viewport height, whichever is larger**, plus the top clamp at `scrollHeight − viewportH`.

Three things the doc does not mention:

**(a) A second render budget at 100 sections.** §1 documents only the "every 10 sections, if `> 5 ms`, break" rule (confirmed verbatim at `1506-TW.js:247`). The *assembly* loop that follows has its own cap:

```js
// app/own/1506-TW.js:251-258
for (var v = [], m = !1, g = [s], y = 0, b = 0, w = o; b < w.length; b++) {
  var k = (S = w[b]).el, C = k.parentElement === a;
  if (!S.computed && S.rendered ? (C = !0, v.push(S), m = !0, y++) : m && (C = !1, m = !1), C && g.push(k), y > 100) {
    u = !0; break;
  }
}
```

**(b) Scroll only re-runs virtualisation past half the overscan.**

```js
// app/own/1506-TW.js:533
Math.abs(n - r) > Math.max(i * this.renderExtra, this.renderExtraMinPx) / 2 && this.updateVirtualDisplay(),
```

**(c) An unmeasured section's height falls back to the mean of measured ones**, in three separate places: the walk (`1506-TW.js:299-302`, `k = y / b`), the visibility pass (`:323`, `0 === F && (E += k)`), and `getSectionTop` (`:631`, `o = n / i` — a running average rather than a global mean). And `measureSection` derives height from the **next sibling's** `offsetTop`:

```js
// app/own/1506-TW.js:389-396
var t = e.el, n = t.nextSibling, i = t.offsetTop,
    r = n ? n.offsetTop : t.offsetTop + t.offsetHeight;
e.height = r - i, e.computed = !0;
```

which is why the assembly loop above has to exist at all: a section is only measurable once its successor is in the DOM, so the last section in the current window is always one frame behind.

Two more precise numbers:

- **iOS scroll-restore suppression**: `var s = !(rd.isIosApp && Date.now() - this.lastScrollTs > 100);` (`1506-TW.js:287`) — on iOS, `updateVirtualDisplay` will not write `scrollTop` back within 100 ms of the last scroll. `1506-TW.js:295,339`.
- `renderHighlights` applies a **zoom/DPR correction** `a = r.offsetWidth / Math.round(o.width)` to every client rect (`1506-TW.js:347`), where `r = i.offsetParent`. §1's description of `.search-highlight` is otherwise confirmed verbatim, including the `is-active` class per range (`:378`) and that the overlay is rebuilt from scratch on every `updateVirtualDisplay` (`:384-387`). `highlightEl` is the other flash path: `.is-flashing` for **3000 ms** (`:655-662`).

`progressiveRender: !0` / `asyncParse: !0` / `listBullet: !0` are hard-coded module constants (`1506-TW.js:3-7`) with no config surface; the `progressiveRender === false` branch exists and attaches every section (`:288-296`).

---

## 14. `queueRender` is rAF-vs-idleCallback, not two timeouts — CORRECTION to §1 step 1

§1 says "`queueRender()` schedules `onRender` via `Pc(fn, delay)` (idle-callback-ish): `0 ms` if not parsing, else `200 ms`". The two branches are different *schedulers*:

```js
// app/own/1506-TW.js:123-131
e.prototype.queueRender = function() {
  this.rendered || (this.rendered = []);
  var e = this.queued, t = !this.parsing;
  if (e) t && !e.high && (e.cancel(), this.queued = Tv(this.onRender.bind(this), 0));
  else { var n = t ? 0 : 200; this.queued = Tv(this.onRender.bind(this), n); }
}
```

```js
// app/own/0629-Tv.js:2-28
function Tv(e, t) {
  var n = activeWindow;
  if (t > 0 && n.requestIdleCallback) { … requestIdleCallback(…, { timeout: t }) … return { high: !1, cancel: … }; }
  var a = n.requestAnimationFrame(e);
  return { high: !0, cancel: … };
}
```

So: **normal path = `requestAnimationFrame`** (frame-aligned, `high: true`); **parsing path = `requestIdleCallback` with a 200 ms timeout** (`high: false`); and a pending idle-callback render is **cancelled and upgraded to an rAF** the moment parsing finishes. A `QTimer::singleShot(0)` port would coalesce differently from a frame-aligned one and would show tearing that Obsidian never shows.

Same story in `MarkdownPreviewRenderer`'s constructor: `Sl(function(){ return a.onResize(); })` where `Sl = queueMicrotask` (`app/own/0356-Sl.js:2`) — a single microtask `onResize()`, not a resize observer. §1's constructor description is otherwise confirmed, including the 1×0.1 px `pusherEl` (`1506-TW.js:15`).

---

## 15. Reading-view specifics the doc gets right but does not quantify — CONFIRMED + NEW

| Claim in `editor-markdown.md` | 1.13.7 evidence |
|---|---|
| Preview mode object is `MarkdownPreviewView`, `type = "preview"`, owns a `renderer` | `1533-QW.js:72-74` |
| 7 config-update methods run in the constructor | `1533-QW.js:74` — eight calls; the eighth is `updateOptions()`, which is a **no-op** on this subclass (`:218`). `MarkdownView.updateOptions` fans out to all modes (`1961-Q3.js:1205-1207`) and `MarkdownEditMode.updateOptions` is also empty |
| `addBottomPadding` adds viewport/2 | `1506-TW.js:406` — `addBottomPadding ? Math.round(clientHeight / 2) + "px" : "0"` |
| `set` clears when `clear=true` | `1533-QW.js:110-112` (`t && this.clear(), this.renderer.set(e)`) |
| `get()` is `renderer.text` | `1533-QW.js:108-109` |
| `getSelection()` is `window.getSelection().toString()` | `1533-QW.js:245-246` |
| `getEphemeralState` is preview-silent | `1533-QW.js:116` — literally `function(e) {}`; the view's `scroll` comes from `MarkdownView.scroll` (`1961-Q3.js:985`) |
| `show()` moves inline-title + properties into the renderer header and backlinks into the footer | `1533-QW.js:104-105` — done on **every** `show()`, i.e. on every mode switch back to reading view |
| `strictLineBreaks` mutates the shared parser option and rerenders | `1533-QW.js:193-194` (see §4) |
| `post-processor-change` → `rerender(true)` | `1533-QW.js:95-97` |
| `onRenderComplete` hook | `1533-QW.js:233-234` — NEW detail: it also re-runs `this.search.updateQuery()` when the search widget has no `highlightRanges` yet |
| `foldAll`/`unfoldAll` respect both configs | `1533-QW.js:235-244` — NEW detail: `updateFoldHeading`/`updateFoldIndent` call `unfoldAllHeadings()`/`unfoldAllLists()` when the config is **off** (`1533-QW.js:180-185`), so turning the setting off *destroys* existing heading/list folds rather than merely hiding the triangles |
| Reading view sets `dir`/RTL from `rightToLeft` | `1533-QW.js:189-190` — `.rtl` on `previewEl` only (matches the wrapper addendum's per-table-cell correction for the *rendered* DOM) |

**Scroll-sync is rate-limited and the doc omits it:**

```js
// app/own/1533-QW.js:230-232
t.prototype.onScroll = function() {
  var e;
  Date.now() - this.renderer.lastRender < 100 || (this.view.syncScroll(), … mobileNavbar.onScroll(…));
}
```

**`markdown-scroll` is suppressed for 100 ms after any parse completes** (`lastRender` is stamped in `parseFinish`, `1506-TW.js:210`). §9's "Scroll-sync across linked-pane-group peers via `markdown-scroll`" is confirmed; the missing fact is that scroll-sync is a no-op while a render is in flight, so opening a big note does not spam peers.

`applyScroll` also has two hard refusals the doc does not list (`1506-TW.js:577-580`): non-numeric input returns `false`, and **`this.text !== this.lastText` returns `false`** — i.e. a scroll request issued before the parse has caught up is dropped, not queued. `applyScrollDelayed` compensates with exactly **one** `onRendered` retry (`:650-654`), so a request issued during a parse that then takes two render frames to settle is still lost.

`showSection`'s auto-reveal — §12's "walks *backwards* through sections expanding any ancestor-heading that's collapsed" — is confirmed verbatim (`1506-TW.js:634-645`, `for (var i = e.level; n >= 0;)`), and `applyScroll` recurses after revealing (`1506-TW.js:588`).

`updateShownSections`'s collapse-level sentinel is **7**, not 0 or 6:

```js
// app/own/1506-TW.js:274-278
for (var e = 7, t = 0, n = this.sections; t < n.length; t++) {
  var i = n[t];
  i.level > e ? i.shown = !1 : (i.shown = !0, e = 7, i.headingCollapsed && (e = i.level));
}
```

`MarkdownPreviewSection.setCollapsed` toggles `is-collapsed` on **two** elements:

```js
// app/own/1199-cI.js:2-6
function cI(e, t) { e.toggleClass("is-collapsed", t); var n = e.find(".collapse-icon"); n && n.toggleClass("is-collapsed", t); }
```

and `resetCompute()` also nulls the cached client rects of every highlight range (`1503-EW.js:24-30`), not just `computed`.

The collapse indicators themselves are gated more tightly than the doc implies:

```js
// app/own/1504-SW.js:2-5  — heading: FIRST CHILD only
function SW(e, t) { var n = e.firstChild; n && n.instanceOf(HTMLHeadingElement) && t(n); }
// app/own/1505-xW.js:2-8 — list: any li with a DESCENDANT ul/ol
function xW(e, t) { for (…) { var r = i[n], o = r.findAll("ul, ol"); o && o.length > 0 && t(r); } }
```

`xW` uses a descendant search, so **every ancestor `li` of a nested list gets a collapse triangle**, not just the nearest parent. And `SW` means a section whose first child is not a heading gets no heading indicator even if it contains one — which is the implicit requirement that the section splitter emit one block per section.

---

## 16. `MarkdownPreviewRenderer` has four construction sites and two worker paths — NEW

| Site | Worker path | `listenForInsertion` | Consumer |
|---|---|---|---|
| `1531-YW.js:13` | `"worker.js"` | passed through (default `true`) | `MarkdownRenderer` → reading view, hover popovers, embeds |
| `1796-K$.js:6` | **`"/worker.js"`** | default `true` | **Publish site renderer** (`K$`), which also calls `addHeader()`/`addFooter()` eagerly (`:7`) and stubs every `on*Link*` handler |
| `1939-m4.js:1608` | `"worker.js"` | **`!1`** | the "reader"/web-viewer mode, which also re-uses the same class for a non-markdown document |
| `1533-QW.js:34`, `1961-Q3.js:1381` | — (none) | — | `MarkdownRenderChild` / PDF export call `AW.registerDomEvents` + a hand-built ctx **without** constructing a renderer |

`listenForInsertion` gates `previewEl.onNodeInserted(() => queueRender())` (`1506-TW.js:18-19`) — so the reader mode deliberately does **not** re-render on DOM mutation, and Corbomite needs the same switch if it ever grows a reader mode.

`registerDomEvents` takes **four** arguments, not three:

```js
// app/own/1506-TW.js:768-775, 813
function o(n) {
  var r = n.getAttr("data-href") || n.getAttr("href");
  return r && e.belongsToMe(n, t, i) ? { href: r, displayText: n.getText().trim() } : null;
}
…
t.on("click", "img,video", s), t.on("contextmenu", "img,video", s);   // s → the 4th arg, mediaHandler
```

and the wiring differs from the doc in three details (`1506-TW.js:787-813`):

- internal links fire on **`click` *and* `auxclick`**, both for `button === 0` or `1` (`:787-788`) — so middle-click navigates;
- `displayText` is the **DOM text** of the anchor, `.trim()`ed — not the `|` alias;
- `a.tag` click calls `e.preventDefault()` before delegating (`:807-813`), whereas internal/external links do not.

The media handler is not a zoom-in-place but a **lightbox over the whole preview's image list**:

```js
// app/own/1506-TW.js:41-57
… for (each section) for (each img) i.push({ url: c.src, alt: c.alt, title: c.title }), t === c && (r = i.length - 1, n = c);
if (-1 !== r) { new YL(i, r, n, function() { return t.focus(); }).open(); }
```

§9's "pinch-zoom image viewer on embedded images in preview" is confirmed; the "index into the full preview" behaviour is not documented.

Two further undocumented methods: `selectRange({section, start, end})` (`:672-700`, maps offsets to a DOM range with `bm(el, from, to)` and scrolls with a **40 px** keep-out and `block: "center"`), and `getSectionTop(section)` (`:626-633`, returns `-1` when the section is not in the list). `cleanupParentComponents` (`:715-722`) walks `owner._children` and removes every `MarkdownRenderer` descendant that does **not** belong to this preview — §10's row is confirmed, including the `_children` private access.

`clear()` (`:33-34`) does more than §1 lists: it resets `text`/`lastText`/`lastScroll`, sets `previewEl.scrollTop = 0`, calls `owner.onRenderComplete()` **unconditionally**, and then `cleanupParentComponents()`.

---

## 17. PDF export — a popup window with the light theme forced, and un-i18n'd strings — NEW

§3 item 3's `pdfExportSettings` schema is **confirmed verbatim**: `{includeName, pageSize, landscape, margin, downscalePercent}`, `pageSize` ∈ `A3|A4|A5|Legal|Letter|Tabloid`, `downscalePercent` slider limits `(10, 100, 1)`, and the `margin` dropdown's option order is `"0"`(Default), `"2"`(Minimal), `"1"`(None) — i.e. the doc's `"0"=default / "1"=none / "2"=minimal` mapping is right even though the emitted order looks transposed (`1961-Q3.js:1254-1296`, write at `:1291`).

Undocumented in the doc:

```js
// app/own/1961-Q3.js:1291-1296
i.marginsType = parseInt(n.margin),
1 !== i.marginsType && 2 !== i.marginsType || (i.margins = { top: 0, left: 0, bottom: 0, right: 0 }),
i.scaleFactor = n.downscalePercent,
i.scale = Math.clamp(i.scaleFactor / 100, .1, 2),
i.open = !0,
```

so `margin: "0"` writes **no** `margins` key at all, `scale` is separately clamped to `[0.1, 2]` (a 10 % slider still renders at 2× in the popup), and `open: true` means the print window is always shown.

The print pipeline:

```js
// app/own/1961-Q3.js:1328-1332
t = window.open("about:blank", "_blank", "popup,hide=true"),
n = createEl("base", { href: location.href }),
t.document.head.appendChild(n), t.document.title = this.file.basename, …
sleep(200);
i.body.removeClass("theme-dark"), i.body.addClass("theme-light"),
```

**PDF export always renders in the light theme**, regardless of the app theme, and injects a `<base href>` so relative asset URLs resolve. `print()` (`:1366-1405`) builds `div.print > div.markdown-preview-view.markdown-rendered` with `rtl` from `rightToLeft` and `show-properties` for `"hidden" !== propertiesInDocument`, prepends an `<h1>` with the basename iff `includeName`, then runs the full chain `WT → KT → GT → RF` (matching the rendering addendum's six-step chain) and `$W.postProcess` with **stub** `getSectionInfo`/`replace`:

```js
// app/own/1961-Q3.js:1389-1394
getSectionInfo: function() { return null; },
replace: function() { return null; },
```

The save dialog is Electron's, with `defaultPath: basename + ".pdf"`, filters `[{name:"PDF Files",extensions:["pdf"]},{name:"All Files",extensions:["*"]}]`, `properties: ["showOverwriteConfirmation"]` (`:1296-1306`). §12's "Print-export rewrites links" is **confirmed** — `for (y = i.findAll("a.internal-link"), …) k[w].removeAttribute("href");` (`:1402`), after all post-processors have resolved.

Two implementation notes for a Corbomite PDF path: the modal's three user-facing strings are **hard-coded English**, not routed through `bd()`:

```js
// app/own/1961-Q3.js:1342, 1344, 1299, 1302
new Aw("PDF Saved to ".concat(e.filepath)),
new Aw("Failed to save PDF."),
{ name: "PDF Files", … }, { name: "All Files", … }
```

whereas every menu item in the same file *is* i18n'd. And `testPrint()` (`:1352-1365`) is a debug-only variant that skips the popup and the save dialog — it has no caller in the corpus.

---

## 18. `saveFrontmatter` is a whole-block splice with LF fences, and `cssclasses` values containing a space are dropped — NEW

§1 says `saveFrontmatter(obj)` "edits YAML via `kI(viewData, obj)` helper, full round-trip". Confirmed, and the exact splice matters:

```js
// app/own/1318-SR.js:2-6
function SR(e, t) { var n = MR(t), i = _T(e).contentStart; return n + e.slice(i); }
```

```js
// app/own/1317-MR.js:2-10
function MR(e) {
  var t = "";
  if (e && Object.keys(e).length > 0) {
    var n = bL(e).trim();
    t = "---\n".concat(n, "\n---\n");
  }
  return t;
}
```

Three consequences:

1. The **new** frontmatter block is prepended and everything from `contentStart` (just past the old closing fence) is preserved — the body is never touched.
2. The fences and the trailing newline are written as **LF unconditionally**, regardless of the file's existing line endings. This is a *different* writer from the parser's `updateFrontMatter` that the parsing addendum found preserves CRLF delimiters, and it is reachable from the properties editor (five `owner.saveFrontmatter` call sites in `app/own/1341-oN.js:297,305,308,317`). A CRLF vault that edits a property through the UI gets an LF frontmatter block. Corbomite must decide explicitly whether to reproduce that.
3. `MR({})` returns `""`, i.e. **emptying the properties object strips the entire frontmatter** — that is the delete-all-properties path (`2075-Vie.js:751` calls `SR(text, {})` directly).

The CSS-class bridge the doc calls `rT(parsed)`:

```js
// app/own/1093-JT.js:2-8
function JT(e) {
  if (!e) return null;
  var t = QT(e, /^cssclasses$/i);
  return t ? t.filter(function(e) { return !!e && !e.includes(" "); }) : null;
}
```

Only the `cssclasses` key (matched case-insensitively) contributes classes, and **any value containing a space is silently dropped**. It is applied at three sites: `loadFrontmatter` (`1961-Q3.js:1185`), the embed subclass (`1844-R1.js:234`), and the reading-view recycle (`1506-TW.js:196-200`, which removes the previous class set first).

---

## 19. `MarkdownRenderChild` / embed contract — CONFIRMED with additions

- **CONFIRMED** `MarkdownRenderChild extends Component` and its constructor only stores `containerEl` (`1503-EW.js:2-8`).
- **CONFIRMED** the embed is a `MarkdownRenderChild` whose constructor adds `markdown-embed` and, when `showInline`, `inline-embed` + a `embed-title markdown-embed-title` header (`1533-QW.js:2-9`).
- **NEW** `loadFile` resolves the subpath *before* reading and **replaces the whole buffer** with the subpath content (`1533-QW.js:32-34`: `o = PD(cache, i) … o && (a = ID(a, r, o).content)`), so a `![[Note#Heading]]` embed renders only the heading's block range.
- **NEW** the embed's ctx also stubs `getSectionInfo`/`replace` (`1533-QW.js:42-47`) and passes `displayMode: !0`, and it applies `show-indentation-guide` from `showIndentGuide` (`:34`) — a class the reading view sets but the doc's §1 constructor list omits.
- **NEW** `cleanupParentComponents` removes `MarkdownRenderer` children whose container is *not* inside this preview (`1506-TW.js:715-722`); `MarkdownRenderChild`s that are plain `Component`s are left alone.
- `belongsToMe`'s stop conditions are confirmed verbatim — `.markdown-preview-view` **or** `.bases-view` — and note it climbs to the **detached-root** fallback `n(e)` (the `handleDetached` filter) when there is no `parentElement` (`1506-TW.js:814-823`, instance wrapper `:35-40`).

---

## 20. Commands — the three the doc guessed are right, and they carry more than §6 records

§6 says the IDs are "registered in the internal-markdown-plugin domain, not here: candidates … `markdown:toggle-preview`, `editor:toggle-source`, `workspace:export-pdf`". All three confirmed, with semantics §6 does not give:

```js
// app/own/2075-Vie.js:691-700
id: "markdown:toggle-preview", name: bd.commands.togglePreview(), icon: "lucide-book-open",
checkCallback: function(t) { var n = e.workspace.activeEditor; if (n) return t || n.toggleMode(), !0; },
hotkeys: [Yw(["Mod"], "E")]
```

- `markdown:toggle-preview` is **`Mod+E`** — §9's "`Ctrl+E` hotkey" is confirmed — and it drives `workspace.activeEditor`, so it is greyed out with no markdown editor focused. `toggleMode` flips only `state.mode` and calls `setViewState(..., {focus: true})` (`1961-Q3.js:1011-1016`).
- `editor:toggle-source` (`2075-Vie.js:981-996`) requires `activeEditor instanceof MarkdownView && currentMode === editMode`, so the command is **unavailable in reading view** (not just a no-op there), and its action sets `active: !0` and calls `workspace.requestSaveLayout()` — unlike the pane-menu path (`1961-Q3.js:1149-1156`), which omits `active`.
- `workspace:export-pdf` (`1932-r4.js:540-546`) has **no hotkey**, is gated on `rd.canExportPdf`, and resolves `getActiveViewOfType(MarkdownView)`.

§6's "`editor-menu` contributions … add 'Rename heading'/'Rename block ID'" is **confirmed but mis-sourced**: both items are added by **workspace-level** `editor-menu` listeners registered once at module init, not by the `Editor` subclass, and they mutate the already-built menu:

- "Rename heading" — section `action`, icon `lucide-edit-3`, gated on `XE(cursorLine)`; opens `gR` (`1932-r4.js:268-289`).
- "Rename block id" — section `selection`, icon `lucide-edit-3`, gated on `getClickableTokenAt(cursor).type === "blockid"`; opens `yR` (`:291-309`).

There is also an `editor:rename-heading` **command** (`editorCheckCallback`, icon `lucide-edit-3`, `1932-r4.js:547-556`) that §6 does not list.

§6's mode-button description needs one correction: the tooltip is not set at construction — `addAction("lucide-book-open", "", onSwitchView)` registers an **empty** tooltip, and `updateButtons()` then installs a three-line `\n`-joined string:

```js
// app/own/1961-Q3.js:1017-1023
var n = bd.interface.menu.modClickOpenNewTab({ key: ay.Mod });
"source" === e.type
  ? (Ag(t, "lucide-book-open"), Zg(t, bd.interface.menu.edit-view) + "\n" + bd.interface.menu.switch-to-read-view) + "\n" + n)
  : (Ag(t, "lucide-edit-3"), Zg(t, bd.interface.menu.read-view) + "\n" + bd.interface.menu.switch-to-edit-view) + "\n" + n);
```

(Operator precedence makes the `+ "\n" + n` apply to both arms.) §6's "tooltip includes `Mod+click → open in new tab`" is confirmed, but the icon/tooltip are rewritten on **every** mode switch, and `onSwitchView`'s mod-click split is `createLeafBySplit` + `i.group = this.leaf` + `i.active = !0` (`1961-Q3.js:1062-1065`).

The pane-menu **section list** is confirmed item-for-item (`1961-Q3.js:1129-1169`): `pane` toggle-reading-view, `action` add-property (`setDisabled(!canShowProperties())`, icon `lucide-plus-circle`), `pane` toggle-source-mode (**only when `mode === "source"`**, `setChecked(!!getState().source)`, icon `lucide-code-2`), `find` Find (`lucide-file-search`), `find` Replace (`setDisabled("source" !== r)`, §6's "Replace disabled in preview" confirmed), `action` Export-to-PDF gated on `rd.canExportPdf`. The "add property" action opens the properties editor and calls `addProperty()` (`1961-Q3.js:1136-1148`).

---

## 21. CONFIRMED (unchanged in 1.13.7, remapped)

| Claim in `editor-markdown.md` | 1.13.7 evidence |
|---|---|
| `VIEW_TYPE === "markdown"` | `1961-Q3.js:867`, `:1240`; `getViewType()` `:960-962` |
| `MarkdownView extends TextFileView`, whose factory is `(leaf) => …` | `1961-Q3.js:1241`; `0025-expressionstatement.js:45,85` |
| `modes: Record<string, Mode>` keyed by `mode.type` | `1961-Q3.js:871-873`, `:1005-1006` |
| `onload` subscribes `quick-preview`, `resize`, `css-change`, `vault.config-changed` | `1961-Q3.js:945-948` |
| config-change acts on only `"spellcheck"` | `1961-Q3.js:1203-1204` |
| `onUnloadFile` saves folds, clears metadata editor, clears CSS class | `1961-Q3.js:999-1004` |
| `onClose` awaits super then `editMode.destroy()` | `1961-Q3.js:949-959` |
| `setMode` early-returns false when already current | `1961-Q3.js:1030` |
| `setMode` awaits `save()` only when leaving `"source"` | `1961-Q3.js:1030` |
| `setMode` order: save → capture+save folds → hide out → show/set/onResize in → scroll → apply folds → metadata collapse → buttons → `data-mode` | `1961-Q3.js:1030-1038` (metadata collapse test is `r.folds.some(e => 0 === e.from)`) |
| `getState()` = `{mode, source, backlinks, backlinkOpts?}` | `1961-Q3.js:964-966` |
| `setState` order: mode → `toggleSource` → backlinks; sets `eState.layout = true` on change | `1961-Q3.js:967-982` |
| `getEphemeralState` adds `scroll` | `1961-Q3.js:983-985` |
| `setEphemeralState` rename gate is `scroll < 0.5`, focus only in edit mode | `1961-Q3.js:986-991` |
| subpath → `{line, startLoc, endLoc}` via `resolveSubpath(metadataCache.getFileCache(file), subpath)` | `1961-Q3.js:993-997` — NEW: `endLoc` is `o.end || null`, not `o.end` |
| `getViewData()` = `currentMode.get()` | `1961-Q3.js:1042-1043` |
| `clear()` iterates all modes | `1961-Q3.js:1044-1049` — NEW: it also calls `onMarkdownFold()` when the current mode is the editor |
| `syncScroll()` captures scroll, triggers `markdown-scroll`, calls `syncState()` | `1961-Q3.js:1120-1122` — NEW: a `null` scroll does **not** suppress the trigger |
| `receiveSyncState` reopens the leaf for a different file, else applies ephemeral state | `1961-Q3.js:1123-1126` |
| `loadFrontmatter` diffs raw string, rejects arrays, `synchronize(parsed)` | `1961-Q3.js:1174-1186` — the throw is `new Error("Frontmatter must be a valid object!")`, caught into `i = null` |
| `onInternalDataChange` only acts when `this.data !== currentMode.get()` | `1961-Q3.js:1187-1189` |
| `onExternalDataChange` → `setData(data, false)` | `1961-Q3.js:1190-1191` |
| `setViewData(data, true)` nulls scroll, fans out to every mode, reloads folds, then always `loadFrontmatter` | `1961-Q3.js:1192-1202` |
| `triggerClickableToken`: 100 ms `setTimeout` → `openLinkText`; `window.open`; `#`-prefixed tag → `openGlobalSearch("tag:" + i)` | `1961-Q3.js:1222-1233` |
| clipboard routes to `metadataEditor` on focus or `obsidian/properties` MIME | `1961-Q3.js:1234-1240` |
| inline-title is `contentEditable` with focus/blur/input/paste/keydown handlers | `1961-Q3.js:874-882` |
| `MarkdownPreviewRenderer` is NOT a `Component`, owned per `MarkdownRenderer` | `1531-YW.js:13` |
| `MarkdownRenderer` makes a `docId = <16-char>` per instance | `1531-YW.js:6` |
| `onload` subscribes `vault.modify`/`vault.delete`/`metadataCache.changed` → `requestUpdateLinks`, 500 ms, per rendered section | `1531-YW.js:15-17`, `:6-11` |
| `onCheckboxClick` algorithm + the exact 6-group regex + `data-line` equality test | `1531-YW.js:18-40` — **NEW**: `navigator.vibrate(100)` is called unconditionally, not only on mobile |
| `toggleCheckbox(text, line)` → `{text, char} \| null`; `" "` → `"x"`, anything else → `" "` | `1531-YW.js:41-51` — the search is `/\[.\]/` **within the line only** (`am(e, t).start/.end`) |
| `postProcess(app, ctx)` delegates unloaded `.internal-embed` to the embed registry with `depth` and pushes `loadFile()` promises | `1531-YW.js:105-121` — NEW: `w && (t.addChild(w), i.push(w.loadFile()))`, so a registry miss is skipped silently |
| `render(...)` logs `console.error` with `plugin:<id>` stack matching when the Component is missing | `1531-YW.js:126-135` — exact text: `'Plugin "'.concat(s, '" is not passing Component in renderMarkdown. This is needed to avoid memory leaks when embedded contents register global event handlers.')`; the gate is `r instanceof MarkdownRenderChild` |
| `renderMarkdown(...)` calls `render(null, …)` | `1531-YW.js:123-124` |
| async parse iff `asyncParse && text.length >= 10240` | `1506-TW.js:217` (`!DW.asyncParse \|\| this.text.length < 10240 ? parseSync() : parseAsync()`), `:3-7` |
| worker protocol `{parseSections, options}` → `{sections, frontmatter}` | `1506-TW.js:147-150`; `worker/own/0169-expressionstatement.js:126` |
| recycle key is exact HTML-string equality; frontmatter diff via `JSON.stringify` | `1506-TW.js:164-195` |
| recycled pool gets prior `rendered && !used` sections; `asyncSections` filtered to survivors | `1506-TW.js:201-209` |
| `MarkdownPreviewSection` fields | `1503-EW.js:11` — exactly the doc's list, plus `used = !0` and `html = e \|\| ""` |
| `render()` sanitises, appends, adds `el-<tagname>` | `1503-EW.js:17-23` — NEW: `RF(html)` is called **even when** `rendered` was set true by the null-html header/footer constructor |
| `getSectionInfo` returns `{text, lineStart, lineEnd}` or `null` | `1506-TW.js:64-71` (see §12 for what `text` really is) |
| checkbox bridge adds `section.start.line` to `data-line`, guards `< 0` / `isNaN` | `1506-TW.js:93-103` |
| heading-collapse click: toggle, clear selection, `queueRender`, `owner.onFoldChange()` | `1506-TW.js:104-111` |
| `registerDomEvents` wires internal-link click/auxclick/dragstart/contextmenu/mouseover, footnote-link mouseover, external-link, tag, media | `1506-TW.js:768-813` (see §16 for deltas) |
| footnote mouseover synthesises `"#[^" + id + "]"` from `data-footref` | `1506-TW.js:796-798` |
| statics are `postProcessors: []`, `codeBlockPostProcessors: {}`, `recyclers: []` | `1506-TW.js:823` |
| `recyclers` signature `(sectionEl, recycledPool)`, called per freshly-rendered section | `1506-TW.js:223-225` — the pool is passed **by reference**, so a recycler may push into it |
| `addHeader`/`addFooter` create `new MW(null)` sections with `mod-header mod-ui` / `mod-footer mod-ui` and `queueRender()` | `1506-TW.js:701-708` |
| `Mod+A` in reading view shows a Notice; `Mod+C` with empty selection copies the source | `1533-QW.js:76-85` — exact i18n key `bd.interface.menu.msgMarkdownCopyInstructions({key: ay.Mod})` |
| `getFoldInfo().lines` mismatch silently drops folds | `1506-TW.js:495,499` (see §5) |

---

## 22. Unverifiable / not covered by this pass

- The **live-preview decoration engine** (`G3(this, cm)` at `1823-RJ.js:1180`, the `cm-embed-block` widgets, the properties widget `vO`) was **not** read. The 2026-06-10 addendum §3's "unaudited engine" caveat stands for decoration/widget internals; everything above is the view/mode/renderer layer.
- The **search widget** `Xq` (`1533-QW.js:171`) and the `selectRange` range-mapper `bm` (`1506-TW.js:681`) were not read; only their call sites and signatures.
- The **properties editor** `oN` (`app/own/1341-oN.js`, 28 KB) was not read beyond its constructor and its five `owner.saveFrontmatter` call sites. §13 Q7 is answered as to *location* (`oN`, constructed directly by `MarkdownView` at `1961-Q3.js:883`, not part of any internal plugin) but not as to behaviour.
- The **`lightbox`** `YL` and the **`ImageContextMenu`**/`ReaderMode` classes (`1939-m4.js`) were not read.
- The **`Publish`-site** subclass `K$` (`1796-K$.js`) was read only far enough to establish its `MarkdownPreviewRenderer` construction; its `_postProcess` link rewriting (`data-href` → `getPublicHref`) is out of scope.
- `ZT`/`Mw`/`OW`/`SW`/`xW`/`Tv`/`cI` were identified and cited by their observable effect only; their own internals beyond the quoted lines were not traced.
- §13 Q2 (public `render` param order), Q4 (`highlightRanges` writers), Q5 (embed-depth limit), Q6 (`foldManager` storage format) and Q8 (source-mode text right-click → `editor-menu` vs `markdown-viewport-menu`) remain open; Q8's likely answer is now visible at `1961-Q3.js:696-699` but is not claimed here.