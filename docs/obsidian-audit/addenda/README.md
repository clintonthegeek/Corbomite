# Audit addenda

> **Append-only directory.** When implementation reveals a fact about Obsidian that the audit didn't capture — or contradicts what the audit said — write a new file here rather than editing the original audit doc. The 15 Pass 2 domain docs and 5 Pass 3 synthesis docs are *snapshots* of what we believed at audit time; they cost ~25 hours of agent compute to produce and must remain stable as historical record.

## Corrections

The 2026-06-10 batch below is the first set of addenda that **refute** main-doc claims (all prior addenda were additive — new coverage, not contradictions). Per the frozen-doc rule the main docs remain unedited, so the wrong text is still sitting there looking authoritative. **Check this directory before implementing from:** `vault.md` §1/§8, `metadata.md` §2, `parsing.md` §1/§3, `workspace.md` §2, `views.md` ViewState, `editor.md` expandText/`lP`, `rendering.md` §8 invariants 5/11/12 and §11 (DOMPurify config, `data-href`, `NaN`, tooltip delay), `00-taxonomy.md` QueryController, and the corresponding `VAULT-FORMAT.md` sections (in particular §4.1's offsets, §4.2's CRLF rule, §4.4's wikilink grammar and §9's NFC placement).

### 2026-10-03 batch — all 16 domains verified against restored 1.13.7 source

The prior audit's corpus was destroyed, so every symbol citation in the 15 domain
docs was unverifiable. Obsidian 1.13.7 was re-extracted into a readable, indexed,
partially-renamed corpus (`RE/extract-obsidian.sh`, reproducible from the `.deb`)
and **all 16 domains** were re-read against it: **44 addenda, ~14.6k lines**.

**Check this section before implementing from any domain doc.** These are the
findings most likely to change code you are about to write:

| Area | Finding | Addendum |
|---|---|---|
| **Sanitiser** | DOMPurify runs with **`ALLOW_UNKNOWN_PROTOCOLS: true`**, so `javascript:` hrefs survive. `rendering.md` §11 says Corbomite "must match this config exactly" — **that is a decision, not an instruction to copy** | `2026-10-03-rendering-sanitiser-policy.md` |
| **Settings files** | `app.json`/`appearance.json` split on a **16-key** allow-list, not the 6 the doc lists; 10 keys move file; plus a whole omitted class `.obsidian/<corePluginId>.json` | `2026-10-03-vault-config-split-and-defaults.md`, `…-settings-config-file-ownership-and-definitions.md` |
| **Hotkeys** | `hotkeys.json` is override-only: `"cmd": []` **unbinds**. **115** command ids, not ~30. A `code` binding is matched as a **name**, not a physical position — and §D's `["Mod"]` + `code:"KeyI"` example does not fire (needs `["Mod","Alt"]`) | `2026-10-03-core-command-and-hotkey-contracts.md`, `…-hotkeys-json-dispatcher.md` |
| **Timings** | `debounce` (`Ml`) is a **leading-edge throttle**, not a trailing debounce — affects every timing claim in the audit | `2026-10-03-editor-wrapper-commands-and-keymaps.md` |
| **File save** | No three-way merge; a mid-paragraph user edit can be **discarded and relocated** | `2026-10-03-views-leaf-viewstate-and-deferred-restore.md` |
| **`.canvas`** | Unknown node `type`s and endpoint-dangling edges are **deleted on the next save** | `2026-10-03-canvas-disk-contract-and-viewport-settings.md` |
| **`.base`** | The writer canonicalises property IDs in `order`/`sort`/`groupBy` but **not** in `properties:`/`data.columnSize` — one file, two conventions. `not:` is **NOR** | `2026-10-03-bases-base-file-serialization-and-dsl-corrections.md` |
| **Frontmatter** | **Two scanners that disagree**: the writer accepts CRLF, the indexer rejects it, so a CRLF note's frontmatter is invisible to `CachedMetadata` | `2026-10-03-parsing-frontmatter-info-offsets-and-parser-divergence.md` |
| **Metadata event** | `changed`'s 2nd argument is the **file contents**, not `oldHash` — refutes the §4 signature table | `2026-10-03-metadata-changed-event-payload.md` |
| **Rename/links** | Heading rename matches with `stripHeading` but writes with `stripHeadingForLink`, and **rewrites YAML frontmatter**, re-dumping it through js-yaml | `2026-10-03-leaf-utilities-subpath-rename-and-leaf-bundle-corrections.md` |
| **Search DSL** | The grammar is **in-tree and readable**; `whole-word` does not exist; `ignore-case` does | `2026-10-03-search-dsl-grammar-and-quick-switcher-scoring.md` |
| **Workspaces** | There is **no `workspaces.json`** — named workspaces are the plugin's `data.json` | `2026-10-03-workspace-named-workspaces-data-json.md` |

**One of our own addenda was wrong.** `2026-10-01-workspace-split-direction-correction.md`
says it "REFUTES" `workspace.md:79`, then its own replacement table restates line 79
verbatim. Six independent source paths confirm **the doc was right**; the inversion was
in our serializer, not the spec. See
`2026-10-03-workspace-split-direction-source-proof.md` — a live reminder that an
addendum refuting a frozen doc needs source proof, not one round-trip.

### 2026-10-03 — canvas domain, verified against the restored 1.13.7 corpus

