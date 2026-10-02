# Cluster Q — The Tags pane

**Date:** 2026-10-01
**Type:** Full plan (new cluster, post-reset letter **Q** — see §8 on the
letter collision)
**Status:** **Plan written 2026-10-01 — ready to dispatch. Not started.**
**Sizing:** ~3–5 days across three phases (Q0 ~0.5d, Q1 ~1.5d, Q2 ~1–2d).
Cheap relative to everything else open, and it closes a *release criterion*.

> **Post-reset Cluster Q.** Letters taken post-reset are A, B, C, D, E, F, I,
> K, L, M, N, O, P (plus G, H, J obsoleted by the 2026-05-25 foundation
> rewrite), so **Q is the next free letter**. It collides with **legacy
> Cluster Q** (Internal-Plugin Wrapping + Permissions, closed 2026-04-17, and
> legacy **Cluster Q.0** Vault architecture) — both pre-reset, both in
> `archive/`. Always qualify: "Cluster Q (post-reset)" vs "legacy Cluster Q".
> This is also not `docs/cluster-retros/cluster-q.md`, which is the legacy one.

---

## 1. Goal

Ship the one core panel that does not exist at all: a **Tags pane** — the
right-sidebar panel that lists every tag in the vault with per-tag counts, and
whose click runs a tag search. Today `grep -rln 'TagPane\|TagsPane\|TagsView'`
over `src/` and `libs/` returns **zero hits**. The feature has been described
in exactly two places in the whole documentation set, both in
`2026-06-10-road-to-dogfood.md`, and both as bullets in a phase list rather
than as tasks:

- `:135` — *"**Tags pane** — the one wholly-missing core panel."* (Phase 4 bullet)
- `:166` — a release criterion: *"Phases 0–3 complete; Phase 4 canvas-edges +
  graph-persistence + tags pane done"*

A release criterion with no owner is how a feature survives two releases
(`v0.1.0`, `v0.2.0`) and a 4-month gap. Filed as
`[views][P2]` on the punch list 2026-10-01; this plan is its home.

**Good news, verified 2026-10-01: every layer this panel needs already exists.**
The panel itself is a missing widget, not a missing subsystem — which is why
this is 3–5 days and not a cluster-sized effort.

---

## 2. Current-state audit (verified 2026-10-01 against the tree)

### 2.1 What already exists — reuse, do not rebuild

| Layer | Status | Evidence |
|---|---|---|
| Tag rows in the index | **live** — a `note_tags(note_path, tag)` table with an index on `tag` | `libs/storage/src/SQLiteIndex.cpp:148-154` (DDL), `:197` (per-note delete), `:320-329` (insert) |
| Tag *extraction*, inline `#tag` | **live** | `libs/storage/src/MetadataParser.cpp:408-419` (`doc->tags()` → `TagCache`) |
| Tag extraction, frontmatter `tags:` **merged** | **live** — the addendum's "no frontmatter merge" complaint is out of date | `libs/storage/src/MetadataParser.cpp:421-451` — `pushFmTag` for `tags` array **and** singular `tag` string |
| Tag with the leading `#` stored verbatim | by design, mirroring Obsidian's `TagCache.tag` shape | `libs/storage/src/SQLiteIndex.cpp:320-321` (comment), `docs/obsidian-audit/domains/metadata.md:107` |
| Aggregate query: distinct tag list | **live** | `SQLiteIndex::allTags()` — `libs/storage/src/SQLiteIndex.cpp:624-635`, header `:134`. **Flat + sorted, no counts** |
| Per-tag note lookup | **live** | `SQLiteIndex::notesWithTag()` — `libs/storage/src/SQLiteIndex.cpp:637-650`, header `:135` |
| **The `#tags` query surface** | **live** — `tag:#X` is a first-class DSL operator, with `tag:`/`-tag:` compiled into SQL against `note_tags` | `libs/search/include/corbomite/search/SearchDSL.h:19,35-36`; `libs/storage/src/SQLiteIndex.cpp:476-481`; UI hint in `src/plugins/search/SearchView.cpp:100,233` |
| `#tag` completion | **live** | `src/editor/TagSuggest.h` (built-in `EditorSuggest`), constructed in `src/app/MainWindow.cpp` |
| A **click-a-tag → run that search** precedent | **live and identical in shape** — Bases' tag cells already do exactly this | `libs/bases/src/BasesView.cpp:218` → `setTagSearchHandler` (`libs/bases/include/corbomite/bases/BasesView.h:83`), wired at `src/app/MainWindow.cpp:1191-1195` (`showSearchForQuery("tag:%1")`) |
| Plugin mounting in the **right** sidebar | **live** — manifest keys, no code | `X-Corbomite-DockArea: "right"` precedent in `src/plugins/outlinks/metadata.json.in` |
| Reveal the panel from a command | **live** | `WorkspaceController::revealDockView(slug)` — `libs/core/include/corbomite/core/proxies/WorkspaceController.h:69`; precedent `src/plugins/outlinks/OutlinksPlugin.cpp:29-41` |
| Tag data available **to a plugin** already | **live** — `metadata.read` grants both calls | `libs/storage/include/corbomite/storage/proxies/MetadataCacheReader.h:32` (`tagsIn`) and `:33` (`allTags`) |

