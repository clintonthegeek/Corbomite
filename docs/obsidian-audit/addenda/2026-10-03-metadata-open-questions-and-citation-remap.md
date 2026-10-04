# §13 open questions Q1–Q10 closed against 1.13.7, plus a citation remap for every dead `MetadataCache.js:*` reference

**Date:** 2026-10-03
**Discovered during:** `metadata.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/metadata.md` §13 (all ten questions), §1 (the short-symbol
cross-reference in §15), and the dead `metadata/MetadataCache.js:*` line citations throughout
**Relevant cluster plans:** none yet — but see "Action taken" for the §15 problem

## Finding

### §13 Q1 — `NL(value)` widget inference: answered, and the answer contains `"unknown"`

The 1.13.7 counterpart is `UF` (`app/own/1297-UF.js:2-5`):

```js
function UF(e) {
  if (null == e || null === e) return "text";
  var t = typeof e;
  return "string" === t ? jF.test(e) ? "datetime" : WF.test(e) ? "date" : "text"
    : "number" === t ? "number" : "boolean" === t ? "checkbox"
    : ef(e, !0) ? "multitext" : "unknown";
}
```

with `WF = /^\d{4}-[01]\d-[0-3]\d$/` and `jF = /^\d{4}-[01]\d-[0-3]\dT[0-2]\d:[0-5]\d$/`
(`app/own/1296-BF.js:191-192`). This **corrects §2's `PropertyInfo.widget`** in two ways: the
documented vocabulary `"text" | "number" | "checkbox" | "date" | "datetime" | "tags" | "aliases" |
"multitext" | "list"` is wrong — `"unknown"` is reachable, and `"tags"`/`"aliases"` are **not**
produced by `UF` at all, they come from `metadataTypeManager.assignedWidgets` (`app/own/1298-_F.js:102-107`,
built-in defaults merged in from `HF` at `:107`). Also note the day/month ranges are loose (`[01]\d`
admits month 19), so `2024-19-39` classifies as a `date`. `"text"` only wins for `null`/`undefined`
and for unmatched strings; the final `?? "text"` in `getAllPropertyInfos` is a *separate* fallback for
entries whose `widget` is still `null`/`undefined` (`app/own/1306-eR.js:645-652`).

First-non-null-wins, not merge: `d.occurrences++, d.widget || null == h || (d.widget = UF(h))`
(`app/own/1306-eR.js:636`) — so the widget comes from the **first file** that has a non-null value for
that key, later files never change it. `occurrences` counts **files**, and `isUserIgnored` paths are
skipped (`:625`).

### §13 Q2 — `gT` position-prune semantics: it is a **versioned row migration**, not a prune

`gD` (`app/own/1110-gD.js:2-7`) is the whole thing, quoted in full in
`2026-10-03-metadata-persisted-shape-and-v-migration.md` §3. Short answer: it does not collapse or
pack positions; it (a) discards an array-shaped legacy `frontmatter`, (b) renames a legacy
`frontmatter.pos` up to `frontmatterPos`, (c) synthesises missing `frontmatterLinks` via `cD`, (d)
rewrites every `link` through `GE` (NBSP→space, trim, NFC), and (e) stamps `v = 1`. The actual
position reshaping is `hD(·, dD)` on write and `hD(·, vD)` on read, which flatten to and expand from
6-element arrays.

### §13 Q3 — the canvas/bases `linkUpdaters` protocol

Answered with the full four-method interface and the `update.reference` patch shape in
`2026-10-03-metadata-blockcache-linkupdaters-and-issupportedfile.md`.

### §13 Q4 — `frontmatterPosition.start.offset === 0`? **Yes, always** — and a BOM kills frontmatter

Answered in `2026-10-03-metadata-worker-cached-metadata-shape.md` ("ANSWER to §13 Q4").
Summary: the block rule is `onlyAtStart` (`worker/own/0157-expressionstatement.js:393`),
so `frontmatterPosition.start` is always `{line: 0, col: 0, offset: 0}`; a UTF-8 BOM or any leading
blank line prevents the `yaml` node from existing at all, which removes `frontmatter`,
`frontmatterPosition` **and** `frontmatterLinks`.

