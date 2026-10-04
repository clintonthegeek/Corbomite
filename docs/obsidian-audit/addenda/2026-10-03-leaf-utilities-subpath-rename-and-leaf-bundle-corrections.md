# Renaming a heading or block id rewrites vault text through a `stripHeading`-match / `stripHeadingForLink`-write asymmetry, and twelve corrections to `leaf-utilities.md`

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/leaf-utilities.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/leaf-utilities.md` §1 (`resolveSubpath`, `stripHeadingForLink`, `requireApiVersion`, `Platform`, `debounce`, `requestUrl`, `logException`), §3, §4, §5, §8, §13 Q2/Q3/Q4/Q5/Q6
**Relevant cluster plans:** `libs/core` subpath resolution and link generation; any Corbomite rename/heading-rename work; `MetadataCache`/`CachedMetadata` shape; embed content extraction

Corpus paths below are relative to `RE/obsidian/1.13.7/renamed/`. Every `app.pretty.js:*` and
`utils/*.js:*` citation in the frozen domain doc is dead — the live symbol names are used here.
`resolveSubpath`'s own three-way dispatch, the `stripHeading`/`stripHeadingForLink` regex bodies and
the block/footnote fall-through are **already covered** by
`2026-10-03-parsing-resolve-subpath-fallthrough.md` and are not repeated here except where a new
consumer contradicts them.

---

## Finding

### NEW 1 — renaming a heading rewrites every matching `#fragment` in the vault, matching with `stripHeading` and writing with `stripHeadingForLink`

The domain doc says `stripHeadingForLink` "must be applied exactly … before appending `#Heading` to a
wikilink" but never says *who* applies it, or that the two strippers are used **asymmetrically in the
same operation**. Obsidian's rename-heading flow does exactly that, in two adjacent lines:

```js
// app/own/1813-CZ.js:1248-1258   (rename heading, from the outline/tab context)
        h = TD(i).toLowerCase(),                       // i = old heading text
        p = DD(c);                                     // c = new heading text
        o.iterateAllRefs(function(e, t) {
          var n = wD(t.link), i = n.path, a = n.subpath;
          a && TD(a.substring(1)).toLowerCase() === h && o.getFirstLinkpathDest(i, e) === r && l.add(e, {
            sourcePath: e,
            reference: t,
            change: JF(t, i + "#" + p)
          });
        });
```

```js
// app/own/1312-hR.js:201-216   (the same algorithm, statement-identical, as RenameHeading.getChanges)
    t.prototype.getChanges = function(e) {
      var t = this.file, n = this.app.metadataCache, i = new _d(),
        r = TD(this.oldHeading).toLowerCase(),           // ← match: stripHeading
        o = DD(e);                                        // ← write:  stripHeadingForLink
      return n.iterateAllRefs(function(e, a) {
        var s = wD(a.link), l = s.path, c = s.subpath;
        c && TD(c.substring(1)).toLowerCase() === r && n.getFirstLinkpathDest(l, e) === t && i.add(e, {
          sourcePath: e, reference: a, change: JF(a, l + "#" + o)
        });
      }), i;
    },
```

Consequences, all disk-visible:

- The **match** side is `stripHeading` (the wide class, `SD`) + lowercase, applied to
  `subpath.substring(1)` — i.e. to the fragment *after* the `#`. The **write** side is
  `stripHeadingForLink` (the narrow class, `xD`). So renaming `## A: B` → `## C|D` rewrites every
  `[[Note#A: B]]` in the vault to `[[Note#C D]]` (`:` and `|` both become spaces under `xD`). And the
  match side is **looser than the text that is ever written**: `TD` also strips `*`, `!`, `[`, `]`,
  `(`, `)`, `#`, backtick and friends, so `[[Note#C*D]]`, `[[Note#C*D]]` lowercased, `[[Note#CD]]`
  and `[[Note#C  D]]` are all the *same* link to this rename pass, while the file on disk only ever
  contains the one spelling `DD` produced. A Corbomite implementation that uses one stripper for both
  directions diverges on exactly the punctuation that differs between `SD` and `xD` — most visibly on
  headings containing `*`, `!`, `[`, `]`, `%`, a backtick or `+`, all of which `xD` keeps and `SD`
  drops. (`#`, `|`, `^`, `\`, CR and LF are stripped by *both*; `xD` additionally strips `%%` and the
  bracket pairs `[[` / `]]`.)
- `getCustomReplacements` (`app/own/1312-hR.js:237-241`) exports the same pair to the non-markdown
  link updaters: `{oldSubpath: TD(this.oldHeading).toLowerCase(), newSubpath: DD(e)}`.
- The ref set is `iterateAllRefs`, which walks **`frontmatterLinks` + `links` + `embeds`** (`CD`,
  `app/own/1115-CD.js:2-4`, driven from `app/own/1306-eR.js:140-152`) — so a rename also rewrites
  `[[…]]` strings sitting in YAML property values. This is the only mechanism in the app that does so.
- The heading text in the note body is replaced by `e.replace(/^(#{1,6} ).*/m, (e, n) => n + t)`
  (`app/own/1312-hR.js:233-236`): the `m`-flagged `^` makes it line-anchored, the ATX marker run and its
  single space are preserved, everything after is replaced, and `replace` being non-global means only
  the **first** matching line in the passed string is touched — which is safe because
  `replaceInEditor`/`replaceInFile` pass exactly one line (`n.getLine(i.line)`).
- `mR.submit` (`app/own/1312-hR.js:134-192`) is the driver, and it is **not** `runAsyncLinkUpdate`:
  `getChanges` → per-source-file `vault.process(file, ZF(text, changes))` (`:161-162`) →
  `replaceInEditor` (`:167`) → `metadataCache.updateInternalLinks(changes)` (`:169`) → a
  `msgUpdatedLinks` Notice with link and file counts (`:171-178`) → **`linkUpdaters[ext].renameSubpath(file, oldSubpath, newSubpath)` on every registered updater** (`:180-186`). The `.canvas`/`.base` subpath references are updated through that last call, keyed by the same `TD`-lowercased old fragment.

### NEW 2 — block-id rename uses a *different* match rule (no stripping at all) and a restricted input charset

```js
// app/own/1312-hR.js:249-268   (RenameBlockid)
    }, t.prototype.validate = function(e) {
      return "" === e ? bd.dialogue.msgBlockIdEmpty() : /^[a-zA-Z0-9\-]+$/.test(e) ? "" : bd.dialogue.msgBlockIdInvalid();
    }, t.prototype.getChanges = function(e) {
      var t = this, n = this.file, i = this.app, r = this.token, o = i.metadataCache, a = new _d(),
        s = "#" + r.text.toLowerCase();
      return o.iterateAllRefs(function(t, i) {
        var r = wD(i.link), l = r.path, c = r.subpath;
        c && c.toLowerCase() === s && o.getFirstLinkpathDest(l, t) === n && a.add(t, {
          sourcePath: t, reference: i, change: JF(i, l + "#^" + e)
        });
      }), a;
    },