### 2.2 What is missing

| Gap | Why it matters |
|---|---|
| **The pane.** No widget, no view class, no plugin, no dock registration | the whole feature |
| **Counts.** `allTags()` returns a sorted distinct list; nothing computes a frequency | Obsidian's pane is count-first — `#proj` with 42 is a different affordance from `#proj` |
| **Subtag-prefix decomposition.** Obsidian's `getTags()` walks a subtag-prefix loop so `#proj/foo/bar` increments `#proj`, `#proj/foo`, and `#proj/foo/bar` | without it, a vault that only uses nested tags shows a flat list of long leaves and typing `#proj` in search under-reports. Called out as a live gap in `docs/audit-2026-04-26/metadata.md:160-168,236` and `docs/obsidian-audit/domains/metadata.md:58,396` |
| **`#`-convention disagreement at the plugin boundary** | `MetadataCacheReader::stripHash` strips the `#` while `cache.tags[].tag` keeps it, so plugin-facing and in-cache shapes disagree. `docs/audit-2026-04-26/metadata.md:169-172` recommends documenting the convention in the proxy header — **not yet done**; `MetadataCacheReader.h` still says nothing about it |
| A way for a plugin to run a global search | `showSearchForQuery` is a private `MainWindow` method (`src/app/MainWindow.h:160`); the search view is reachable only by `QMetaObject::invokeMethod` on a host-held pointer (`src/app/MainWindow.cpp:2645-2658`). A tags **plugin** cannot do what Bases does with a host callback |
| The pane's **own** UI spec | see §3 — this is the real gap in the *documentation*, not the code |

### 2.3 Obsidian-side spec (what the audit corpus actually says)

- **Layout/behaviour:** *"Right-sidebar tag tree; subtag counts; click to
  `openGlobalSearch("tag:" + name)`"* — `docs/obsidian-audit/FEATURE-MATRIX.md:553-561`.
  Status there ("Partial … inline-only tag extraction; no frontmatter merge;
  no subtag-prefix counting") is **stale**: the frontmatter merge shipped (see
  §2.1), the subtag counting did not.
- **The data contract:** `MetadataCache.getTags()` → `Record<tagString, count>`
  with subtag-prefix decomposition; `getAllTags(cache)` concatenates
  frontmatter tags (via `parseFrontMatterTags`) then inline tags, preserving
  order and duplicates — `docs/obsidian-audit/domains/metadata.md:43,53-58`.
