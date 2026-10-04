# `changed` fires with the file **contents** as its second argument, not the old hash — and `resolve`/`finished` fire per queue entry, not per file

**Date:** 2026-10-03
**Discovered during:** `metadata.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/metadata.md` §1 (`didFinish`), §4 (all five event rows + the
"Payload ordering guarantee" paragraph), §8 (`changed`/`resolve` invariants)
**Relevant cluster plans:** none yet — `metadata.md` §11 already lists the five-event gap as
unimplemented in `libs/storage/SQLiteIndex`; this addendum fixes the *shape* any Qt-signal port
must emit.

## Finding

### 1. `changed` payload is `(file: TFile, contents: string, cache: CachedMetadata)` — **not** `(file, oldHash, cache)`

`metadata.md` §4's table row for `changed` reads
`| changed | (file: TFile, oldHash: string, cache: CachedMetadata) |`. The second argument is the
**decoded file text**. Both emit sites in `app/own/1306-eR.js` pass the same local `o`, which is
assigned from a `TextDecoder` decode of the bytes just read out of the vault, *before* the async
hash resolves:

```js
// app/own/1306-eR.js:473-477
//   case 1:  return [4, this.vault.readBinary(e)];
//   case 1:  t = c.sent(), o = Sd(t), [4, OC(t)];
//   case 2:  if (a = c.sent(), r.mtime = n.mtime, r.size = n.size, r.hash = a, this.saveFileCache(i, r),
//                    s = this.metadataCache[a])
//              return this.queueFileForLinkResolution(e), this.trigger("changed", e, o, s), [2];
```

```js
// app/own/0538-Sd.js:2-4
function Sd(e) {
  return new TextDecoder().decode(new Uint8Array(e));
}
```

An in-app consumer proves it beyond doubt by comparing that argument against the view's own
loaded body — a hash can never equal file text:

```js
// app/own/1844-R1.js:322-324
t.prototype.onFileChanged = function(e, t, n) {
  e !== this.file || t === this.data || this.dirty || this.loadFileInternal(t, n);
},
```

`loadFileInternal(t, n)` takes `(contents, cache)`. A second consumer uses the third argument as
the cache: `app/own/2039-vee.js:575-581` (`onCacheChanged = function(e, t, n) { … Object.keys(n.frontmatter …) }`).

Corbomite note: this means the `changed` payload is a **full copy of the file text on every
modify**. Any port must not confuse it with a hash, and must not use it as a change token.

### 2. `queueFileForLinkResolution` runs *before* `trigger("changed")` — §4's "Payload ordering guarantee" has the mechanism backwards

§4 states: *"`changed` is emitted inside `computeFileMetadataAsync`'s worker-complete continuation,
**before** `queueFileForLinkResolution` pushes the file onto the resolver queue."* Both branches do
the opposite — `queueFileForLinkResolution(e)` comes first, then the trigger (`app/own/1306-eR.js:477`
and `app/own/1306-eR.js:488`).

The doc's *observable* ordering (`changed` → `resolve` → `resolved` → `finished`) still holds,
because the queue is asynchronous anyway: `Rw.add` resolves the parked generator promise through
`setTimeout(function() { e && e.resolve(); }, 0)` (`app/own/0824-Fw.js:36-42`), so the drain always
lands in a later macrotask. Fix the mechanism, keep the ordering.

### 3. `resolve` fires once per **queue entry**, and the queue does not de-duplicate

§4's `resolve` row says it "Fires **once per file** during a drain". There is no de-dup:
`Rw.add` → `Pw.enqueue` → `this.queue.push(e)` (`app/own/0824-Fw.js:29-31`,
`app/own/0821-Pw.js:17-19`). A file queued N times runs `resolveLinks` and `trigger("resolve", file)`
N times, and `resolveLinks` re-assigns `resolvedLinks[path]`/`unresolvedLinks[path]` wholesale each
time (`app/own/1306-eR.js:411`). Bursty edits therefore produce repeated `resolve` events for one
file. Plugins that treat `resolve` as an edge must latch on the file, not on the event.

Related: `deletePath` and `onRename` both enqueue a literal **`null`** as a wake-up
(`app/own/1306-eR.js:513`, `app/own/1306-eR.js:525`); the drain loop skips null entries but still
spins (`app/own/1306-eR.js:369-370`: `!(r = c)) return [3, 3]`).

### 4. `didFinish` is a **trailing** 10 ms debounce, not "leading-edge"

§1 says `didFinish = debounce(() => trigger("finished"), 10, leading=true)`. The generic debouncer
is `Ml(fn, wait, immediate)` (`app/own/0355-Ml.js:2-33`); with the third argument truthy it takes
the `n ? a = s = p + t : …` branch (`app/own/0355-Ml.js:26`), which **re-arms the deadline to
`now + wait` on every call** and only fires from the timer (`u`). The first call never fires
synchronously. So `Ml(fn, 10, true)` means "coalesce the burst, fire once 10 ms after the *last*
call" — the opposite edge from the one §1 names. `false` would have been "fire 10 ms after the
*first* call of a burst" (plain throttle).