```

- **Input charset**: `/^[a-zA-Z0-9\-]+$/` — letters, digits, hyphen (`app/own/1312-hR.js:250`). This is
  the only user-facing constraint on block ids and it is *not* the constraint the parser enforces (see
  `2026-10-03-parsing-tag-and-blockid-grammar.md`), so a hand-written block id can contain characters
  the rename UI refuses to produce — and such an id can never be renamed.
- **Match**: `subpath.toLowerCase() === "#" + id.toLowerCase()`, verbatim. **No `stripHeading`, no
  `stripHeadingForLink`** — unlike the heading path. This is consistent with `CachedMetadata.blocks`
  being keyed on `id.toLowerCase()` (`app/own/1103-uD.js:29,113`), so a block id is compared as a raw
  lowercased token while a heading is compared as a stripped-and-collapsed token.
- **Write**: `"#^" + newId`, and `replaceInEditor` splices only the text *after* the `^`
  (`app/own/1312-hR.js:272` — `n.replaceRange("^" + e, n.start, n.end)`), i.e. the token range excludes
  the caret.

### NEW 3 — HIGH: applying a link fixup re-serialises the whole frontmatter block through js-yaml, and silently drops it on a parse error

`ZF` is the change applier for **every** link-rewrite flow — file rename via `updateInternalLinks`
(`app/own/1306-eR.js:176`), heading and block-id rename via the modal's `submit`
(`app/own/1312-hR.js:162`), and `.canvas` text-node rewrites keyed by the canvas ref-node index
(`app/own/2028-n9.js:498-503`, applied only to nodes with `type === "text"`):

```js
// app/own/1304-ZF.js:2-32
function ZF(e, t) {
  for (var n = [], i = [], r = 0, o = t; r < o.length; r++) {
    var a = o[r];
    if (iD(a.reference)) {
      var s = a.reference.position;
      n.push({ start: s.start.offset, end: s.end.offset, text: a.change });
    } else rD(a.reference) && i.push({ key: a.reference.key, value: a.change });
  }
  if (e = im(e, n), 0 === i.length) return e;
  var l, c = _T(e);
  if (!c.exists) return e;
  try { l = yL(c.frontmatter); } catch (t) { return e; }
  for (var u = 0, h = i; u < h.length; u++) { var p = h[u]; $F(l, p.key.split("."), p.value); }
  var d = bL(l);
  return e.slice(0, c.from) + d + e.slice(c.to);
}
```

- The two ref kinds are discriminated by **shape**, not by a flag: `iD` = `hasOwnProperty("position")`
  (body ref, `app/own/1097-iD.js:2-4`), `rD` = `hasOwnProperty("key")` (frontmatter ref,
  `app/own/1098-rD.js:2-4`). Frontmatter refs carry `key`/`link`/`original`/`displayText` and **no**
  `position` (`app/own/1102-cD.js:11-16,27-32`).
- A frontmatter fixup **re-dumps the entire frontmatter payload**: `yL` (js-yaml load) → `$F` set →
  `bL` (js-yaml dump) → splice over `[c.from, c.to)`. Whatever `bL`'s options are (recorded in
  `2026-10-03-vault-filemanager-content-rewrites.md`: `lineWidth: 0`, `nullStr: ""`,
  `aliasDuplicateObjects: false`) therefore governs the whole block, including properties the rename
  did not touch. The delimiters and everything outside the block survive (`:31`).
- **Failure modes, both silent.** If `yL` throws (`:22-24`) the function returns `e` **after** the body
  splices were already applied at `:17` — so body links are updated and frontmatter links are not, with
  no diagnostic. If `!c.exists` (`:20`) the same asymmetry applies.
- **Dotted frontmatter keys silently no-op.** `cD` builds `key` by joining the object path with `.`
  (`app/own/1102-cD.js:5`), and `$F` walks it with `key.split(".")`:

  ```js
  // app/own/1303-$F.js:2-16
  function $F(e, t, n) {
    for (; e && t.length;) {
      var i = t.shift();
      if (Array.isArray(e)) {
        var r = parseInt(i);
        if (isNaN(r) || r < 0 || r >= e.length) return;
        if (!(t.length > 0)) return void (e[r] = n);
        e = e[r];
      } else {
        if ("object" != typeof e) return;
        if (!(t.length > 0)) return void (e[i] = n);
        e = e[i];
      }
    }
  }
  ```

  A **top-level key that contains a literal dot** (`"my.link": "[[Note#Old]]"` ⇒ `key === "my.link"`)
  makes `$F` walk into `frontmatter["my"]` (`undefined`), at which point the `e &&` guard ends the loop
  with nothing written. The frontmatter link keeps the stale fragment, silently, forever. Corbomite
  must decide whether to reproduce this (faithful) or fix it (divergent) and document the choice.
- `im` (`app/own/0651-im.js:2-11`) sorts splices by `start` **descending** before applying, so offsets
  stay valid.

### NEW 4 — `JF`, the link-text rewriter: alias auto-tracking and display-text rewriting

`JF(reference, newLinktext)` produces the replacement string. The *syntax preservation* half (leading
`!`, `<…>` target form, optional link title, `[]()` fallback) is already recorded in
`2026-10-03-vault-filemanager-content-rewrites.md` §"link rewriting preserves more syntax". The
**semantic** half is not documented anywhere:

```js
// app/own/1305-JF.js:14-30
    if (u) {
      var p = /\\\|/.test(r) ? "\\|" : "|";
      if (l.contains("/")) {
        var d = u.trim();
        Ll(l) === d && (u = Ll(bD(t)));
      }
      i = s + t + p + u + h;
    } else i = s + t + h;