The domain doc's symbol→role map is **actively misleading** in the split corpus: `dc`/`fc` are tree-sitter, `V5`/`H5`/`j5` are Bases, `A3` is an embed widget. Every `app.pretty.js:*` citation is dead. A replacement role map for 1.13.7 is in the addendum. On the HIGH-severity disk contract: the serializer is **`$d`/`Zd`** (tab indent, all-primitive arrays/objects collapse to one compact line, `undefined` keys dropped, no space after colon, no trailing newline) — **not** `dc`/`fc`, so §3 item 6 and §13 Q4 name the wrong function and byte parity is a ~35-line port. **`nodes` is re-sorted by `zIndex` on every save** (edges are not) and file order *is* z-order, which round-trips via a running-max trick in `importData`. **Three silent-loss paths**: unknown node `type`s are `continue`d away, edges with unresolvable endpoints are dropped — both destroyed on the next save — while **explicit defaults** (`fromEnd:"none"`, `toEnd:"arrow"`, `color:""`, `backgroundStyle:"cover"`) are *preserved* via `unknownData`, and `text`/`file`/`url` are *always* written. Plus the complete `canvas/data.json` schema (8 keys), the whole align/arrange/distribute/justify family (§9.5 has none), every viewport/snap/gesture constant with its thresholds, the link index's 5000-unit parse threshold, export bounds padding **100** not 20 and a hard **2×** scale ceiling, and §13 Q2/Q3 answered (`mW` = YouTube + Twitter only; `backgroundStyle` closed at three).

- [`2026-10-03-canvas-disk-contract-and-viewport-settings.md`](2026-10-03-canvas-disk-contract-and-viewport-settings.md) — **do not build `.canvas` I/O from `canvas.md` §3 or §11.1**: §3 names the wrong serializer and misses the node-array re-sort; §11.1's four "priority interop fixes" and its bezier/containment criticisms are **already done** in `libs/canvas`, while its *actual* remaining deltas are three concrete `CanvasDocument` bugs (explicit defaults destroyed, `text`/`file`/`url` conditionally written, unknown node types silently coerced to `type:"text"`).

### 2026-10-03 — rendering domain, verified against the restored 1.13.7 corpus

Eight addenda. The headline: **the sanitiser config is five keys long and turns the URI-scheme check
off** (`ALLOW_UNKNOWN_PROTOCOLS: true`), so `javascript:` hrefs are not removed by DOMPur —
and it is called `FF`, not the `SL` that `rendering.md` §13 spent an open question hunting.
Second: **Obsidian's own rendered markdown is passed through that sanitiser**, because
mdast→hast→html runs with `allowDangerousHtml: true` (`GT`) before `RF()` — sanitisation is
on the main render path, not a plugin-HTML concern as §10/§11 read. Third: the mermaid
pipeline is behind a **per-vault trust gate** whose Allow button fires `post-processor-change`,
and math is a `.math:not(.is-loaded)` post-processor whose 100 ms batch **never calls
`typeset`** — it swaps `MathJax.chtmlStylesheet()` and toggles `data-change` between `"1"` and
`"2"`. Also: `htmlToMarkdown`'s output escaping is **switched off** (`MO.escape = e => e`),
`§13 Q1/Q2` are both answered, `NumberValue` renders `NaN` as `"NaN"` (only ±Infinity is `"∞"`),
`data-href` is the resolved path for `TFile` targets, tooltips are a delegated
`document.body` system with a **1000 ms default hover delay**, and reading view sets `dir`
**per table cell** from a non-UBA first-strong regex — so §11's "Qt is per-paragraph only"
mislocates the gap. This domain does **not** share `editor.md`'s `_internal.js` coverage
defect (no `_internal.js` exists in 1.13.7); its defect is that §13 filed two extractable
facts as un-extractable.