`didFinish` has exactly two call sites, both guarded on the counter hitting zero:
`app/own/1306-eR.js:497` (`computeFileMetadataAsync` completion) and `app/own/1306-eR.js:513`
(`deletePath`).

### 5. `deleted` has no in-app consumer at all

A corpus-wide search for `.on("deleted"` on `metadataCache` returns nothing; the only `deleted`
subscribers are unrelated components (`app/own/1528-jW.js:9` is that component's *own* event, and
`app/own/2037-j9.js` subscribes to `vault.on("delete")` instead, at `app/own/2037-j9.js:1134`). The
event exists purely as a plugin surface — §4's "typical consumers: graph/backlinks plugins pruning
references" is speculation with no in-tree example.

### 6. CONFIRMED — event names, payloads and the clean-cache latch

| Claim in `metadata.md` | Live location | Verdict |
|---|---|---|
| Event names `changed`/`deleted`/`resolve`/`resolved`/`finished` | emit sites `app/own/1306-eR.js:477`, `:488`, `:511`, `:370`, `:353`; all five consumed at `app/own/1298-_F.js:53`, `app/own/1961-Q3.js:42`, `app/own/1961-Q3.js:218`, `app/own/1848-Y1.js:871`, `app/own/2037-j9.js:1134` | **CONFIRMED** |
| `resolve` payload is a `TFile` | `app/own/2037-j9.js:87` (`e === this.file`), `app/own/2037-j9.js:1178-1180` | **CONFIRMED** |
| `deleted` payload is the last cache snapshot, read before the row is cleared | `app/own/1306-eR.js:511` — `this.trigger("deleted", e, this.getFileCache(e)), this.deletePath(e.path)` | **CONFIRMED**, with the caveat that `getFileCache` returns `{}` (not `null`) for non-`.md` files — see `2026-10-03-metadata-blockcache-linkupdaters-and-issupportedfile.md` |
| `resolved` is emitted by the queue's `onStop` | `app/own/1306-eR.js:351-358` — `new Rw({ onStop: function() { return e.trigger("resolved"); }, onCancel: … })`, reached from `Rw.generator()`'s `else { e.stop(); await … }` at `app/own/0824-Fw.js:57-62` | **CONFIRMED** |
| `resolved` is "the queue has drained" | `app/own/0824-Fw.js:55-62` — `stop()`/`onStop` fires only on the empty-queue branch | **CONFIRMED** |
| `finished` has two sources: debounced, plus one explicit at end of `initialize()` | `app/own/1306-eR.js:497`, `:513`, and `app/own/1306-eR.js:301` (`this.trigger("finished")`) | **CONFIRMED** |
| The two are indistinguishable | both funnel into the same `trigger("finished")`; no flag argument | **CONFIRMED** |
| `isCacheClean()` is exactly `inProgressTaskCount === 0 && items.length === 0 && !runnable.isRunning()` | `app/own/1306-eR.js:425` — `return 0 === this.inProgressTaskCount && 0 === this.linkResolverQueue.items.length && !this.linkResolverQueue.runnable.isRunning();` | **CONFIRMED, verbatim** |
| `onCleanCache(fn)` fires immediately if clean, else from a `finished`/`resolved` edge | `app/own/1306-eR.js:413-423`, self-subscribed at `app/own/1306-eR.js:9` (`i.on("finished", i.checkCleanCache, i), i.on("resolved", i.checkCleanCache, i)`) | **CONFIRMED** |
| `onCleanCache` callback exceptions are swallowed and logged | `app/own/1306-eR.js:418-422` — `try { e(); } catch (e) { console.error(e); }` | **CONFIRMED** (undocumented) |
| Indexing notice polls every 300 ms | `app/own/1306-eR.js:340-343` — `window.setInterval(function() { … i.value = Math.max(0, a - n.inProgressTaskCount); … }, 300)` | **CONFIRMED** |

## Why noticed now

The 1.13.7 corpus was restored, which made it possible to (a) read the worker **and** the main
thread parser, and (b) trace real in-app consumers of each event. `metadata.md` §4 was written
entirely from the emitter side with payloads marked "(inferred)". Reading
`app/own/1844-R1.js:322` — the one line in the app that destructures the `changed` payload and
compares argument 2 to `this.data` — settles the payload question that §4 left open, and it settles
it *against* the doc.

## Action taken

No Corbomite code touched: `metadata.md` §11 already records that `SQLiteIndex` exposes a single
`indexReady` signal and none of the five events, so there is no implementation to correct. This
addendum exists so that when the five events *are* ported, the port carries
`(file, contents, cache)` for `changed`, tolerates repeated `resolve` per file, and treats
`finished` as trailing. No plan cites it yet.