```

- **`|` escaping is preserved from the original**, not re-derived: if the original linktext contained
  `\|` *anywhere*, the rewritten link emits `\|` as the separator, even if the alias itself contains no
  pipe.
- **Alias auto-tracking**: if the link was written with an explicit alias **and** that alias
  (trimmed, `===`, case-sensitive) equals the *written path's* extensionless basename
  (`Ll` = `normalizePath` then drop the last extension, `app/own/0363-Ll.js:2-5`), the alias is
  rewritten to the **new** path's extensionless basename. Any other alias text is preserved verbatim.
  The check is gated on `l.contains("/")`, i.e. only for path-shaped linktext — a
  `[[Note|My Alias]]` in the same folder keeps `My Alias` on rename, while `[[folder/Note|Note]]`
  becomes `[[newfolder/NewName|NewName]]`.
- **A wikilink with no explicit alias is rewritten bare** (`i = s + t + h`), so the display breadcrumb
  is never materialised into the file. Renaming `[[Note#Old]]` → `[[Note#New]]` leaves no trace of the
  old text; renaming `[[folder/Note]]` → `[[New]]` silently changes what the user sees.
- **Markdown-link display text is rewritten only when it equals the old target** (`:26-28`):
  ```js
      var m = f[2], g = Dl(m).trim();
      g === Vl(l = bD(o)) ? m = Vl(bD(t))
        : g.contains("/") && g === Hl(l) && (m = Hl(bD(t)));
  ```
  `Dl` = markdown-unescape-then-NBSP-fold (`Tl = /\u00A0|\u202F/g`, `app/own/0358-Tl.js:2`);
  `Vl` = normalise + drop `.md` (`app/own/0369-Vl.js:2-5`); `Hl` = drop the last extension iff it is
  `md` (`app/own/0370-Hl.js:2-4`). First test: display text equal to the old linkpath *without* `.md`
  ⇒ follow the rename. Second test (only if the display text contains `/`): equal to the old linkpath
  with its extension dropped ⇒ follow the rename. Otherwise the display text is byte-preserved.
- The URL side is `YE(t)` (`app/own/0934-YE.js:2-6`): `t.replace(/[\\\x00\x08\x0B\x0C\x0E-\x1F ]/g, encodeURIComponent)`
  — space becomes **`%20`**, tab/LF/CR are left alone — unless the original target was already in
  `<…>`, in which case it is re-wrapped in angle brackets and **not** encoded (`:24`).

### NEW 5 — `HR`: the six-variant breadcrumb reducer, including an `"alias"` variant that has no subpath

```js
// app/own/1325-HR.js:2-9
function HR(e) {
  var t, n = "", i = "";
  return "block" === e.type && (n = e.file.path, i = "#^" + e.node.id),
    "heading" === e.type && (n = e.file.path, i = "#" + DD(e.heading)),
    "file" !== e.type && "alias" !== e.type || (n = e.file.path, i = ""),
    "linktext" === e.type && (n = (t = wD(e.path)).path, i = t.subpath),
    { path: n, subpath: i };
}
```

Six variants — `block`, `heading`, `file`, `alias`, `linktext`, and anything else (both fields `""`).
Three points a port must not get wrong:

- the `block` variant writes `"#^" + e.node.id` with the **original-cased** id, not a stripped or
  lowercased one, so a breadcrumb's block fragment and `resolveSubpath`'s lookup key differ in case;
- `file` and `alias` share one branch and both yield `subpath === ""` — there is no `[[Note#…]]` form
  for an alias match;
- the `linktext` variant passes the subpath through **verbatim** (`wD(e.path).subpath`, `#` included),
  with no `DD` applied at all.

`DD`'s other call sites confirm the same convention: every `#`-fragment Obsidian *generates* is
`"#" + DD(heading)` (`app/own/2039-vee.js:310`, `1972-Z5.js:2131,2373,2417,2639`, `1328-jR.js:26,35`,
`1332-GR.js:82,130`, `1295-NF.js:25`, `1792-M$.js:230`, `1813-CZ.js:1249`), plus
`getAvailablePathForAttachments` (`app/own/1129-UD.js:575`, re-anchoring the 2026-06-10 §4 citation).
The one exception is the Publish site's "Copy link to heading", which additionally percent-encodes and
turns spaces into `+`: `"#" + k$(DD(k))` where `k$(s) = encodeURIComponent(s).replace(/%20/g, "+")`
(`app/own/1796-K$.js:88`, `app/own/1789-k$.js:2-3`).

### NEW 6 — `ID`: the subpath→content extractor for embeds, documented nowhere

`resolveSubpath` is public and documented; its mandatory second half is not. Every embed of a subpath
calls `ID(text, cache, subpathResult, editable)`:

```js
// app/own/1123-ID.js:2-65  (abridged; full source in the corpus)
function ID(e, t, n, i) {
  void 0 === i && (i = !1);
  var r = "", o = "", a = "", s = "";
  if ("block" === n.type && n.list) { /* walk to the last descendant list item */ }
  else {
    u = n.start.offset, h = n.end ? n.end.offset : e.length;
    var d = u;
    i && "heading" === n.type && (d = n.current.position.end.offset + 1, s = e.substring(u, d)), r = e.substring(0, u), o = e.substring(h), e = e.substring(d, h);
  }
  if ("footnote" === n.type) {
    var f = e.match(/^\[\^[^\]]+\]:\s*/);
    f && (r += f[0], e = e.slice(f[0].length)), e = e.replace(/\n( {4}|\t)/g, "\n");
  }
  return { before: r, after: o, indent: a, heading: s, content: e };
}
```

- **Block-on-a-list-item**: the range is first extended from the item to its **last descendant** (`:9-22`)
  — it advances while the next list item's `parent` is one of the already-collected start lines — then
  widened to whole lines (`start.offset - start.col` … `end.offset`, `:23-30`). The common leading
  indent is computed from the **first line** with tabs expanded to 4 columns, stripped from every line,
  and returned as `indent` (`:34-48`). So an embedded `^block-id` on a list item is rendered
  de-indented and includes the item's children.
- **Heading with `editable = true`**: the heading's own line is split out into `heading`
  (`current.position.end.offset + 1`, `:52`) so the live-preview editor can style it separately; with
  `editable = false` it stays in `content`.
- **Footnote**: the leading `^\[\^[^\]]+\]:\s*` marker is moved into `before` (`:55`), and content is
  dedented by exactly **one** level — `replace(/\n( {4}|\t)/g, "\n")`, which removes four spaces or one
  tab after each newline (`:56`). Two-level indentation is *not* fully removed; tab-vs-4-spaces mixing
  is not normalised.
- `end` being `null` (a heading running to EOF) means `h = text.length` (`:50`).

### NEW 7 — `resolvedLinks` and `unresolvedLinks` both discard the subpath

```js
// app/own/1306-eR.js:403-411   (resolveLinks)
    this.iterateRefsForFile(r, function(r) {
      var o = bD(r.link),
        a = t.getFirstLinkpathDest(o, e);
      if (a) n[a.path] = (n[a.path] || 0) + 1;
      else { var s = KF(o); i[s] = (i[s] || 0) + 1; }
    }), this.resolvedLinks[e] = n, this.unresolvedLinks[e] = i;
```

`bD` cuts at the first `#`, so both key spaces are keyed on the **bare linkpath**. Consequence for
Corbomite's backlinks/graph/orphan logic: `[[Note]]`, `[[Note#Heading]]`, `[[Note#^block]]` and
`[[Note#A#B]]` are **one** entry with count 1, and the counts are *link occurrences*, not distinct
targets. (`2026-10-03-metadata-unresolved-links-key-normalisation.md` quotes this function and pins the
`bD`→`KF` pipeline; the subpath's disappearance is the part it does not draw out.) Note the contrast
with the markdown-render dedup set at `app/own/2037-j9.js:1253`, which keys on
`(resolved ? p.path : KF(path)) + subpath` — i.e. **that** space *retains* the subpath.

### NEW 8 — `Keymap`/`Scope` behaviour the domain doc omits

All in `app/own/0747-ly.js` (`Scope` at `:2-29`, `Keymap` at `:40-139`).

- **`Scope.setTabFocusContainerEl`** (`:16-18`) plus `Keymap.onFocusIn` (`:92-106`) install a focus
  trap: when focus lands outside the scope's `tabFocusContainerEl` and the container does not contain
  `document.activeElement`, Obsidian calls `activeElement.blur()` on a `setTimeout(…, 0)`, re-checking
  that the scope is still current. Corbomite's modal/menu scope push needs the same trap or Tab escapes
  the scope.
- **Per-window scope stacks**: `Keymap` keys a `WeakMap` of `window → {scope, prevScopes}`
  (`:48-53`), and `pushScope`/`popScope` operate on `activeWindow` (`:63-74`), stamping `scope.win`.
  `popScope` is a no-op for the root scope and silently tolerates a scope whose `win` is null.
- **`Ctrl+Shift+I` is forced to register as the `KeyI` vkey** (`:83`):
  `54 === t.which && "^" == t.key && "KeyI" === t.code && (o = "KeyI")`. A binding on `Mod+I`
  therefore also fires on `Mod+Shift+I` on layouts where that combination reports `key === "^"`.
- `isModEvent` returns `"tab"` for middle-click *regardless of modifiers* and `false` for no-modifier
  (`:134-138`) — CONFIRMED against §8.

---

## Corrections to `domains/leaf-utilities.md`

### CORRECTION 1 — `logException` is **not** on the public `obsidian` export table (§1, §13 Q2)

The corpus has **two** webpack export tables. The public one is
`app/own/0025-expressionstatement.js` (158 entries: `App`, `Vault`, `MetadataCache`, `Platform`,
`Keymap`, `Scope`, `SecretStorage`, `request`, `requestUrl`, `moment`, `apiVersion`,
`requireApiVersion`, `debounce`, `stripHeading`, `stripHeadingForLink`, `resolveSubpath`,
`parseLinktext`, `getLinkpath`, `normalizePath`, `parseFrontMatterAliases`, …). It does **not** contain
`logException`.

`logException` is exported from a separate 53-entry CodeMirror-compatibility table
(`app/own/0003-expressionstatement.js`, statement #3) alongside `EditorView`, `Decoration`, `keymap`,
`hoverTooltip`, `showTooltip`, `runScopeHandlers`, `__test`:

```js
// app/own/0003-expressionstatement.js:2,43
n.r(t), n.d(t, {
  …
  logException: () => Ei,
```

So §1's "**Exported as:** `logException`" and §13 Q2's framing ("was it exported by mistake?") both
rest on the extraction artefact that put `logException` in a file labelled
`// public API symbol: logException`. It is **not** importable from `"obsidian"`. Its body is exactly
as §1 describes (`app/own/0185-Ei.js:2-5`, CM6 facet `ci`, else `window.onerror` with a `||` guard
before `console.error`).

### CORRECTION 2 — `SecretStorage` extends `Events` and emits a `"changed"` event (§1 "Mixes in", §4, §5)

§1 says "**Mixes in:** neither `Component` nor `Events`"; §4 says "No events emitted by any domain in
this bundle. None of `utils`, `platform`, `secrets`, or `network` extends `Events` or calls
`.trigger(…)`". Both are wrong for `secrets`:

```js
// app/own/1921-l6.js:434-492  (abridged)
  E6 = function(e) {
    function t(t) { … n.adapter = n.setupStorage(), n; }
    return v(t, e), …                                   // ← extends FD === Events
```

- `setupStorage()` (`:468-469`): `rd.isDesktopApp ? new k6(this.app) : rd.isMobileApp ? new C6(this.app) : null`.
- `trigger("changed")` fires at **three** sites: end of `load()` (`:458`), `setSecret` (`:482`),
  `deleteSecret` (`:490`).
- The one in-app subscriber is the Keychain settings tab, which re-renders its item list on the event
  (`app/own/1921-l6.js:62`: `i.app.secretStorage.on("changed", function() { … })`).

A plugin-visible `app.secretStorage.on("changed", cb)` therefore exists; §10's extension-surface row
should list it.

### CORRECTION 3 — `isEncryptionAvailable()` is `true` whenever the adapter is *not* the Electron one

§1 says "`true` unless the adapter is the Electron adapter (`r0`) and `safeStorage.isEncryptionAvailable()`
returns `false`". The implementation is the short-circuit form:

```js
// app/own/1921-l6.js:466-467
    t.prototype.isEncryptionAvailable = function() {
      return !(this.adapter instanceof k6) || this.adapter.isEncryptionAvailable();
    },
```

so it returns **`true`** when the adapter is `null` (web build, where `setSecret` throws outright) and
**`true`** on Capacitor mobile — `C6` has no `isEncryptionAvailable` method at all
(`app/own/1921-l6.js:390-425`). Only the Electron path can report `false`.

### CORRECTION 4 — the secret storage key names are recoverable, and `save({})` *deletes* the blob (§3, §13 Q4)

§3 says "`t0` is an opaque `localStorage`/Capacitor key … the exact string not derivable from the
extraction". They are plain string literals:

```js
// app/own/1921-l6.js:331-334
  m6 = "secrets-encryption-warning-dismissed",
  g6 = "secrets",
  y6 = "secrets-encrypted",
  b6 = "secrets-meta",
```

(`m6` = the "don't warn again" flag read by `load()` at `:458`; `b6` = `secretsMeta`, written through
`saveMeta = Ml(fn, 5e3)` at `:437-439`; `g6` = the legacy plaintext blob; `y6` = the current blob, used
by **both** adapters — `localStorage` on Electron (`:365`, `:385`) and the Capacitor secure-storage
plugin key on mobile (`:400`, `:420`).) Migration is `w6.prototype.migrate` (`:339-342`): read `g6`,
**clear it**, then re-save under `y6` (`:365-374`).

Two further facts §3 does not have: `save({})` writes `null`, i.e. an empty secret set stores **nothing**
(`:385`, `:420`); and on Electron the blob is `safeStorage.encryptString(json).toString("base64")`, but
**plain `json` (not base64) when encryption is unavailable** — `encrypt` returns its input unchanged
(`:353-354`) and `decrypt` likewise (`:355-358`). The mobile adapter stores and reads raw JSON with no
crypto at all (`:402-403`).

### CORRECTION 5 — `deleteSecret` guards `adapter.save` (§1)

§1's row says `deleteSecret` "Removes from `this.secrets`, calls `adapter.save`, returns `true` if
existed". Reality is `!!Object.hasOwn(this.secrets, e) && (delete this.secrets[e], this.adapter && this.adapter.save(this.secrets), this.trigger("changed"), !0)` (`app/own/1921-l6.js:490`) — the save is guarded by `this.adapter &&`, so a delete on the web build (`adapter === null`) succeeds silently instead of throwing as `setSecret` would. `trigger("changed")` fires only on a successful delete.

### CORRECTION 6 — `requireApiVersion` is a numeric component-wise compare, not lexicographic (§1, §13 Q6)

§1 names the comparator `gy`; §13 Q6 asserts "the implementation is a lexicographic semver-like
comparison" and asks about pre-release suffixes. It is neither: `parseInt` per dot-component, then
integer comparison.

```js
// app/own/2067-Lie.js:2-4
function Lie(e) { return !Uy(Pie, e); }
// app/own/0773-jy.js:2-10   — version parser
function jy(e) {
  if (!e || "string" != typeof e) return null;
  var t = !0, n = e.split(".").map(function(e) { var n = parseInt(e); return isNaN(n) && (t = !1), n; });
  return t ? n : null;
}
// app/own/0774-Uy.js:2-12   — a < b
function Uy(e, t) {
  var n = jy(e), i = jy(t);
  return !!i && (!n || function(e, t) {
    for (var n = Math.min(e.length, t.length), i = 0; i < n; i++) {
      if (e[i] < t[i]) return !0;
      if (e[i] > t[i]) return !1;
    }
    return e.length < t.length || (e.length, t.length, !1);
  }(n, i));
}
```

Answers to Q6, all citable from the above:

- **`1.4.0` vs `1.13.7` compares numerically** (`13 > 4` ⇒ satisfied). A lexicographic compare would have got this wrong.
- **A non-numeric component poisons the whole string**: `requireApiVersion("1.4.x")` ⇒ `jy` returns `null` ⇒ `!!i` is false ⇒ **`false`**.
- **A trailing pre-release suffix is silently absorbed**: `parseInt("0-beta") === 0`, so `"1.4.0-beta"` parses as `[1,4,0]` and is treated as `1.4.0`. No semver precedence exists.
- **A longer minimum is not satisfied by a shorter actual**: `"1.13.7.1"` ⇒ the shared prefix is equal, then `e.length < t.length` ⇒ `3 < 4` ⇒ `true` ⇒ `Lie` returns **`false`**.
- `requireApiVersion("")` or a non-string ⇒ `jy` returns `null` ⇒ `false`.

`apiVersion` in this corpus is `Pie = "1.13.7"` (`app/own/2066-Die.js:582`), with `Iie = "28.2.3"` the
Electron runtime (`:583`) — the doc's `"1.12.7"`/`"28.2.3"` pair is correct for its own 1.12.7 tree and
only the first value moves.

### CORRECTION 7 — `Platform.isMacOS/isWin/isLinux` come from `process.platform`, not `navigator.appVersion` (§1, §8)

§1 annotates them `jl = "macOS" === zl`, `Ul = "Windows" === zl`, `Wl = "Linux" === zl` and its Purpose
paragraph repeats "derived from `navigator.appVersion` at module load time using string inspection
(`zl` derived from `navigator.appVersion`)". §8 repeats it as an invariant. In 1.13.7:

```js
// app/own/0519-Qp.js:2-16
  Qp = function() {
    var e = "undefined" != typeof process ? process.platform : "";
    if ("win32" === e) return "win";
    if ("darwin" === e) return "mac";
    if ("linux" === e) return "linux";
    if (e) return "unknown";
    var t = navigator.userAgent;
    return -1 !== t.indexOf("Win") ? "win" : -1 !== t.indexOf("Mac") ? "mac" : -1 !== t.indexOf("X11") || -1 !== t.indexOf("Linux") ? "linux" : "unknown";
  }(),
  $p = navigator.userAgent.toLowerCase(),
  Zp = "mac" === Qp, Jp = "win" === Qp, ed = "linux" === Qp,
  td = /^((?!chrome|android).)*safari/i.test($p),
```

- Electron takes `process.platform`; the **UA sniff is the web-only fallback** (and it keys on
  `X11`/`Linux`, not `"Windows"`/`"macOS"`/`"Linux"`).
- A `process.platform` that is present but unrecognised (e.g. `"freebsd"`) yields `"unknown"`, so
  **all three of `isMacOS`/`isWin`/`isLinux` are `false`** and Corbomite's `mod-linux` body class is
  never applied (`app/own/2074-expressionstatement.js:5`).
- `isSafari` is a negative-lookahead UA test, so it is `false` on Chrome/Chromium and Android, not
  merely "a UA regex".
- The values are data properties assigned once at object-literal evaluation and never reassigned
  anywhere in `src/` — §8's "immutable after boot" half is **CONFIRMED**.

### CORRECTION 8 — `Platform.resourcePathPrefix` is not hardcoded; boot code overwrites it (§1, §13 Q5)

`resourcePathPrefix: "file:///"` is the literal default (`app/own/0521-rd.js:15`), but on Electron:

```js
// app/own/0579-expressionstatement.js:2-4
  lf(function(e) {
    df = process.versions.electron, ff = parseInt(df.split(".")[0]), rd.resourcePathPrefix = e.ipcRenderer.sendSync("file-url");
  });
```

a **synchronous IPC** to the main process. §13 Q5 asks "is this mutated by boot code for mobile?" —
yes on desktop, via `sendSync("file-url")`, and the returned value is a `file://` UNC form on Windows
(`app/own/2061-oie.js:314`, `app/own/0376-_l.js:356` both special-case `file://` → `"%5C%5C"`). Every
consumer is the "rewrite a `file:///` URL into a resource URL" idiom:
`app/own/2075-Vie.js:1741`, `1967-M5.js:335`, `1956-W3.js:223`, `1649-dK.js:456`, `2061-oie.js:314`,
`2076-expressionstatement.js:89`. The rest of `Platform`'s property list in §1 is **CONFIRMED
verbatim** against `app/own/0521-rd.js:2-44`.

### CORRECTION 9 — `getLanguage`: the supported-locale list is recoverable (§1, §13 Q3)

§1's mechanism is CONFIRMED (`localStorage` first, then `navigator.language` filtered against a
supported list) but §13 Q3 says the list "is not in the extracted file". It is:

```js
// app/own/0522-od.js:2-3
var od = "language",
  ad = "en",
// app/own/0515-_p.js:2-48  — _p = { am, ar, be, bn, ca, cs, da, de, en, "en-GB", es, fa, fi, fr, ga, he,
//   hu, id, it, ja, ka, kh, ko, lv, ms, ne, nl, no, pl, pt, "pt-BR", ro, ru, sk, sq, sr, sv, th, tr,
//   uk, uz, vi, zh, "zh-TW" }   (native display names), Kp = Object.keys(_p).sort()
// app/own/0527-fd.js:2-7
function fd() {
  var e = navigator.language;
  if (Kp.contains(e)) return e;
  var t = e.split("-")[0];
  return Kp.contains(t) ? t : ad;
}
```

- 45 locales; the only multi-part entries are `en-GB`, `pt-BR`, `zh-TW`.
- Resolution order is exact-tag → primary subtag → `"en"`.
- The setting writes **`localStorage["language"]`, but "auto" is stored as an *absent* key**:
  `var t = e === "en" && fd() === "en"; t ? localStorage.removeItem(od) : localStorage.setItem(od, e)`
  (`app/own/0526-dd.js:2-5`), and on desktop it also sends `set-language` over IPC. A Corbomite
  language picker that persists `"en"` explicitly will not round-trip the same `localStorage`.
- The adjacent `ld` list (`0522-od.js:9`: `ar, dv, fa, ff, he, ks, ku, ur, wo, yi`) is the RTL set, not
  the supported set — five of its ten entries are not shippable locales.
- i18next is initialised with `interpolation: {prefix: "{{{{", suffix: "}}}}"`
  (`app/own/0522-od.js:10-15`) specifically so Obsidian's `{{…}}` templating survives translation.

### CORRECTION 10 — `Scope.handleKey`: a *specific* binding that returns `undefined` **swallows** the event (§8)

§8's last `platform/` invariant reads: "`Scope.handleKey` returns `undefined` (falls through to parent)
if the matched handler returns `undefined`; returns the handler's value otherwise." The real control
flow is:

```js
// app/own/0747-ly.js:18-28
  }, e.prototype.handleKey = function(e, t) {
    for (var n = 0, i = this.keys; n < i.length; n++) {
      var r = i[n];
      if (uy.isMatch(r, t)) {
        var o = r.func(e, t);
        if (void 0 !== o) return o;
        if (null !== r.key || null !== r.modifiers) return o;
      }
    }
    if (this.parent) return this.parent.handleKey(e, t);
  }, e;
```

- A handler returning non-`undefined` ⇒ that value is returned (and `false` triggers
  `preventDefault` + `stopPropagation` at `:89`).
- A handler returning `undefined` on a binding with **either** a key or modifiers ⇒ `return o`
  (i.e. `undefined`) **immediately** — remaining bindings in this scope *and* every parent scope are
  skipped. Only a fully wildcard binding (`register(null, null, fn)`, `key === null && modifiers === null`)
  continues iterating and eventually defers to `parent`.
- With no parent, the function returns `undefined` for *any* unmatched key.

This is the mechanism behind modal key capture: a menu that registers `Mod+K` swallows `Mod+K` for the
root scope without calling it. A Corbomite port that "falls through on undefined" will leak every
scoped shortcut into the window.

The rest of §1's `Keymap`/`Scope` list is **CONFIRMED** verbatim: `register` compiles modifiers
(`:9`), `compileModifiers` maps `"Mod"`→`"Meta"` on mac / `"Ctrl"` elsewhere then `.sort().join(",")`
(`:112-115`), `decompileModifiers` inverts and filters empties (`:116-121`), `isModifierKey` accepts
`Control/Alt/Shift/OS/Meta` (`:122-126`), `isModifier` maps `Mod` per-platform (`:128-129`), and
`isMatch` is `(modifiers === null || compiled === info.modifiers) && (!key || key === info.vkey || (info.key && key.toLowerCase() === info.key.toLowerCase()))` (`:130-133`).

### CORRECTION 11 — `debounce`'s window-switch rule is "fire now if overdue", not "clear and reschedule" (§1)

§1: "if the active window changes between calls and the timer hasn't fired, clears the old window's timer
and reschedules on the new one (`utils/debounce.js:38–43`)".

```js
// app/own/0355-Ml.js:22-27
    h = function() {
      for (var e = [], c = 0; c < arguments.length; c++) e[c] = arguments[c];
      r = this, o = e;
      var p = Date.now();
      return i ? n ? a = s = p + t : l !== El() && s <= p && (l.clearTimeout(i), l = El(), i = l.setTimeout(u, 0))
                 : (l = El(), s = p + t, i = l.setTimeout(u, t)),
      h;
    };
```

- `l` is the window the pending timer was scheduled on; `El()` is the current active window
  (`app/own/0354-Cl.js:15-17`: `activeWindow`, else the main window).
- On the non-immediate path with a pending timer, Obsidian only intervenes when **both** the window
  changed **and** the pending call is already overdue (`s <= now`). Then the old timer is cleared and
  the call is re-armed for the **next tick on the new window** — not for a fresh `delay`.
- If the window changed but the call is *not* yet due, nothing happens: the original window keeps the
  timer and the call fires there.
- `immediate = true` is a leading-plus-trailing hybrid, not pure leading edge: it re-arms `a = s = now + delay`
  without rescheduling, and `u` reschedules itself once when it fires before the new due time (`:15-21`).
- `cancel()` returns the debounced function; `run()` returns **the wrapped function's return value** when
  a call was pending and `undefined` otherwise (`:28-32`) — §1's `run(): void` is a slight
  over-narrowing, and §8's "`run()` is a no-op if no call is pending" is **CONFIRMED**.

### CORRECTION 12 — `requestUrl` silently drops `headers` on the web path, and the Electron path is a main-process IPC round-trip (§1, §3, §8)

§3's `RequestUrlParam` documents `headers?: Record<string, string>` without qualification. It is used on
two of three paths only:

```js
// app/own/0766-Ny.js:10-31
      Tf ? (… Pf.requestUrl({ url: e.url, method: e.method, contentType: e.contentType, headers: e.headers, body: t, binary: n }))
         : [3, 2] … sf("electron") ? [4, pf(e)] : [3, 4] … 
      case 4:
        o = e.url, a = e.method, s = e.contentType, l = e.body, c = null,
        s && (c = { "Content-Type": s }),
        [4, fetch(o, { method: a, headers: c, body: l })];
```

On the `fetch` fallback, `headers` is `null` unless `contentType` is set — **`opts.headers` never
reaches `fetch`**. Only `method`, `contentType` (as `Content-Type`) and `body` do.

```js
// app/own/0577-pf.js:7-12
      return n = t.ipcRenderer, [2, new Promise(function(t, i) {
        var r = Gd(16);
        n.once(r, function(e, n) { n.body ? t(n) : i(n.error); }),
        n.send("request-url", r, e);
      })]
```

§1 says the Electron branch "calls `Nf(opts)` — internal Node.js http/https wrapper". It is not
in-process: a 16-character request id is sent over `ipcRenderer.send("request-url", …)` and the reply is
awaited on `ipcRenderer.once(id, …)`, with `result.body` resolving and `result.error` rejecting. The HTTP
work happens in the Electron **main** process. The CORS-bypass conclusion of §8 still holds.

Also new: `requestUrl` attaches its lazy getters with `Object.defineProperty(n, "arrayBuffer"|"json"|"text", {get})`
and **no `enumerable: true`** (`app/own/0767-By.js:5-44`), on the promise object itself — so
`Object.keys(requestUrl(u))` is `[]` and `JSON.stringify` of the pending promise yields `{}`. §1's
description of the lazy getters is otherwise **CONFIRMED**.

### CORRECTION 13 — `window.request` / `window.requestUrl` are globals

`window.request = Hy, window.requestUrl = Vy` (`app/own/0776-expressionstatement.js:2`). Both are
exported from the public table (`app/own/0025-expressionstatement.js:150-151`) *and* installed as
globals. The internal wrapper `zy` (`app/own/0770-zy.js:2-25`) adds a CORS-proxy retry for
`https://github.com/` and `https://raw.githubusercontent.com/` prefixes and rethrows every non-404 —
it is not the exported `requestUrl` and is not plugin-reachable.

---

## CONFIRMED (compact)

| `leaf-utilities.md` claim | 1.13.7 location | Verdict |
|---|---|---|
| `stripHeading` = wide class → space, collapse, trim | `app/own/1118-SD.js:2` (`SD`) + `app/own/1119-TD.js:3` | **CONFIRMED byte-identical** (also in `2026-10-03-parsing-resolve-subpath-fallthrough.md`) |
| `stripHeadingForLink` = narrow class → space | `app/own/1118-SD.js:3` (`xD`) + `app/own/1120-DD.js:3` | **CONFIRMED byte-identical** — note the frozen doc omits that it *also* collapses and trims (`1120-DD.js:3` is `replace(xD," ").replace(/\s+/g," ").trim()`); the parsing addendum has this right |
| `resolveSubpath(cache, subpath)` → `SubpathResult \| null`; three shapes; `end = null` at EOF | `app/own/1122-PD.js:2-64` | **CONFIRMED** (dispatch details in the parsing addendum) |
| `resolveSubpath` is the public `resolveSubpath` | `app/own/0025-expressionstatement.js:153` (`resolveSubpath: () => PD`) | **CONFIRMED** |
| `debounce(fn, delay = 0, immediate = false)`; `cancel()` returns the debounced fn; `run()` no-ops with nothing pending | `app/own/0355-Ml.js:2-33` | **CONFIRMED** (window-switch and `run()` return value: Corrections 11) |
| byte helpers are thin, synchronous, lowercase hex, lossless round-trip | `Td` `app/own/0540-Td.js:2-4`, `xd` `app/own/0539-xd.js:2-5`, `Pd` `app/own/0543-Pd.js:2-8`, `Ad` `app/own/0542-Ad.js:2-5` | **CONFIRMED** |
| `getBlobArrayBuffer` prefers `blob.arrayBuffer()`, falls back to `FileReader` | `app/own/0623-kv.js:2-15` | **CONFIRMED** |
| `moment` is `window.moment` | `app/own/1220-wL.js:2` (`var wL = window.moment`) | **CONFIRMED** |
| `logException(view, error, context)`; CM6 facet → `window.onerror` → `console.error` | `app/own/0185-Ei.js:2-5` | **CONFIRMED as to body**, **but not a public export** (Correction 1) |
| `Platform` property list (7 flags, 4 derived OS flags, 7 getters, 10 device strings) | `app/own/0521-rd.js:2-44` | **CONFIRMED** (derivation: Correction 7) |
| `Keymap.init()`, `compileModifiers` sorts, `decompileModifiers`, `isModifierKey`, `isModifier`, `isMatch`, `isModEvent`, `pushScope`/`popScope`, `getRootScope` | `app/own/0747-ly.js:40-139` | **CONFIRMED**; `pushScope`/`popScope` are **per-window** and `handleKey` fall-through is wrong (Corrections 10, NEW 8) |
| `SecretStorage` id regex `/^[a-z0-9-]+$/` and ≤ 64 chars; `setSecret` throws synchronously on no-adapter or bad id | `app/own/1921-l6.js:477-483` | **CONFIRMED** |
| `getSecret` records access before returning, even for absent keys; `lastAccess` throttled at `> 3e5` ms; `saveMeta` is `debounce(fn, 5000)` | `app/own/1921-l6.js:470-473,437-439,485-486` | **CONFIRMED** — note the gate is strictly `> 300000`, not `>=` |
| `load()` shows a `Notice` when secrets exist and encryption is unavailable, with a dismiss button | `app/own/1921-l6.js:458-462` | **CONFIRMED** |
| adapter selection: Electron / Capacitor / `null` | `app/own/1921-l6.js:468-469` | **CONFIRMED** |
| `RequestUrlResponse` = `{status, headers, arrayBuffer, get json, get text}` via `TextDecoder` | `app/own/0765-Ry.js:5-15` | **CONFIRMED** |
| error type has `.status` and `.headers`; thrown iff `(opts.throw ?? true) && status >= 400`; message `"Request failed, status " + t` | `app/own/0765-Ry.js:4`, `app/own/0764-Fy.js:2-8` | **CONFIRMED** |
| `request(opts)` ≡ `(await requestUrl(opts)).text` | `app/own/0769-Hy.js:7-9` | **CONFIRMED** — it calls the inner `Ny` directly, not `requestUrl` |
| `requestUrl` with `throw = false` never throws | `app/own/0765-Ry.js:4` | **CONFIRMED** |
| §3 "`utils/`, `platform/`, `network` — no on-disk contracts" | — | **CONFIRMED** for these three; `secrets/` has four recoverable `localStorage` keys (Correction 4) |
| §6 "No commands registered in any file in this bundle" | `app/own/0025-expressionstatement.js:1-161`, `app/own/0003-expressionstatement.js:1-54` | **CONFIRMED** — no `registerCommand` in either table |

---

## Why noticed now

`domains/leaf-utilities.md` is the doc that says `resolveSubpath`, `stripHeading` and
`stripHeadingForLink` "are link-format-critical" and tells Corbomite to implement them before any
wikilink navigation works. Reading the corpus to check *where those three functions are actually
called* turned out to be the only way to find the operations that put them on disk: renaming a heading
or a block id. Those flows are not `runAsyncLinkUpdate`, they are not in the domain doc, and they are
not in any existing 2026-10-03 addendum — the vault addendum covers `generateMarkdownLink`
and `JF`'s syntax preservation for *file* renames, and the parsing addendum covers `resolveSubpath`'s
internal dispatch, but the *asymmetric* match/write pair and the js-yaml frontmatter re-serialisation
sit in the gap between them. The same pass through the remaining leaf-utility symbols turned up
thirteen factual errors, five of which (§4's "no events", `logException`'s public status, `Platform`'s
derivation source, `Scope.handleKey`'s fall-through, `requestUrl`'s dropped `headers`) would each have
produced wrong code if trusted.

## Action taken

No Corbomite code touched. Recorded as port contracts:

1. Heading-rename link fixup: match with `stripHeading(fragment).toLowerCase()`, write with
   `"#" + stripHeadingForLink(newHeading)`, over `frontmatterLinks ∪ links ∪ embeds`, restricted to refs
   whose `getFirstLinkpathDest(path, sourcePath) === file`.
2. Block-id rename: match `subpath.toLowerCase() === "#" + id.toLowerCase()` with **no** stripping;
   write `"#" ^ newId`; restrict new ids to `/^[a-zA-Z0-9\-]+$/`.
3. Change application: body refs as offset splices (descending), frontmatter refs as
   `load → set-at-dotted-path → dump → splice`, with the invalid-YAML and dotted-key cases reproduced
   and covered by tests (both are silent, so they need explicit tests).
4. `JF` semantics for Corbomite's own link rewrite: `\|` preservation, basename-alias auto-tracking,
   bare rewrite for alias-less wikilinks, `Vl`/`Hl` display-text rewriting, `YE` encoding vs `<…>`.
5. `ID` as the mandatory second half of `resolveSubpath` for embeds: descendant-list expansion,
   whole-line widening, first-line indent computation with tab→4, footnote marker split, one-level
   footnote dedent, `editable` heading split.
6. `Platform` flags from `QSysInfo::currentPlatform()`-style detection with the same unknown-platform
   outcome (all three false), `isSafari` as a negative-lookahead UA test, and `resourcePathPrefix` as
   a mutable boot-time value.
7. `Scope.handleKey` modelled with the *swallow* semantics: a specific binding returning `undefined`
   must stop the chain, not fall through.
8. `debounce`'s overdue-window-switch rule and the leading+trailing `immediate` mode recorded as
   deviations from a plain `QTimer::singleShot` port.
9. Secret storage: the four key names recorded so a Corbomite wallet shim can round-trip an Obsidian
   install's `localStorage`; the empty-set-deletes-the-blob behaviour recorded so the two agree on
   whether an empty store exists.

Open questions this addendum answers: **§13 Q2** (`logException` is not public), **Q3** (45-locale
list + `"en"` default + `"auto"` stored as an absent key), **Q4** (all four secret key names, plus the
`save({})`-deletes-blob and plaintext-not-base64 behaviours), **Q5** (yes — mutated on desktop via
`sync IPC "file-url"`, not on mobile), **Q6** (numeric per-component compare; pre-release suffixes are
absorbed, non-numeric components poison the string, longer minimums are unsatisfiable).

Unverified / not covered, so nobody assumes otherwise:

- Whether `.canvas` and `.base` `linkUpdaters[ext].renameSubpath` implementations consume
  `oldSubpath`/`newSubpath` the same way `TD`-lowercase/`DD` — not read.
- `Platform.build/manufacturer/model/osName/osVersion/deviceName` writers — not traced.
- The `getSelectedStorageBackend()` → macOS `keychain/` vault-folder read path
  (`app/own/1182-VP.js:45`) — seen, not chased; §3's storage table may be incomplete on macOS.
- `debounce`'s caller list beyond `SecretStorage.saveMeta` — not enumerated.
- Q2's second half ("do community plugins actually use it in CM extensions?") needs the plugin corpus,
  which is not in this repository.