### §13 Q5 — footnote shape: `footnoteRefs` is **not** the `^1` refs array

`worker/own/0169-expressionstatement.js:41-53`. An `fnRef` goes to `footnotes` when
`r.data.contentPosition` exists, else to `footnoteRefs`; `footnoteDefinition` always goes to
`footnotes`; `id` may be `""`. Full quote in the worker-shape addendum, Correction 3.

### §13 Q6 — `getLinks()` and embeds

§2's union type is right, but the discriminator does not exist in the data: `getLinks` concatenates
`cache.links` then `cache.embeds` with **no tag** (`app/own/1306-eR.js:126`:
`r.links && (n = n.concat(r.links)), r.embeds && (n = n.concat(r.embeds))`). A Corbomite port that
splits the two tables must synthesise the discriminator itself — and it will get no help from the
source. New constraint discovered here: **`getLinks()` never includes canvas or base links**,
because it iterates `fileCache` and looks up `metadataCache[hash]`, which is `undefined` for
`hash: ""`.

### §13 Q7 — can a non-`.md` source appear in `resolvedLinks`? **Yes**

`resolveLinks` guards on `if (this.getCache(e))` (`app/own/1306-eR.js:399`), and `getCache` returns
`{}` — truthy — for any registered non-`.md` path (`app/own/1306-eR.js:133`). It then delegates to
`iterateRefsForFile`, which prefers `linkUpdaters[ext]` (`app/own/1306-eR.js:154`). Canvas text nodes
therefore appear as `resolvedLinks`/`unresolvedLinks` **sources**. Caveat discovered: canvas ref
discovery only walks top-level nodes (`app/own/2028-n9.js:426-428`).

### §13 Q8 — does `xT` walk `frontmatterLinks`? **Yes, and in that order**

```js
// app/own/1115-CD.js:2-4
function CD(e, t) {
  return !!e && (!0 === ED(e.frontmatterLinks, t) || !0 === ED(e.links, t) || !0 === ED(e.embeds, t));
}
```

So `frontmatterLinks` is visited **first**, then `links`, then `embeds`, short-circuiting on the first
literal `true`. §1's `iterateCacheRefs` claim ("does not include `tags`, `headings`,
`frontmatterLinks`, `listItems`, or `blocks`") is therefore **wrong about `frontmatterLinks`** —
§13 Q8's own speculation ("if `xT` walks only `links` + `embeds`, rename-refactor of a frontmatter
link would need `linkUpdaters["md"]`") is resolved in the other direction: markdown frontmatter links
are rewritten through the plain `vault.process` + `ZF(text, patches)` fallback
(`app/own/1306-eR.js:175-177`), because there is no `"md"` entry in `linkUpdaters` (the canvas plugin
registers `"canvas"` at `app/own/2028-n9.js:719-720`).

Also newly settled: the doc's §1 claim that only literal `true` short-circuits is **CONFIRMED**, and
the `=== true` test lives inside the walker, not at the call site — `ED`
(`app/own/1116-ED.js:2-8`): `if (!0 === t(i[n])) return !0;`.

### §13 Q9 — IndexedDB version history

Still unanswerable from this corpus (the audit of past schema versions is not in the bundle), but the
question is now moot for a different reason: there are exactly **four** `appId`-keyed IndexedDB
databases, none of which share a store with the metadata cache — see the confirmed table in
`2026-10-03-metadata-persisted-shape-and-v-migration.md` §8.

### §13 Q10 — `BlockCache` (`qL`): where it lives and how it persists

`QF`, `app/own/1302-QF.js:2-90`. **In-memory only**, keyed by `file.path`, invalidated by
`mtime` + `TFile` identity, rebuilt on demand from `vault.cachedRead` + a fresh markdown parse.
Not persisted, and not derived from `CachedMetadata`. Full description in
`2026-10-03-metadata-blockcache-linkupdaters-and-issupportedfile.md` Correction 3.

### Citation remap — every dead `metadata/MetadataCache.js:*` reference in the domain doc

The doc's source tree (`/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/metadata/`)
no longer exists. In 1.13.7 the class lives in `app/own/1306-eR.js` (680 lines) and its helpers are
one-function-per-statement files. Remap, in doc order:

| `metadata.md` reference | 1.13.7 location |
|---|---|
| `MetadataCache.js:6-44` (constructor) | `app/own/1306-eR.js:2-10` |
| `MetadataCache.js:41` / `:42` (self `finished`/`resolved` subs) | `app/own/1306-eR.js:9` |
| `MetadataCache.js:1087-1094` (`on`/`trigger` passthroughs) | `app/own/1306-eR.js:674-679` |
| `MetadataCache.js:1097` (class close) | `app/own/1306-eR.js:680` — `}(FD);` |
| `MetadataCache.js:18` (`didFinish` debounce) | `app/own/1306-eR.js:5-7` |
| `MetadataCache.js:242-248`, `:264-268`, `:236-240` (`linkUpdaters` reads) | `app/own/1306-eR.js:150-152`, `:153-159`, `:166-171` |
| `MetadataCache.js:340-344` (`watchVaultChanges`) | `app/own/1306-eR.js:218-220` |
| `MetadataCache.js:368-401` (`_preload`) | `app/own/1306-eR.js:223-279` |
| `MetadataCache.js:516` (`finished` at end of `initialize`) | `app/own/1306-eR.js:301` |
| `MetadataCache.js:587` (`resolved` from `onStop`) | `app/own/1306-eR.js:351-358` |
| `MetadataCache.js:609` (`resolve`) | `app/own/1306-eR.js:370` |
| `MetadataCache.js:780`, `:805` (`changed`) | `app/own/1306-eR.js:477`, `:488` |
| `MetadataCache.js:847` (`deleted`) | `app/own/1306-eR.js:511` |
| `MetadataCache.js:868-869` (`onRename` map remap) | `app/own/1306-eR.js:522` |
| `MetadataCache.js:887-902` (`saveMetaCache`) | `app/own/1306-eR.js:529-536` |
| `MetadataCache.js:905-906` (`work` sequentiality assert) | `app/own/1306-eR.js:539` |
| `MetadataCache.js:646-662` (`resolveLinks`, cited by the 2026-06-10 addendum) | `app/own/1306-eR.js:397-412` |
| `getAllTags.js:5-21` | `app/own/1117-MD.js:2-18` |
| `iterateRefs.js:5-11` | `app/own/1116-ED.js:2-8` |
| `iterateCacheRefs.js:5-10` | `app/own/1115-CD.js:2-4` |

### §15 short-symbol cross-reference — every name is stale

§15 lists 30 short symbols (`xT`, `UL`, `qL`, `NL`, `BL`, `VL`, `vT`, `wT`, `kT`, `mT`, `gT`, `yc`,
`sc`, `vb`, `wb`, `Cb`, `Zw`, `GL`, `Sf`, `kf`, `oc`, `nu`, `au`, `su`, `iu`). **None of them exists
in 1.13.7.** Mapping:

| §15 name | 1.13.7 | File |
|---|---|---|
| `xT` (ref walker) | `CD` | `app/own/1115-CD.js:2` |
| `iterateRefs` | `ED` | `app/own/1116-ED.js:2` |
| `UL` (text mutator) | `ZF` | `app/own/1304-ZF.js:2` |
| `qL` (`BlockCache`) | `QF` | `app/own/1302-QF.js:2` |
| `NL` (widget inference) | `UF` | `app/own/1297-UF.js:2` |
| `BL` (unresolvedLinks key) | `KF` | `app/own/1299-KF.js:2` |
| `getLinkpath` | `bD` | `app/own/1112-bD.js:2` |
| `VL` (path comparator) | `GF` | `app/own/1300-GF.js:2` |
| `vT` / `mT` / `gT` (persist/hydrate/prune) | `rf`/`pD`/`hD`/`dD` (persist) and `mD`/`fD`/`vD` (hydrate) and `gD` (migrate) | `app/own/0569-rf.js`, `1105-pD.js`, `1104-hD.js`, `1106-dD.js`, `1109-mD.js`, `1107-fD.js`, `1108-vD.js`, `1110-gD.js` |
| `sc` (multi-map) | `_d` | `app/own/0556-_d.js:2` |
| `vb` (serial queue) | `Iw` | `app/own/0821-Pw.js:38` |
| `wb` (generator queue) | `Rw` (+ `Fw` runnable, `Pw` FIFO) | `app/own/0824-Fw.js:25-71`, `app/own/0821-Pw.js:2-36` |
| `Cb` (runnable) | `Fw` | `app/own/0824-Fw.js:3-22` |
| `Zw` (idb-open) | `Xk` | `app/own/0875-Xk.js:2` |
| `GL` (chunked cursor) | `tR` | `app/own/1307-tR.js:2` |
| `Sf`/`kf` (decode / hash) | `Sd` / `OC`(+`FC`,`Pd`) | `app/own/0538-Sd.js:2`, `app/own/0904-OC.js:2` |
| `nu`/`au`/`su`/`iu` (path+linktext normalisers) | `Pl` (dirname), `Al` (basename), `Ol` (strip-extension), `Fl` (extension), `bD` (getLinkpath) | `app/own/0361-Pl.js:2`, `0360-Al.js:2`, `0364-Ol.js:2`, `0365-Fl.js:2`, `1112-bD.js:2` |
| `parseFrontMatterTags` | `ZT` | `app/own/1092-ZT.js:2` |
| `parseFrontMatterAliases` | `$T` | `app/own/1091-$T.js:2` |
| `Events` | `FD` | `app/own/1306-eR.js:680` |
| (unlisted) debounce | `Ml` | `app/own/0355-Ml.js:2` |
| (unlisted) frontmatter-link extractor | `cD` | `app/own/1102-cD.js:2` |
| (unlisted) linktext rewriter (migration only) | `GE` | `app/own/0933-GE.js:2` |
| (unlisted) `updateRelatedLinks` matcher | `YF` | `app/own/1300-GF.js:5` |
| (unlisted) tag validator | `uO` | `app/own/1255-uO.js:2` |
| (unlisted) property-value validator | `XF` | `app/own/1301-XF.js:2` |
| (unlisted) frontmatter scalar reader | `QT` | `app/own/1090-QT.js:2` |
| (unlisted) worker entry | `self.onmessage` | `worker/own/0169-expressionstatement.js:2` |
| (unlisted) main-thread parser twin | `uD` | `app/own/1103-uD.js:2` |
| (unlisted) UTF-8 encode for the worker | `Md` | `app/own/0537-Md.js:2` |
| (unlisted) markdown parse (app / worker) | `WT` / `hr` | `app/own/1082-WT.js:2`, `worker/own/0156-hr.js:2` |
| (unlisted) position (de)serialisers | `aD`/`oD` (app), `vr`/`yr` (worker) | `app/own/1100-aD.js`, `1099-oD.js`, `worker/own/0164-vr.js`, `0163-yr.js` |

§15's *functional* descriptions also need two fixes: `Al` is **basename** and `Pl` is **dirname**
(§15 files them all under "path + linktext normalisers (strip heading, lowercase, extname, dirname)"
with no assignment), and `oc`/`jd` (`escapeRegExp`) is used by `updateUserIgnoreFilters`
(`app/own/1306-eR.js:588`).

## Why noticed now

Two reasons. First, `metadata.md` §13 is the doc's own list of "we did not read this", and with the
worker bundle plus the app-side helper statements restored, eight of the ten questions are now
one-file answers — leaving them open would mean re-deriving them later. Second, the 2026-06-10
addenda had to be re-verified for this domain (its §2 and §5 both touch `metadata.md`), and doing so
surfaced that **every** citation in the domain doc and **every** short symbol in §15 is dead — the
same problem the 2026-10-03 vault pass hit, where the fix was an explicit remap table
(`2026-10-03-vault-prior-corrections-re-verified.md`).

## Action taken

No Corbomite code touched. §13 is now fully answered except Q9, which is unanswerable from this
corpus and immaterial. The remap tables above are what an implementer should quote; the domain doc's
`metadata/MetadataCache.js:*` references and §15 symbol names should be treated as historical, in the
same way the 2026-10-03 vault addenda treat `vault.md`'s `Vault.js:*` references. If a plan ever
cites `metadata.md` for a line number, it should cite these addenda instead.
