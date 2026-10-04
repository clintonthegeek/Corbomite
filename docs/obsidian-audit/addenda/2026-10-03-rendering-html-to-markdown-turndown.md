# `htmlToMarkdown`: the full Turndown config, nine custom rules, and output escaping that is switched **off**

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/rendering.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/rendering.md` §1 (`htmlToMarkdown`), §8 invariant 11, §11 row `htmlToMarkdown`, §12 bullet, **§13 Q2** (answered), §14 item 3, §15 (`hP` row)
**Relevant cluster plans:** any plan implementing `Markoff::htmlToMarkdown`; Cluster N (rich clipboard) is merged, so this is prospective — paste-from-browser is still listed as *Missing* in `rendering.md` §11

## Finding

### CORRECTION — the service is `MO`, not `hP`, and it is configured in eight lines

```js
// app/own/1274-IO.js:2-3   (this is htmlToMarkdown)
function IO(e) {
  return MO.turndown(e);
}
```

`§13 Q2` ("Pass 3 grep for `new TurndownService` / `.addRule(`; enumerate GFM extensions enabled, HTML→MD tag mapping, custom rules") is answerable in full:

```js
// app/own/1265-MO.js:2-9
var MO = new window.TurndownService({
  headingStyle: "atx",
  hr: "---",
  bulletListMarker: "-",
  codeBlockStyle: "fenced",
  fence: "```",
  linkStyle: "inlined"
});
```

Turndown itself is a **global** (`window.TurndownService`), not an in-bundle `hP` singleton as §15 states. There is no explicit `gfm` key, so Turndown's default `gfm: true` applies — which means the built-in `strikethrough` and `table` rules are active and are then **both overridden** by Obsidian's own (below).

### NEW — output escaping is disabled outright

The single most consequential line in the whole Turndown block:

```js
// app/own/1275-expressionstatement.js:30-32
MO.escape = function(e) {
  return e;
};
```

Turndown's `escape` is the function applied to every text node before rules run. Replacing it with the identity means **no Markdown metacharacter escaping happens at all**: `[`, `]`, `*`, `_`, `` ` ``, `#`, `|`, `\` pass through verbatim into the produced Markdown.

This directly refutes `rendering.md` §8 invariant 11 — "`htmlToMarkdown` accepts arbitrary (untrusted) HTML; **sanitisation is on the input**. Callers must not double-escape." There is no input sanitisation either (Turndown has none), and there is no output escaping. A `<style>`/`<script>` *element* is dropped by `MO.remove`, but its text content is not present to be escaped; text like `<p>a * b</p>` round-trips to `a * b` and will be re-parsed as emphasis-start by Obsidian's own reader.

`rendering.md` §11 marks `htmlToMarkdown` as *Missing* in Corbomite and "high-priority for web-clipper plugin compat". Whoever implements it must decide deliberately whether to reproduce the no-escape behaviour (faithful) or to escape (safer, divergent).

### NEW — the nine custom rules, verbatim

Group 1 — `app/own/1267-expressionstatement.js:2`, after the removals:

```js
MO.remove(["script", "style", "title"]),
MO.addRule("strikethrough", { filter: ["del", "s"], replacement: function(e) { return "~~" + e + "~~"; } }),
MO.addRule("highlight",     { filter: ["mark"],       replacement: function(e) { return "==" + e + "=="; } }),
MO.addRule("link", { … filter: "inlined" === t.linkStyle && "A" === e.nodeName && !!e.getAttribute("href") … }),
MO.addRule("media", { filter: ["img"], … return i ? "![" + n + "](" + i + r + ")" : ""; });
```

- `highlight` → `==…==` is an **Obsidian-only syntax**, not GFM and not CommonMark. Any HTML→Markdown converter Corbomite ships needs it.
- `link` encodes `href` with `.replace(/ /g,"%20").replace(/([()])/g,"\\$1")` and emits `title` as `" … "` with `"` escaped.
- `media` (`<img>`) emits `![alt](src "title")`, with the same two-step URL encoding, and returns `""` when there is no `src`.

Group 2 — `app/own/1269-expressionstatement.js:2`:

```js
MO.addRule("highlightedCodeBlock", {
  filter: function(e) { var t = e.firstChild; return "DIV" === e.nodeName && xO.test(e.className) && t && "PRE" === t.nodeName; },
  replacement: function(e, t, n) { var i = ((t.className || "").match(xO) || [null, ""])[1]; return "\n\n" + n.fence + i + "\n" + t.firstChild.textContent + "\n" + n.fence + "\n\n"; }
}), MO.addRule("listItem", { … e = e.replace(/^\n+/, "").replace(/\n+$/, "\n").replace(/\n/gm, "\n    "); … }),
MO.addRule("taskListItems", { filter: … "checkbox" === e.type && "LI" === e.parentNode.nodeName, replacement: … t.checked ? "[x] " : "[ ] " }),
MO.addRule("tableCell", { filter: ["th","td"], … });
```

`highlightedCodeBlock` is the **reverse of Obsidian's own reading-view code-block DOM**: it recognises a `<div class="highlight-text-<lang>">` / `highlight-source-<lang>` whose first child is a `<pre>` and converts it back to a fenced block, taking the language from the class name via

```js
// app/own/1268-xO.js:2
var xO = /highlight-(?:text|source)-([a-z0-9]+)/;
```

Note the language capture is **`[a-z0-9]+`** — an uppercase or punctuated language id silently truncates.

`listItem` indents continuations by **four spaces** (`\n` → `\n    `) and honours `start` on ordered lists (`(o ? Number(o) + a : a + 1) + ". "`).

Group 3 — `app/own/1275-expressionstatement.js:2`:

```js
MO.addRule("tableRow", { … }), MO.addRule("table", { … }), MO.addRule("tableSection", { filter: ["thead","tbody","tfoot"], replacement: function(e) { return e; } }),
MO.escape = function(e) { return e; };
```

`tableSection` is a deliberate **no-op passthrough** (the section markup contributes nothing). Header rows and separators are computed by three helpers:

```js
// app/own/1270-TO.js:2-5
var TO = {
  left: ":--",
  right: "--:",
  center: ":-:"
};
```

```js
// app/own/1272-AO.js:2-7
function AO(e) {
  var t = e.getAttribute("colspan");
  if (!t) return 0;
  var n = parseInt(t);
  return isNaN(n) ? 0 : Math.max(0, n - 1);
}
```

```js
// app/own/1273-PO.js:2-4
function PO(e, t) {
  return t.repeat(AO(e));
}
```

`AO` is **colspan − 1**, so a `colspan="3"` cell pads with two extra separator copies. Alignment is emitted only for `align` values `left`/`right`/`center`; everything else (including `colSpan`-only columns) gets `---`.

Header-row detection is stricter than "first row":

```js
// app/own/1271-DO.js:2-10
function DO(e) {
  if (!e) return !1;
  var t,
    n,
    i = e.parentNode;
  return "THEAD" === i.nodeName || i.firstChild === e && ("TABLE" === i.nodeName || (n = (t = i).previousSibling, "TBODY" === t.nodeName && (!n || "THEAD" === n.nodeName && /^\s*$/i.test(n.textContent)))) && Array.prototype.every.call(e.childNodes, function(e) {
    return "TH" === e.nodeName;
  });
}
```

i.e. parent is `THEAD`, **or** it is the first child of `TABLE`/`TBODY` **and every cell is a `TH`**. A synthesised `<tbody>` between an empty `<thead>` and the row is handled.

Two more details worth having:

- `tableCell` escapes pipes and converts newlines: `e.trim().replace(/\|+/g, "\\|").replace(/\n\r?/g, "<br>")` — so multi-line cell content round-trips as a literal `<br>`.
- `table` synthesises a full-width separator when no header row was found: `"|" + "   |".repeat(i) + "\n|" + "---|".repeat(i) + "\n"`.
- `SO` (used by the link/media/tableCell rules) collapses whitespace runs: `e ? e.replace(/(\n+\s*)+/g, "\n") : ""` (`app/own/1266-SO.js:2-4`).

### CONFIRMED

| §claim | verdict | source |
|---|---|---|
| `htmlToMarkdown` is a single-line wrapper around Turndown | confirmed | `1274-IO.js:2-3` |
| "The Turndown rule list is configured outside this directory — §13 filed" | **refuted**; it is `1265`–`1275`, ten statements | `1265-MO.js`, `1267`/`1269`/`1275` |
| Used on paste-from-browser before insertion | consistent — no other consumer of `IO` exists in the corpus | — |

## Why noticed now

Same reason as the sanitiser file: `rendering.md` §13 filed the Turndown rule list as un-extractable and §11 filed the converter itself as *Missing*. Both statements are now checkable, and the escaping override is a behaviour no amount of reading the call sites would reveal.

## Action taken

None yet. Any Corbomite `Markoff::htmlToMarkdown` implementation should start from `1265-MO.js` + the three `addRule` groups + `MO.escape`, and the paste-from-browser path needs a written decision on escaping (faithful = none).

## Unverifiable

- Whether `MO.remove(["script","style","title"])` covers `<noscript>`/`<iframe>`/`<style>` bodies — Turndown's own default rules govern the rest, and Turndown is a *global* (`window.TurndownService`) whose bundle is not in this corpus, so its default rule table cannot be quoted from source here.
- The version of that global Turndown.