- **Tag normalisation:** frontmatter tags get `#` prepended at read time; blank
  tags and tags **containing a space** are silently dropped; the `tags` key
  match is case-insensitive — `docs/obsidian-audit/domains/parsing.md:96-108`.
  **Two Corbomite divergences live here, neither documented:**
  ① Corbomite **also** accepts a singular `tag:` key
  (`libs/storage/src/MetadataParser.cpp:452-454`), which Obsidian's
  `parseFrontMatterStringArray(fm, /^tags$/i)` does not; ② Corbomite keeps a
  space-containing frontmatter value as **one** tag
  (`MetadataParser.cpp:447-450`, *"treat the whole thing as one tag per
  Obsidian's permissive handling"*) where Obsidian **drops** it. Both mean a
  vault can produce a tag Corbomite shows and Obsidian cannot reproduce — see
  Q4.
- **Where `openGlobalSearch("tag:"+name)` lives:** `search.md:9` notes **21
  call sites** in Obsidian's `_internal.js` use exactly that string;
  `search.md:264` records the Corbomite-side status as *"Direct Qt call, not a
  query string. Acceptable divergence."*
- **Plugin id:** `tag-pane`, one of the 31 internal-plugin ids
  (`domains/core.md:234`, `VAULT-FORMAT.md:186`).

### 2.4 ⚠ The pane's own UI is an **audit gap** — read this before Q1

There is **no `tag-pane` domain doc**. `domains/` has 15 docs and none covers
this panel's own surface: sort options (name vs count), the filter/search field
in the pane header, tag nesting presentation (flat list with prefix counts vs a
collapsible tree), per-tag colours, the right-click menu (rename tag, search,
copy tag, open in new pane), and the `Settings → Tags` options.

`FEATURE-MATRIX.md:553-561` is three sentences and is a **Pass 3 synthesis
doc**, frozen at 2026-04-14 and explicitly stale on the Corbomite side (per
`CLAUDE.md`). Everything else about this feature is reverse-engineered from
Obsidian's shipped behaviour; this part is not reverse-engineered at all.

**Consequence:** Q1 must not invent a UI and call it parity. Two honest options,
both cheap:

- **(a)** Ship the minimum that the one sentence of spec supports — a flat,
  count-sorted, clickable list in the right sidebar — and file the richer UI
  (sort toggle, filter field, nesting, context menu) as **stretch**, explicitly
  labelled "not Obsidian-verified".
- **(b)** Spend ~half a day writing an addendum (see §5 Q3) from
  `testvaults/obsidian-help/` or a live Obsidian session, the way Cluster F-b
  proposes for `workspaces.json`, and then build to it.

Recommendation: **(a) now, (b) as a Q3 addendum before Q2's stretch items.**
The release criterion is "the pane exists and works", not "the pane matches
Obsidian's options menu".

---

## 3. Scope

### In scope

- A `tags` internal plugin (`src/plugins/tags/`) that mounts in the **right**
  sidebar and registers a `tag-pane` command (Q1).
- Tag counts, and **subtag-prefix decomposition** in the data layer (Q1/Q2).
- Click → run the tag search (Q1).
- Pane-local filter field + alphabetical/count sort toggle (Q2, stretch-labelled).
- Documenting the `#`-stripping convention in the proxy header (Q2, one comment).
- Tests named in §6.

### Out of scope

- **Tag renaming/merging** — a write-side feature Obsidian has, but it mutates
  user files; that is a punch-list item at most, and it deserves its own scope
  after the pane exists.
- Graph-view tag filters (`GraphControlsPanel.cpp:79-80` TODOs — separate
  punch-list item).
- Bases tag columns (already shipped via `TagListValue`, per Cluster D).
- The `Fullscreen` / `Mod+1..9` items that share a `PARITY-MATRIX.md` row with
  this one (row 123) — different features, left where they are.
- Nested-tag *folding* presentation (Q2 stretch only).

---

## 4. Open questions

- **Q1 — host view or internal plugin?** Corbomite's model says internal
  plugins own sidebar views (`src/plugins/{backlinks,outlinks,...}`), and the
  audit says Obsidian's own features are internal plugins so the external API
  mirrors the internal one (`docs/obsidian-audit/02-extension-surfaces.md:42`).
  Recommendation: **plugin**, matching the 9 existing ones. But see Q2.
- **Q2 — the search hand-off is the awkward part.** Bases solved it with a
  host-supplied `std::function` callback (`BasesView.h:83`). A *plugin* cannot
  do that (its `createView(MainWindow*)` does receive the host pointer, so it
  could — but the callback would have to be a host-side API, which means
  touching `MainWindow`/the plugin ABI). Options:
  (a) host callback, same as Bases — smallest, but duplicates a seam;
  (b) a new `WorkspaceController::runGlobalSearch(query)` verb — a small public
      API addition, and arguably the *right* one to add (Obsidian has
      `openGlobalSearch` on the app object);
  (c) invoke `SearchView::setQuery` by name like `MainWindow` does
      (`MainWindow.cpp:2655`) — fragile, host-internal, **not recommended**.
  Recommendation: **(b)**, and fold it into the same review as Cluster D.5's
  plugin-API spec so public-surface additions are reviewed together.
- **Q3 — does the pane-UI addendum happen before Q2?** See §2.4.
- **Q4 — two undocumented tag-normalisation divergences.** (i) Corbomite
  accepts a singular `tag:` frontmatter key that Obsidian ignores
  (`MetadataParser.cpp:452-454`); (ii) Corbomite keeps a space-containing
  frontmatter tag value as one tag where Obsidian drops it
  (`MetadataParser.cpp:447-450`). Both mean Corbomite can display a tag that
  Obsidian will not reproduce from the same file. Recommendation: **keep both**
  (a superset is kinder than a silent drop) and **write the addendum** so the
  divergence is documented rather than accidental. Do not "fix" either without
  the user's call.

---

## 5. Work breakdown

### Phase Q0 — Registration and the data-layer fix (no UI yet)

- [ ] **Q0.1** Add `tagCountMap()` (or `allTagsWithCounts()`) to
      `SQLiteIndex` — `SELECT tag, COUNT(*) FROM note_tags GROUP BY tag`. One
      statement, next to `allTags()` at
      `libs/storage/src/SQLiteIndex.cpp:624`. Expose it on
      `MetadataCacheReader` so a plugin can reach it under `metadata.read`
      (the proxy today has `allTags()` only,
      `libs/storage/include/corbomite/storage/proxies/MetadataCacheReader.h:33`).
- [ ] **Q0.2** **Subtag-prefix decomposition** as a pure, separately-testable
      function (this is the part Obsidian gets right and Corbomite does not):
      for each stored tag, increment every `/`-delimited prefix as well as the
      tag itself. Decide and document the leading-`#` convention here — the
      decomposition must split on `/` after the `#`, not before.
- [ ] **Q0.3** Document the convention in the proxy header, closing the
      `stripHash`-vs-`cache.tags` disagreement
      (`docs/audit-2026-04-26/metadata.md:169-172`). One comment block, no
      behaviour change.
- [ ] **Q0.4** Cache invalidation: the map must refresh when the index changes.
      Reuse the existing `cacheChanged`/`indexFinished` signals on
      `MetadataCacheReader` (`:41,53`) rather than adding a new one.
- [ ] **Q0.5** `tst_tag_counts` — counts, decomposition (`#proj/foo` → `#proj`
      + `#proj/foo`), singular-tag frontmatter, empty vault, `#`-convention
      invariants.

### Phase Q1 — The pane

- [ ] **Q1.1** New plugin dir `src/plugins/tags/` with `CMakeLists.txt`
      (`corbomite_add_plugin(... TRUSTED)`, mirroring
      `src/plugins/outlinks/CMakeLists.txt`) **and** one line in
      `src/CMakeLists.txt` next to the other nine `add_subdirectory(plugins/*)`
      calls (currently `:117-125`).
- [ ] **Q1.2** `metadata.json.in` mirroring the outlinks manifest:
      `"X-Obsidian-Id": "tag-pane"`, `"X-Corbomite-DockArea": "right"`,
      permissions `["metadata.read", "vault.read", "workspace", "ui.views",
      "ui.commands"]`, icon from `QIcon::fromTheme` themes that exist.
- [ ] **Q1.3** `TagsView` — a `QWidget` (mirror `src/plugins/outlinks/OutlinksView.h`
      for the proxy-lifetime comment: proxies are owned by `PluginContext`,
      which outlives the widget). `QTreeView`/`QListView` + a small model, or
      `QListWidget` if the model buys nothing at this size.
- [ ] **Q1.4** Populate from `MetadataCacheReader`; sort by count descending
      then name (Obsidian's default is *not* verified — see §2.4; make the sort
      a constant that Q2 can flip, and say so in a comment).
- [ ] **Q1.5** Click → tag search, via **Q2's chosen mechanism**. If Q2(b) lands,
      this is `workspace->runGlobalSearch("tag:" + tag)`. Also wire
      `tags:open` → `workspace->revealDockView("tags")` like outlinks does
      (`OutlinksPlugin.cpp:29-41`).
- [ ] **Q1.6** Empty state ("No tags in this vault yet") — copy the empty-label
      pattern from `OutlinksView`.
- [ ] **Q1.7** i18n every string. No `QStringLiteral` user-visible text.
- [ ] **Q1.8** `tst_tags_view` — model contents against a seeded index,
      count sort, click emits the right query string, empty state.

### Phase Q2 — Polish (stretch-labelled until Q3's addendum exists)

- [ ] **Q2.1** Filter field in the pane header (fuzzy, via the existing
      `FuzzyMatcher` — `libs/search/include/corbomite/search/FuzzyMatcher.h`).
- [ ] **Q2.2** Sort toggle: count ↔ alphabetical.
- [ ] **Q2.3** Nesting presentation (indented sub-tags) — **only** if Q3's
      addendum confirms Obsidian's behaviour.
- [ ] **Q2.4** Context menu: copy tag name, search tag (also reachable via
      plain click), open in a new pane.
- [ ] **Q2.5** The `runGlobalSearch` proxy verb from Q2(b) gets its own tests in
      `tests/core/tst_proxy_workspace.cpp` (the existing proxy test) if it is a
      new `WorkspaceController` method rather than a Bases-style callback.
- [ ] **Q2.6** `PARITY-MATRIX.md` row 123 updated to split "Tags pane" from
      "Fullscreen / Mod+1..9", which share the row today.

---

## 6. Named tests

| Test | Binary | Covers |
|---|---|---|
| `tst_tag_counts` | new, `tests/storage/` | count map, subtag decomposition, singular `tag:` frontmatter, empty vault, `#` convention |
| `tst_tags_view` | new, `tests/plugins/tags/` (mirroring `src/plugins/outlinks/tests/`) | model population, sort order, click → query string, empty state |
| `tst_proxy_workspace` | **existing**, `tests/core/` | extended only if Q2(b) adds `runGlobalSearch` to `WorkspaceController` |
| `tst_search_dsl_pipeline` | **existing**, `tests/integration/` | must stay green — the `tag:` operator is the pane's whole downstream, and must keep working against a `note_tags` table that Q0.1/Q0.2 now query differently |
| `tst_metadata_cache_worker_integration` | **existing** | frontmatter tag merge must stay green (it is the reason the audit's "no frontmatter merge" line is stale) |

Falsifiability note: Q0.2's decomposition test must be **seen red** before
Q0.2 lands — a decomposition function that is accidentally a no-op on the test
vault's tag set would otherwise pass.

---

## 7. Definition of done

- [ ] The Tags pane appears in the right sidebar, lists every tag with a count,
      and updates when the index changes.
- [ ] `#proj/foo` contributes a count to `#proj` as well as itself.
- [ ] Clicking a tag runs a search that returns exactly the notes carrying it —
      verified **end to end in a real vault**, not just at the query-string level.
- [ ] `[views][P2]` "Tags pane" punch-list item marked `[x]`; the two
      `road-to-dogfood.md` bullets are either struck or cross-linked to this
      plan so the release criterion has an owner.
- [ ] `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E "benchmark|perf" --output-on-failure -j 10`
      green; every new test seen red once.
- [ ] **Human live eyeball** of the pane, the sort, and the click-through. This
      is a visible panel in the main window — offscreen cannot tell you whether
      it looks right or whether the counts are plausible.
- [ ] The Q3 addendum either written or explicitly deferred with a reason, so
      §2.4 is no longer an unowned unknown.

---

## 8. Audit references

- [`docs/obsidian-audit/FEATURE-MATRIX.md`](../../obsidian-audit/FEATURE-MATRIX.md) §"Tags pane" (line 553) and §"Frontmatter tags" (line 583) — the one-sentence behaviour spec. **Stale on two of three points.**
- [`docs/obsidian-audit/domains/metadata.md`](../../obsidian-audit/domains/metadata.md) §1 `getAllTags` / `getTags` (lines 43, 53-58, 396), §2 `TagCache` (line 107), §6 tag completion (line 356) — the data contract, including the subtag decomposition this cluster must implement.
- [`docs/obsidian-audit/domains/parsing.md`](../../obsidian-audit/domains/parsing.md) line 337 + §`parseFrontMatterTags` (lines 96-108) — frontmatter tag normalisation, the space-dropping rule, and the case-insensitive `tags` key.
- [`docs/obsidian-audit/domains/search.md`](../../obsidian-audit/domains/search.md) lines 9, 264, 281, 293 — `openGlobalSearch("tag:"+name)`, its 21 Obsidian call sites, and the note that the DSL is out-of-tree (**Corbomite shipped it anyway** — `SearchDSL.cpp`).
- [`docs/obsidian-audit/domains/core.md`](../../obsidian-audit/domains/core.md) §7 line 234 + [`VAULT-FORMAT.md`](../../obsidian-audit/VAULT-FORMAT.md) line 186 — the `tag-pane` internal-plugin id.
- [`docs/audit-2026-04-26/plugin.md`](../../audit-2026-04-26/plugin.md) §"Internal-plugin inventory" line 361 — *"tag-pane | **MISSING** | Tag list/sidebar is implicit; Obsidian has a dedicated panel."*
- [`docs/audit-2026-04-26/metadata.md`](../../audit-2026-04-26/metadata.md) lines 160-172, 236 — subtag-prefix gap, and the `#`-convention disagreement.
- [`docs/superpowers/plans/2026-06-10-road-to-dogfood.md`](2026-06-10-road-to-dogfood.md) `:135` and `:166` — the two orphan mentions this cluster was created for.
- [`docs/PARITY-MATRIX.md`](../../PARITY-MATRIX.md) row 123 — "Tags pane / fullscreen / Mod+1..9 ❌"; split by Q2.6.

## 9. Blocks / enables

- **Blocks:** the roadmap's first release criterion (`:166`) — the last
  unowned line item in it.
- **Enables:** nothing. Fully independent of every other cluster; no substrate
  work, no cross-repo work, nothing waiting on Markoff.
- **Cheap relative to its value:** the whole feature is one widget plus one
  SQL query that Corbomite has never written, on top of five layers that are
  already live and test-covered.