- [`2026-10-03-rendering-sanitiser-policy.md`](2026-10-03-rendering-sanitiser-policy.md) — the whole DOMPurify config verbatim (`ALLOW_UNKNOWN_PROTOCOLS`/`RETURN_DOM_FRAGMENT`/`FORBID_TAGS`/`ADD_TAGS`/`ADD_ATTR`); **no `ALLOWED_TAGS`/`ALLOWED_ATTR`, so stock 3.0.1 defaults apply**; DOMPurify is the vendored global `window.DOMPurify` v3.0.1; three `afterSanitizeAttributes` hooks with two *different* iframe-sandbox allowlists and an internal-link `nofollow` exemption; `OF()` drops unknown `sandbox`/`allow` tokens. **§13 Q1 answered**; the doc's `EL`/`SL` names are a Levenshtein helper and the CLI registry.
- [`2026-10-03-rendering-html-to-markdown-turndown.md`](2026-10-03-rendering-html-to-markdown-turndown.md) — the service is `MO`, not `hP`, and Turndown is a global; all six options plus **nine custom rules** verbatim, including `highlight` → `==…==` (Obsidian-only) and `highlightedCodeBlock` (`div.highlight-(text|source)-<lang> > pre` → fenced); `==`-aligned table separators with colspan-aware padding; **output escaping is disabled** (`MO.escape = function(e) { return e; }`), refuting §8 invariant 11. **§13 Q2 answered**.
- [`2026-10-03-rendering-math-pipeline.md`](2026-10-03-rendering-math-pipeline.md) — math is a `.math:not(.is-loaded)` post-processor; display mode is read off the **`math-block` class**; only `&amp; &lt; &gt; &quot;` are entity-decoded; **invalid LaTeX falls back to the literal source text**, not an error span (§9 wrong); the 100 ms batch calls `MathJax.chtmlStylesheet()` and **toggles `data-change` `"1"`↔`"2"`** rather than typesetting (`startup.typeset: !1`); full MathJax config incl. empty delimiter lists and `safeProtocols`; recycling **transplants** loaded math nodes.
- [`2026-10-03-rendering-mermaid-pipeline.md`](2026-10-03-rendering-mermaid-pipeline.md) — the **`mermaid-vault-trust` gate** with its exact guarded DOM and `post-processor-change` re-render on Allow; post-`render` DOM surgery (**marker-ID namespacing**, `<a>`-href re-arming, foreignObject→`<a class="internal-link">`); per-diagram-type `useMaxWidth` table and Monday-only gantt axis; lazy `onNodeInserted` rendering; **errors are plain text**, not an error pane (§9 wrong); the documented `break-spaces` local patch **verified applied**, and mermaid **11.13.0** confirmed.
- [`2026-10-03-rendering-postprocessor-recycler-registries.md`](2026-10-03-rendering-postprocessor-recycler-registries.md) — the undocumented **`registerRecycler`** registry; `codeBlockPostProcessors` is a **plain object, not a `Map`**, and double registration **throws**; code-block registration is **dual** (wrapper into `postProcessors[]` for reading view, raw fn into the map for live preview) and the two verbs **return different things**; `ctx.replaceCode` + `ctx.promises`; the `gm()` loader verbatim (bundled assets, retryable on failure); `Prism.manual = true`; the fixed PDF.js viewer options and `pdfjs-is-themed`; `registerDomEvents` (`data-href || href`); `.is-flashing` for 3 s.
- [`2026-10-03-rendering-callout-and-bidi-dom.md`](2026-10-03-rendering-callout-and-bidi-dom.md) — the callout DOM verbatim: 4 `data-callout-*` attributes, **`--callout-color` only for 6-digit hex** (3-digit expanded first), `callout-icon` before `callout-title-inner`; the fold post-processor (`+`/`-` exact, `is-collapsed` on two elements, a `setTimeout(…,0)` + `defaultPrevented` re-check so links in a title win the click race); three callout-icon forms incl. **sanitised inline SVG**; **reading view sets `dir` per table cell** from `rm`, a non-UBA first-strong regex (digit/bracket-prefixed cells return `auto`), refuting §11's bidi row; the doc's `mW` bidi table is **dead code** (live twin is `Rp`).
- [`2026-10-03-rendering-tooltip-and-match-highlight.md`](2026-10-03-rendering-tooltip-and-match-highlight.md) — tooltips are one **delegated `document.body`** listener pair on `[aria-label]` with a **1000 ms default hover delay** (`Ng`), a 100 ms sweep cooldown (`Bg`) whose mechanism is now explained, a global `pointerup` dismiss and a `--no-tooltip` opt-out; `mod-bottom` never exists; the synthetic-event filter `jv`; `renderResults` is `lb` and the class is exactly **`suggestion-highlight`** (with its break/clamp edge cases). **§13 Q5 answered**.
- [`2026-10-03-rendering-rendercontext-value-and-symbol-map.md`](2026-10-03-rendering-rendercontext-value-and-symbol-map.md) — **`data-href` is the resolved path for `TFile` targets**, not always the linktext; the label fallback is string-vs-`TFile`, with no "raw linktext for unresolved" branch; `nE`=**`QE`** is a `>`-breadcrumb joiner, `Ic`=**`Av`** suppresses the middle-click default, `Lc`=**`Dv`** is an enters-element test (**not** a modifier predicate — that lives in Page Preview's `defaultMod`), `Yc`=**`$v`** is `new URL()` parseability (**not** a scheme allowlist); **`NumberValue` renders `NaN` as `"NaN"`**; the image dimension-spec carve-out + extension-family table; `isTruthy()` on the whole `Value` tower; `ListValue.get()` mutates its own array. **§13 Q3/Q4/Q6/Q7 answered**; `"bases"`/`"graph"`/`"bookmarks"` hover sources sourced.
- [`2026-10-03-rendering-citation-remap-and-coverage.md`](2026-10-03-rendering-citation-remap-and-coverage.md) — coverage verdict (**no `_internal.js` in 1.13.7**, so `editor.md`'s defect does not transfer), a remap of every load-bearing dead citation, the real six-step markdown→HTML chain ending in `RF()` with `allowDangerousHtml: true`, the "you forgot the Component" stack-sniffing guard, the **`ctx.replace`/`getSectionInfo` stubs** in the one-shot `renderMarkdown` path, and §3's "No on-disk contracts" **refuted** by three `localStorage` keys.

**Read the sanitiser one first** — `rendering.md` §13 filed the DOMPurify config as
un-extractable and §11 tells Corbomite to match "this config exactly", but the config turns
off the URI-scheme check, and it also sits on the *main* render path (not just plugin HTML).

### 2026-10-03 — parsing domain, verified against the restored 1.13.7 corpus

Five addenda. The headline: **Obsidian has two frontmatter scanners and they disagree** — the regex
pair `parsing.md` documents is only the *writer*; the *indexer* uses a hand-rolled block rule that
rejects CRLF and accepts fences the writer rejects. Second: **every link target is NFC-normalised at
parse time and never again**, so the fold lives in the parser, not the vault API `VAULT-FORMAT.md`
§9 credits. Third: the wikilink grammar §14 nominated for promotion to `VAULT-FORMAT.md` has five
pre-normalisation steps in front of the documented `path`/`subpath` split, and its default display
text is a `>`-joined breadcrumb that lands in `CachedMetadata`. Also: `VAULT-FORMAT.md` §4.1's
"**P0 correctness bug in Corbomite**" does not exist, and `parsing.md` never covered footnotes,
tags, or block ids at all.

- [`2026-10-03-parsing-frontmatter-info-offsets-and-parser-divergence.md`](2026-10-03-parsing-frontmatter-info-offsets-and-parser-divergence.md) — §1/§2's four "byte offset"s are **UTF-16 code units**; §3's "CRLF-original files get LF delimiters after write-back" is **wrong** (delimiters sit outside the `[from, to)` splice and survive verbatim — only the payload goes LF); the CRLF and trailing-fence **disagreement table** between `_T` and the AST block rule, with executed evidence; `parseFrontMatterAliases` returns `[]` not `null`; an undocumented fifth helper `cssclasses`; `IA` preserves the frontmatter key's spelling. Answers §13 Q3/Q4/Q6; re-verifies 2026-06-10 §5; closes the bogus P0.
- [`2026-10-03-parsing-wikilink-grammar-nfc-and-breadcrumb.md`](2026-10-03-parsing-wikilink-grammar-nfc-and-breadcrumb.md) — the wikilink parser is `$E` → `{href, title, isAlias}`, **not** the `wD` `{path, subpath}` splitter §1 documents (so §1's "`|` is stripped upstream" hides five behaviours); **NFC + NBSP fold at five parse sites, and `bD`/`KF`/`getLinkpathDest` never fold**; `displayText` is the `>`-breadcrumb for `[[Note#H1#H2]]` and is written unconditionally; `[[|foo]]` → target `|foo`, `[[a|b|c]]` → display `b|c`, `[[Note\]]` strips one backslash from the href only; an interior containing `[[` is not a link.
- [`2026-10-03-parsing-tag-and-blockid-grammar.md`](2026-10-03-parsing-tag-and-blockid-grammar.md) — the body-tag charset verbatim, and the consequences nobody wrote down: **`/` and `#` are illegal inside a tag** (so `#work/sub` is `#work`), `#123` is not a tag, a tag must follow whitespace; frontmatter tags use a **U+0020-only** filter that *disagrees* with the body rule; `getAllTags` (frontmatter-first, duplicates kept) vs `getTags()` (inline-first, de-duped) are two different lists; subtags are the search-time `^(regex)($|\/)`; block ids have **two** regexes and are grafted onto the **preceding sibling**, which is why a `blocks` position is the host's.
- [`2026-10-03-parsing-resolve-subpath-fallthrough.md`](2026-10-03-parsing-resolve-subpath-fallthrough.md) — the block and footnote branches **fall through** to the heading walk on a miss (no early return); footnote ids are **case-sensitive** with an unguarded `slice(2,-1)`, block ids lowercased; both `stripHeading` regexes **confirmed byte-identical**; §15's `AT`/`PT` citations are dead and those names are now webpack module ids; a third heading normaliser (`AD`) exists for display text.
- [`2026-10-03-parsing-footnote-grammar-and-inline-ids.md`](2026-10-03-parsing-footnote-grammar-and-inline-ids.md) — ids may not contain space/tab/LF, `[^a] : text` is not a definition, `[^` is reserved against markdown-it's reference rule; a **dangling `[^x]` renders as bare `x`** and is absent from the cache; unreferenced definitions aren't rendered; repeat labels are `[1]`/`[1-1]`/`[1-2]`; an inline footnote's cache id is **`"[inline" + N`**; synthetic footnote `listItem`s never reach `CachedMetadata`.

**Read the frontmatter one first** — §3's CRLF sentence is the sentence an implementer writes a
`processFrontMatter` from, and it is wrong in the direction that reformats a CRLF vault.

### 2026-10-03 — metadata domain, verified against the restored 1.13.7 corpus

Six addenda. The headline: **the `changed` event's second argument is the file
*contents*, not the old hash** — refuted by the one in-app consumer that compares it
to the view's own `this.data`. Also: **`worker.js` is in the restored corpus**, which
closes nine of `metadata.md` §13's ten open questions and turns up four `CachedMetadata`
fields §2 does not model at all (`referenceLinks`, lowercased `blocks` keys, the
`listItems.parent` unit, deleted-empty-arrays). Every `metadata/MetadataCache.js:*`
citation and every §15 short symbol in the domain doc is dead; a remap table is
included. Both 2026-06-10 metadata claims (UTF-16 offsets, `unresolvedLinks` casing)
**re-verified unchanged**, with `BL`↦`KF` and `getLinkpath`↦`bD`.

- [`2026-10-03-metadata-changed-event-payload.md`](2026-10-03-metadata-changed-event-payload.md) — `changed` is `(file, contents, cache)`; `queueFileForLinkResolution` runs *before* the trigger (§4's ordering paragraph has the mechanism backwards); `resolve` fires once per **queue entry** (the queue has no de-dup), not once per file; `didFinish` is a **trailing** 10 ms debounce, not "leading-edge"; `deleted` has zero in-app consumers. Full CONFIRMED table for all five events.
- [`2026-10-03-metadata-unresolved-links-key-normalisation.md`](2026-10-03-metadata-unresolved-links-key-normalisation.md) — the key pipeline is `bD` (cut at the **first** `#`) then `KF` (strip `.md` only when the **basename's** extension is `md`); **no lowercasing anywhere**, so §2/§8's "`BL(getLinkpath(linktext))`, lowercased" is wrong twice over. `updateRelatedLinks` matches by **substring containment** on lowercased basenames (`YF`), and skips never-resolved files entirely. Re-verifies the 2026-06-10 casing correction.
- [`2026-10-03-metadata-linkpath-dest-algorithm-corrections.md`](2026-10-03-metadata-linkpath-dest-algorithm-corrections.md) — four corrections to §8's algorithm, three of which change **which file a link resolves to**: the `.md` retry is **unconditional** (not an `else` on "contains a dot"), the sort comparator is **path length only with no alpha tiebreak** (ambiguity breaks by vault-listing order), the same-folder bias is a **raw string prefix with no `/`**, and the single-candidate shortcut tests "no `/` in the linktext". Plus: `[[/Note]]` (rooted, extensionless) **can never resolve**.
- [`2026-10-03-metadata-persisted-shape-and-v-migration.md`](2026-10-03-metadata-persisted-shape-and-v-migration.md) — on-disk positions are flat **6-number arrays** (`pD`/`fD`), per-ref `position` becomes `pos`, the flatten walker **skips** `footnotes`/`footnoteRefs`/`frontmatterLinks`; rows carry a **`v: 1` schema stamp that `saveMetaCache` never writes**, so every re-parsed row is silently re-migrated and re-`put` on the next launch; write coalescing runs for one **microtask** over a `["file","metadata"]` transaction with a swallowed-error reuse path.
- [`2026-10-03-metadata-worker-cached-metadata-shape.md`](2026-10-03-metadata-worker-cached-metadata-shape.md) — the real `CachedMetadata` from `worker/own/0169-expressionstatement.js`: **offsets are UTF-16 code units** (§2's "byte offsets" refuted, 2026-06-10 §5 re-verified); **`referenceLinks`** is an undocumented tenth field; **`footnoteRefs` is not the `^1` refs array** (an `fnRef` with a `contentPosition` lands in `footnotes`); `blocks` keys are lowercased and fed from **sections and list items**, not just headings; `listItems.parent` is a **0-based line number**; empty collections are **deleted**; a second worker message (`parseSections`) and a statement-identical **main-thread twin parser** (`uD`) exist. §13 Q4 answered: frontmatter is always at offset 0 and a **BOM defeats it entirely**.
- [`2026-10-03-metadata-blockcache-linkupdaters-and-issupportedfile.md`](2026-10-03-metadata-blockcache-linkupdaters-and-issupportedfile.md) — `isSupportedFile` does **not** gate indexing (§3 is wrong); it has exactly two call sites, link suggestions and the rename link-fixup. `linkUpdaters` is keyed by **bare extension** (`"canvas"`, not `".canvas"`) and its interface has a fourth method, `renameSubpath` — §13 Q3 answered, including the `update.reference` patch shape. `blockCache` is a **display-text index over a fresh re-parse**, never persisted, *not* a `^block-id` resolver (§7 and §11 both wrong). Canvas ref discovery walks **top-level nodes only**.
- [`2026-10-03-metadata-open-questions-and-citation-remap.md`](2026-10-03-metadata-open-questions-and-citation-remap.md) — §13 Q1 (`UF` returns **`"unknown"`**; `"tags"`/`"aliases"` come from `assignedWidgets`, not from inference; first-non-null-wins), Q2, Q4, Q5, Q6 (`getLinks()` tags nothing and **omits canvas/base entirely**), Q7 (yes, non-`.md` sources appear in `resolvedLinks`), Q8 (**`xT`/`CD` does walk `frontmatterLinks`, first**), Q10, plus a **remap of every dead `MetadataCache.js:*` citation** and of all 30 §15 short symbols.

**Read the changed-payload one first** — `metadata.md` §4 is the table an implementer
quotes when porting the five events, and its `changed` row is wrong.

### 2026-10-03 — workspace domain, verified against the restored 1.13.7 corpus

Six addenda. The headline: **the split `direction` convention is now proven from source
(six independent code paths), and the 2026-10-01 addendum's claim that it "refuted"
`workspace.md:79` is itself the thing that needed correcting** — line 79's gloss matches
1.13.7 exactly. All five 2026-06-10 serialization corrections below were re-checked and
**hold unchanged**.

- [`2026-10-03-workspace-split-direction-source-proof.md`](2026-10-03-workspace-split-direction-source-proof.md) — six source proofs that `direction` names the divider line (incl. `workspace:split-vertical` → `bd.commands.splitRight()`, icon `lucide-separator-vertical`; `"horizontal"` → `splitDown()`; `getElSize`/`setElSize` reading `offsetWidth` vs `offsetHeight`; `getAdjacentLeafInDirection`'s direction gate; `updateFrameless`'s all-children-vs-`children[0]` asymmetry), plus the doc-vs-doc conflict and a `WorkspaceSplit`/`Sidedock` confirmation table.
- [`2026-10-03-workspace-node-schemas-and-key-order.md`](2026-10-03-workspace-node-schemas-and-key-order.md) — exact key order for every node **and** for the root (`main, left, right, left-ribbon, floating, active, lastOpenFiles`); **`mobile-drawer.pinnedSize`** (a field neither `workspace.md` nor the 2026-06-10 addendum records); window `direction` hard-coded `"vertical"`; **a maximized popout is saved with pre-maximize bounds**; `saveLayout` early-returns when `!layoutReady`; ribbon `hiddenItems` carries an explicit `false` per item and the **right ribbon is never persisted**; `dimension` is written on truthiness with an inclusive `[0,100]`-outside clamp; `recomputeChildrenDimensions` is all-or-nothing. Re-verifies all five 2026-06-10 claims.
- [`2026-10-03-workspace-deserialize-and-platform-gating.md`](2026-10-03-workspace-deserialize-and-platform-gating.md) — `deserializeLayout` **mutates the input JSON** via `children.splice`; a bare `leaf` under a `split` is wrapped in a synthesised tab group; popouts need desktop **and Electron major ≥ 13**; `stacked: true` is dropped without `canStackTabs`; the root tree is flattened without `canSplit`; `active` resolves by leaf id only when its dock isn't collapsed; the no-`main` fallback opens `getLastOpenFiles()[0]` (capped at 10).
- [`2026-10-03-workspace-last-open-files-quota.md`](2026-10-03-workspace-last-open-files-quota.md) — `lastOpenFiles` has **four per-category quotas (25 md / 10 canvas / 10 image / 10 other ⇒ ≤ 55 on disk)** while `getLastOpenFiles()` returns at most **10**; every vault `create` event inserts an entry; `load()` does no validation, capping, or existence check.
- [`2026-10-03-workspace-named-workspaces-data-json.md`](2026-10-03-workspace-named-workspaces-data-json.md) — **there is no `workspaces.json`.** Named workspaces are the Workspaces core plugin's `data.json` (`.obsidian/plugins/workspaces/data.json`), shape `{workspaces: {<name>: LayoutJson & {mtime: string}}, active: string}`. Refutes `workspace.md` §3 and the Pass-1 on-disk-contracts line.
- [`2026-10-03-workspace-missing-event-and-small-items.md`](2026-10-03-workspace-missing-event-and-small-items.md) — **`hover-link-sources-changed`** is an 18th Workspace event, triggered by this domain and absent from §4; `focusLeaf` closes the Settings tab when it shares the target window; `registerEditorExtension` also rebuilds the spellcheck dictionary; `setViewState`'s completion order is `onLayoutChange`→`updateHeader`→`recordHistory`→`done`; the tab-group menu has a **trailing** `""` section (five, not four); `pushUndoHistory` skips empty leaves. Carries a batch confirmation of every debounce and history constant in §1, §5 and §8.

**Read the split-direction file first** — `addenda/README.md` still points implementers at
`workspace.md §2` for split guidance, and the record on that point was self-contradictory
until now.

### 2026-10-03 — vault domain, re-verified against the restored 1.13.7 corpus

The 2026-06-10 vault correction below was written against a 1.12.7 tree that has since been destroyed, so its own citations went dead. All four of its claims were re-checked and **hold unchanged** in 1.13.7 (see the re-verification note below), but the same pass turned up **eight further refutations of `vault.md`**, four of which are on-disk format:

- [`2026-10-03-vault-config-split-and-defaults.md`](2026-10-03-vault-config-split-and-defaults.md) — the `app.json`/`appearance.json` partition is a **16-key** allow-list, not the 6 `vault.md` §2 lists: `enabledCssSnippets`, `showViewHeader`, `showRibbon`, `nativeMenus`, `translucency`, `interfaceFontFamily`, `monospaceFontFamily`, `slidingSidebar`, `floatingNavigation`, `autoFullScreen` go to **`appearance.json`**. Plus the full defaults table, `openBehavior: ""`, `baseFontSizeAction: false`, `types.json`, and the Sync allow-list.
- [`2026-10-03-vault-hidden-paths-and-userignore.md`](2026-10-03-vault-hidden-paths-and-userignore.md) — the hidden-path predicate has **no `configDir` parameterisation** (§3's "except under `configDir`" is inverted; *any* dot-segment hides a path); `userIgnoreFilters` are **regexes, not globs**; they do not affect the tree, the link graph, or indexing.
- [`2026-10-03-vault-path-validation-and-filenames.md`](2026-10-03-vault-path-validation-and-filenames.md) — `checkPath` character sets are **platform-conditional** (POSIX forbids only `\ / :`; reserved device names and trailing dot/space are Windows-only); dotfiles have `extension === ""`; attachment-basename sanitiser character class; markdown-link encoder set.
- [`2026-10-03-vault-adapter-reconcile-and-trash.md`](2026-10-03-vault-adapter-reconcile-and-trash.md) — `TFile.saving` is a nesting **counter**, not a boolean; `getNewPathAfterRename` **replaces** control chars with a space; a watcher-seen ENOENT gets a **100 ms grace** before the node is removed; the mobile adapter's `.trash/` naming **does** preserve the extension (§1/§3 wrong); `.trash/` is never read by core, refuting §13 Q7's "presumably a cleanup tool" guess.
- [`2026-10-03-vault-filemanager-content-rewrites.md`](2026-10-03-vault-filemanager-content-rewrites.md) — the frontmatter serialiser pins `lineWidth: 0` / `nullStr: ""` / `aliasDuplicateObjects: false` (**no YAML line wrapping**); `processFrontMatter` deletes the block when the mutator empties it; `insertIntoFile`'s exact newline rule; `generateMarkdownLink`'s same-file collapse and empty display text for non-`.md`; the orphan-attachment sweep is **two-phase** with a `not-in {md,canvas,base}` filter.
- [`2026-10-03-vault-prior-corrections-re-verified.md`](2026-10-03-vault-prior-corrections-re-verified.md) — re-anchors the 2026-06-10 claims below to 1.13.7 and maps `vault.md` §4's dead `Vault.js:*` / `FileManager.js:*` references to their live locations.
- [`2026-10-03-workspace-split-direction-source-proof.md`](2026-10-03-workspace-split-direction-source-proof.md) — **corrects the attribution** of the 2026-10-01 split-direction addendum below: the mapping it gives is right, but `workspace.md:79`'s gloss was already right too; the inverted thing was Corbomite's implementation, not the frozen doc.

### 2026-10-03 — bases domain, verified against the restored 1.13.7 corpus

One addendum. The headline: **the `.base` writer canonicalises property IDs and `bases.md` §3's schema block does not show it** — `hQ` strips the `note.` prefix from `order`/`sort`/`groupBy`/`summaries` keys and shortens `file.file` to `file`, while `properties:` and the newly-found `data.columnSize` are emitted **raw**, so one file carries two key conventions for the same logical property. Also: **`not:` is NOR, not NAND** (refuting the 2026-04-17 addendum §10), **`html()` returns the `Markdown` value and nothing returns the `HTML` value** (`labelGlobalMarkdown` survives in the locale tables with zero code references — §13 Q9 answered), and **`duration("5ms")` can never parse** because the unit regex has no `ms` alternative, making three branches dead code. Plus the whole per-view-type `data:` key inventory (§3 mentions none of it), `.base` as a CodeMirror language with a tab→4-space rewrite and a 100 ms `replaceCode` write-back, `![[X.base#View]]` selecting a named view (§13 Q6 answered), six commands where §6 says none, and `QueryController` found at `1813-CZ.js:1893` with the refresh gate shown to be a `JSON.stringify` fingerprint (§13 Q2/Q3 answered).

- [`2026-10-03-bases-base-file-serialization-and-dsl-corrections.md`](2026-10-03-bases-base-file-serialization-and-dsl-corrections.md) — **read §A before promoting the §3 schema to `VAULT-FORMAT.md`**: `note.status` does not survive a write, `columnSize` does, and free-form view keys are relocated after the recognised ones. `summaries: {}` is emitted when empty but `formulas: {}` is not; `limit: 0` is dropped at parse; a non-array `sort` is silently ignored while a non-array `order` throws; `sort[].column` is a legacy alias; `BasesViewConfig` has **no** `unrecognizedData`, refuting §8's `[CRIT]` "at every level". On the DSL: `date()` takes `YYYYMMDDHHmm`/`YYYYMMDD` and returns `NullValue` not an error Value, `link("https://…")` returns a `UrlValue`, `d.relative()` returns a `RelativeDateValue`, `tagMatches` is one-directional (§1/§11 backwards), `list.sort()` **has** a comparator and `unique()` uses `equals` not `looseEquals` (§14's TBDs resolved: `stddev()` is population), `file.isEmpty()` **throws**, a `NullValue` argument null-propagates the whole call, the registered `if` is dead code, `duration("…ms")` is unreachable, and `o.map`'s stub is not identity. Re-verifies 2026-06-10 §2's `summaries` double-handling **unchanged**, and re-cites the 2026-04-17 addendum's dead `_internal.js` names. **Do not start `libs/bases` from §3 or from the 2026-04-17 catalogue as written.**

### 2026-10-03 — editor domain, verified against the restored 1.13.7 corpus

- [`2026-10-03-editor-wrapper-commands-and-keymaps.md`](2026-10-03-editor-wrapper-commands-and-keymaps.md) — **`Editor` is half-documented.** The public export binds the *mixin*; the concrete `EditorView`-owning subclass (`app/own/1823-RJ.js:885`) adds **~45 undocumented methods** — the 7-entry format-markup table, a ~48-binding base keymap, undo-history snapshotting, fold-restore line-count gate, clipboard hooks. §14's sizing basis is off ~2x. `exec` is a fixed **18-key** table that **throws** on unknown names. `insertCallout()` selects `NOTE` in both branches. Suggesters are link / tag / **footnote** (no headers suggester). **Audit-wide: `debounce` (`Ml`, `app/own/0355-Ml.js:22-27`) is a leading-edge throttle, not a trailing debounce** — repeat calls inside the window do not reset the timer, so every debounce/timing claim needs re-reading.

### 2026-10-03 — editor-markdown domain, verified against the restored 1.13.7 corpus

- [`2026-10-03-editor-markdown-view-modes-and-preview-pipeline.md`](2026-10-03-editor-markdown-view-modes-and-preview-pipeline.md) — **The async preview worker is single-flight and contention degrades to a blocking main-thread parse** (`app/own/1500-wW.js:11-13` returns `null` when in flight; `parseAsync` doesn't check, so the `catch` runs `parseSync()`). `TW` is module-level, so contention is app-wide — a Corbomite thread-pool-with-queue would diverge. **`commonmark: true` is hard-coded with no user escape hatch**; `breaks` is a process-global singleton. 8 corrections (save throttle, Ctrl+Wheel, double-tap, sortOrder, rAF-vs-timeout) + full citation remap: every `MarkdownView.js:*` style reference in the frozen doc is dead.

### 2026-10-03 — views domain, verified against the restored 1.13.7 corpus

- [`2026-10-03-views-leaf-viewstate-and-deferred-restore.md`](2026-10-03-views-leaf-viewstate-and-deferred-restore.md) — **There is no three-way merge** (`app/own/1810-bZ.js:2-8`): `diff_main(base, view)` -> `patch_make` -> `patch_apply(patches, disk)[0]`, with per-patch success discarded. Demonstrated by execution against the extracted code: a mid-paragraph user edit is **discarded and its text relocated onto the preceding line**, with only a notice. Refutes §1/§12's "does not lose in-progress edits". Also: a **second registry `app.embedRegistry`** exists besides `viewRegistry`, and `_empty` is a per-leaf singleton, not global.

### 2026-10-03 — search domain, verified against the restored 1.13.7 corpus

- [`2026-10-03-search-dsl-grammar-and-quick-switcher-scoring.md`](2026-10-03-search-dsl-grammar-and-quick-switcher-scoring.md) — **The DSL grammar is in-tree and readable** (`app/own/1175-SP.js:148-221`, a ~60-line operator table) — refuting §13 Q1's "out-of-tree, reverse-engineer from documentation". Real flags are `match-case` + **`ignore-case`**; `whole-word` **does not exist**. Missing from the doc: `content:`, `task:`, `task-todo:`, `task-done:`, `[key:value]`, `TRUE`/`FALSE`/`EMPTY`, `>`/`<`. **`PARITY-MATRIX.md` §5 overclaims**: quick-switcher fuzzy constants match but the *penalty rule* does not (two worked examples).

### 2026-10-03 — settings domain, verified against the restored 1.13.7 corpus

- [`2026-10-03-settings-config-file-ownership-and-definitions.md`](2026-10-03-settings-config-file-ownership-and-definitions.md) — **Definitive tab -> config-key -> on-disk-file table.** §3's table is wrong in 3 of 7 rows: **4 keys it names do not exist** (proven exhaustively — 55 unique `getConfig`/`setConfig` keys, all keys of the defaults table `ME` at `app/own/0920-eE.js:1174-1237`), and **17 real keys are missing**. **`.obsidian/<corePluginId>.json` is a whole file class the doc omits** — core-plugin data goes through `readConfigJson(pluginId)`, *not* `plugins/<id>/data.json`. Encodings that corrupt a vault if missed: `openBehavior` is `"file:" + path`, not an enum; the three font keys are comma-joined **strings**. `display()` is no longer the contract (declarative `getSettingDefinitions()`), so a `display()`-only tab is **invisible to global settings search**. Safe mode is a `localStorage` flag, not absence of `community-plugins.json`. Cross-checked and **confirmed** the 16-key `app.json`/`appearance.json` partition.

### 2026-10-03 — plugin domain, verified against the restored 1.13.7 corpus

- [`2026-10-03-plugin-loader-lifecycle-and-capability-surface.md`](2026-10-03-plugin-loader-lifecycle-and-capability-surface.md) — **Corbomite deliberately does not mirror this domain's API** — a C++ plugin is not a web plugin. What *is* transferable is documented as a 15-item capability surface. Load-bearing facts: **there is no permission model at all** (zero permission hits; the only gate is `Ab.plugins`); enable-set order *is* load order; `minAppVersion` is a real install gate; a throw from `onload` **leaks a half-loaded instance that is never retried**; `loadCSS()` is not opt-in (§12 is backwards); `addCommand` **silently overwrites** on duplicate id; `saveData` **cannot report a failed write**. §10's enumeration is complete (26 members). 8 corrections + 17 catalogued defects. Web-delivery surface (`require` shim, `window.eval`, inline-sourcemap stripping, hot-reload) is explicitly marked **not applicable by design**.

### 2026-10-03 — core domain, verified against the restored 1.13.7 corpus

- [`2026-10-03-core-command-and-hotkey-contracts.md`](2026-10-03-core-command-and-hotkey-contracts.md) — **`.obsidian/hotkeys.json` is override-only** (`app/own/0833-$w.js:57-76`): `bake()` walks custom keys first and falls back to defaults only when `!hasOwnProperty` — so `"some:command": []` **unbinds** the command, the only way Obsidian expresses an unbind. **115 command ids, not "30+"**; implementing only the doc's 25 silently drops every `editor:*` override in a real vault. `Modal` allocates a **parentless** `Scope`, so the root scope and its hotkeys are unreachable while a plain modal is open. `Events.off` deletes *all* listeners on a no-match. 21 NEW + 8 corrections; §13 Q1-Q6 answered, Q7 refuted.

### 2026-10-03 — ui-bundle domain, verified against the restored 1.13.7 corpus

- [`2026-10-03-ui-bundle-widgets-and-overlays-source-proof.md`](2026-10-03-ui-bundle-widgets-and-overlays-source-proof.md) — **The menu section protocol is a hierarchical `a.b.c` namespace** (`app/own/0751-my.js:62-81`): separators key on the id's *first dot-segment*, and `submenuConfigs` matching is **prefix-based**, so `info` and `info.copy` render contiguously with no separator — the doc's flat list cannot produce this. **The shipped Lucide table has 1408 entries, not 125** and is *generated* by `getIconIds()`, not hand-listed. `PopoverState` is a **string enum with reversed member order** (so §2, §8's `Hidden === 0` and §13 Q1 are all wrong); `SliderComponent.setDynamicTooltip`/`showTooltip` are empty stubs; `Modal.open` listens for `pagehide`, not `beforeunload`; `openSubmenuSoon` is a leading-edge throttle. **Web→desktop UI translation is intended, so DOM-widget choices are not reported as gaps** — only observable-behaviour and data-contract differences.

### 2026-10-03 — leaf-utilities domain, verified against the restored 1.13.7 corpus

- [`2026-10-03-leaf-utilities-subpath-rename-and-leaf-bundle-corrections.md`](2026-10-03-leaf-utilities-subpath-rename-and-leaf-bundle-corrections.md) — **Heading rename matches with `stripHeading` but writes with `stripHeadingForLink`** — asymmetrically, in adjacent lines (`app/own/1312-hR.js:205-206`). The match side strips `* ! [ ] % ` + backtick that the write side **keeps**, so matching is permanently looser than the bytes written. The ref set is `frontmatterLinks ∪ links ∪ embeds`, so **renames rewrite YAML property values**, and the applier **re-dumps the whole frontmatter block through js-yaml** — silently keeping body edits but dropping frontmatter edits if the YAML won't parse, and silently no-opping on dotted keys. Block-id rename uses a third rule entirely. `SecretStorage` **extends `Events`** and fires `changed` 3x (refuting §4); `Scope.handleKey` lets a specific binding returning `undefined` **swallow** the event; `requestUrl` **drops `opts.headers`** on the web path.

### 2026-06-10 batch

- [`2026-06-10-vault-path-and-naming-corrections.md`](2026-06-10-vault-path-and-naming-corrections.md) — `getAvailablePath` collision suffix starts at ` 1` not ` 2`; `unresolvedLinks` keys keep original casing; `normalizePath` never strips `./` (and does strip leading slashes); 250-**UTF-16-unit** (not byte) attachment truncation; all CachedMetadata offsets are UTF-16 code units. **All re-verified unchanged in 1.13.7.**
- [`2026-06-10-workspace-serialization-corrections.md`](2026-06-10-workspace-serialization-corrections.md) — window nodes serialize bounds flat (no nested `size`); mobile-drawer node has no `width`/`collapsed` and writes `currentTab` unconditionally; `pinned` is written at both leaf-node and ViewState level; `group` is a sibling of `state` (views.md wrong); `hotkeys.json` records may carry a `code` field.
- [`2026-06-10-editor-timing-and-coverage-corrections.md`](2026-06-10-editor-timing-and-coverage-corrections.md) — `expandText` fires from a 10 ms-debounced CM updateListener (not `inputHandler`); the third `lP` regex is redundant with the *first* pattern (editor.md's `？` sentence is garbled); live-preview/table-editor/vim/DnD/find-replace engine is **unaudited** `_internal.js` territory mislocated by editor.md §14.
- [`2026-06-10-taxonomy-and-bases-corrections.md`](2026-06-10-taxonomy-and-bases-corrections.md) — `QueryController` does NOT parse the search DSL (taxonomy refuted; treat `00-taxonomy.md` as a routing index only); Bases `summaries:` key is double-handled into `unrecognizedData` then overwritten at serialize; bases.md Open Q2 stale (QueryController since extracted); 2026-04-17 bases addendum's corpus paths moved to `/home/clinton/bin/ObsidianRAW/audit/`.

## File naming

`YYYY-MM-DD-<short-topic>.md`

Examples:
- `2026-05-12-bases-formula-grammar.md`
- `2026-06-03-frontmatter-eof-edge-case.md`
- `2026-08-19-vault-config-app-json-extra-keys.md`

## File format

```markdown
# <Short title>

**Date:** YYYY-MM-DD
**Discovered during:** Cluster <X> Phase <N> implementation (or "exploratory spike", or "user report")
**Supersedes / extends:** `domains/<file>.md §<section>` (or "no prior coverage in audit")
**Relevant cluster plans:** `superpowers/plans/<file>.md` (list any plans that should cite this addendum)

## Finding

<one or two paragraphs describing the new fact, the corrected fact, or the previously unknown behaviour>

## Why noticed now

<one paragraph: what implementation work or question caused this to surface>

## Action taken

<one paragraph: what cluster plans were updated to cite this addendum, what compat behaviour was chosen, what tests were added>
```

## Linkage

After writing an addendum:

1. Append a one-line bullet to `docs/obsidian-audit/00-taxonomy.md` under the `## Addenda` section, sorted by date (most recent on top).
2. If the addendum **contradicts** a Pass 2 or Pass 3 claim, also update the relevant cluster plan's "Audit references" section to cite both. Do **not** edit the original audit doc.
3. If the addendum reveals a new gap or extension surface, append a bullet to `docs/obsidian-audit/01-markoff-gaps.md` or `02-extension-surfaces.md` under a `## Implementation additions — YYYY-MM` heading.
4. If the addendum surfaces a load-bearing decision, log it in `docs/PROJECT-STATE.md` Recent decisions.

## What is **not** an addendum

- Corbomite implementation choices (those go in cluster plans or `docs/cluster-retros/`).
- Build / test fixes (commit messages cover those).
- General notes-to-self (use a scratch markdown elsewhere).

Addenda are exclusively *new or corrected facts about Obsidian's behaviour*.
