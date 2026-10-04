# The sanitiser config is five keys long, it is called `FF` (not `SL`), and it turns the URI-scheme check off

**Date:** 2026-10-03
**Discovered during:** verification pass of `domains/rendering.md` against the restored Obsidian 1.13.7 corpus (`RE/obsidian/1.13.7/`)
**Supersedes / extends:** `domains/rendering.md` §1 (`sanitizeHTMLToDom`), §8 invariant 10, §11 row `sanitizeHTMLToDom`, §12 bullet, **§13 Q1** (answered), §14 item 3, §15 (`EL`/`SL` rows)
**Relevant cluster plans:** any plan implementing `Markoff::sanitizeHtml`; `superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md` (sibling DOM-compat work)

## Finding

### CORRECTION — §13 Q1 is answered, and both of the doc's symbol guesses for it are wrong

`rendering.md` §13 Q1 asks for "the exact DOMPurify `SL` config" and three places (§1 `sanitizeHTMLToDom`, §12, §15) state that the chokepoint is `document.importNode(EL.sanitize(html, SL), true)` with "`EL` is DOMPurify; `SL` is the rule object." Neither name is the DOMPurify pair.

- `sanitizeHTMLToDom` is `RF`, and its second argument is **`FF`**:

```js
// app/own/1294-RF.js:2-3
function RF(e) {
  return document.importNode(LF.sanitize(e, FF), !0);
}
```

- `SL` is the **CLI handler registry** — the object behind `window.handleCli` (`app/own/1225-SL.js:2`, `:31`). It is a name collision, not a config.
- `EL` is a **Levenshtein "did you mean" string helper** (`app/own/1223-EL.js:2`), used at `1225-SL.js:38` to suggest CLI command names.

The DOMPurify instance is the bare bundle binding `LF`, exposed as a global:

```js
// app/own/1290-LF.js:2-7
var LF = function e() {
  var t = arguments.length > 0 && void 0 !== arguments[0] ? arguments[0] : IF(),
    n = function(t) { return e(t); };
  if (n.version = "3.0.1", ...
```

```js
// app/own/1292-expressionstatement.js:2-3
window.DOMPurify = LF, LF.addHook("afterSanitizeAttributes", function(e) { ... });
```

So DOMPurify is **vendored in-bundle at version 3.0.1**, not fetched, and every later hook site in the app calls the *global* `DOMPurify.addHook(...)` (see `1802-rZ.js:18`, `2072-expressionstatement.js:2`).

### NEW — the entire config, verbatim, is eight lines

```js
// app/own/1293-FF.js:2-8
var FF = {
  ALLOW_UNKNOWN_PROTOCOLS: !0,
  RETURN_DOM_FRAGMENT: !0,
  FORBID_TAGS: ["style"],
  ADD_TAGS: ["iframe"],
  ADD_ATTR: ["frameborder", "allowfullscreen", "allow", "sandbox", "data-tooltip-position"]
};
```

Five keys. Consequences an implementer needs:

| key | effect in the vendored 3.0.1 source |
|---|---|
| `ALLOW_UNKNOWN_PROTOCOLS: true` | sets the internal `X` flag (`1290-LF.js:128`), which short-circuits the `IS_ALLOWED_URI` branch at `1290-LF.js:241`. **The URI-scheme allowlist regexp is never consulted.** `href="javascript:…"` / `data:` is *not* removed by DOMPurify. |
| `RETURN_DOM_FRAGMENT: true` | `1290-LF.js:128` forces `RETURN_DOM` on as a side effect (`ne && (te = !0)`), which is what makes `document.importNode(fragment, true)` at `1294-RF.js:3` legal. |
| `FORBID_TAGS: ["style"]` | `<style>` elements are removed; children are kept (`KEEP_CONTENT` defaults true, `1290-LF.js:105`). |
| `ADD_TAGS: ["iframe"]` | `<iframe>` becomes allowed (it is in the default `FORBID_CONTENTS` set, `1290-LF.js:109`). |
| `ADD_ATTR: [...]` | merged into the **global** allowed-attribute set, so `frameborder`/`allowfullscreen`/`allow`/`sandbox`/`data-tooltip-position` are legal **on every element**, not only on iframes. |

**There is no `ALLOWED_TAGS` or `ALLOWED_ATTR` key**, so the stock DOMPurify 3.0.1 allowlists apply unmodified. `DOMPurify.setConfig` (`1290-LF.js:324`) is never called for this instance — the only `setConfig` hits in the corpus belong to unrelated subsystems. So there is no Obsidian-authored tag/attribute allowlist to port; a Corbomite sanitiser needs DOMPurify's own defaults plus these five keys.

### NEW — three `afterSanitizeAttributes` hooks, two of them with different iframe policies

**1. Global baseline** — `app/own/1292-expressionstatement.js:2-3`:

```js
LF.addHook("afterSanitizeAttributes", function(e) {
  e.instanceOf(HTMLAnchorElement) && (e.setAttribute("target", "_blank"), e.hasAttribute("rel") || e.setAttribute("rel", "noopener nofollow"));
});
```

Every anchor in sanitised HTML gets `target="_blank"` unconditionally; `rel="noopener nofollow"` only if `rel` is absent.

**2. Per-view hook, registered inside a constructor** — `app/own/1802-rZ.js:9-21`:

```js
var n = {
  "allow-downloads": !0, "allow-forms": !0, "allow-modals": !0, "allow-orientation-lock": !0,
  "allow-pointer-lock": !0, "allow-popups": !0, "allow-popups-to-escape-sandbox": !0,
  "allow-presentation": !0, "allow-same-origin": !0, "allow-scripts": !0
};
DOMPurify.addHook("afterSanitizeAttributes", function(e) {
  if (e.instanceOf(HTMLIFrameElement) && (e.hasAttribute("sandbox") ? OF(e, "sandbox", " ", n) : e.setAttribute("sandbox", "allow-forms allow-modals allow-presentation allow-popups allow-same-origin allow-scripts")), e.instanceOf(HTMLAnchorElement)) {
    var i = e.getAttribute("rel") || "";
    i.split(" ").contains("nofollow") || (i += " nofollow"),
    e.hasClass("internal-link") && e.hasAttribute("data-href") && e.href.startsWith(t.site.pathprefix) && (i = i.replace("nofollow", "").trim()),
    e.setAttribute("rel", i);
  }
})
```

- If an `<iframe>` has **no** `sandbox`, the default is the **six-token** list (no `allow-downloads`, no pointer-lock/orientation-lock, no `allow-popups-to-escape-sandbox`).
- If it **has** one, the existing token list is *filtered* against the ten-token `n`.
- Anchors: `nofollow` is appended if missing; and an `a.internal-link[data-href]` whose `href` starts with `this.site.pathprefix` has `nofollow` **removed** — i.e. internal links in sanitised HTML (plugin-HTML, web-clipper output) are exempted from `nofollow`.
- **This hook is registered in the class constructor**, so every instantiation adds another hook to DOMPurify's global hook list. That is an unbounded-accumulation leak in Obsidian itself; Corbomite should register once.

**3. Global iframe/`allow` hook** — `app/own/2072-expressionstatement.js:2`:

```js
DOMPurify.addHook("afterSanitizeAttributes", function(e) {
  e.instanceOf(HTMLIFrameElement) && (e.hasAttribute("sandbox") ? OF(e, "sandbox", " ", Bie) : e.setAttribute("sandbox", "allow-forms allow-presentation allow-same-origin allow-scripts allow-modals")), e.hasAttribute("allow") && OF(e, "allow", ";", Nie));
});
```

Note this one uses a **different, five-token** sandbox allowlist and **no `allow-popups`** — and it filters `allow` with a **`;`** separator, not a space. The two allowlists:

```js
// app/own/2071-Rie.js:3-19
var Rie,
  Nie = { "encrypted-media": !0, fullscreen: !0, gamepad: !0, hid: !0, "idle-detection": !0,
          "oversized-images": !0, "picture-in-picture": !0, "sync-xhr": !0 },
  Bie = { "allow-forms": !0, "allow-modals": !0, "allow-popups": !0,
          "allow-presentation": !0, "allow-same-origin": !0, "allow-scripts": !0 };
```

(`Nie` deliberately omits `autoplay`; `Bie` is the sandbox-token set even though hook 3 compares with `;` — `OF` splits on the passed separator, so a space-separated `sandbox` attribute would not be filtered by hook 3.)

The filter itself:

```js
// app/own/1291-OF.js:2-9
function OF(e, t, n, i) {
  for (var r = e.getAttribute(t), o = [], a = 0, s = r.split(n); a < s.length; a++) {
    var l = s[a];
    l = l.trim().toLowerCase(), i.hasOwnProperty(l) && i[l] && o.push(l);
  }
  var c = o.join(n);
  c !== r && e.setAttribute(t, c);
}
```

Unknown tokens are **dropped**, known ones lowercased and re-joined with the same separator. Corbomite needs this if it ever sanitises untrusted iframe markup.

### NEW — one more live consumer of the sanitiser chokepoint

Callout icons can be inline SVG supplied by a theme or a note, and that SVG goes through `RF`:

```js
// app/own/1525-WW.js:6-16  (WW = resolve callout icon)
if (t.startsWith("<svg")) {
  var i = RF(t).firstChild;
  i && i.instanceOf(SVGSVGElement) && (i.setAttrs({ width: 16, height: 16, fill: "currentColor", stroke: "currentColor" }), e.empty(), e.appendChild(i));
} else Ag(e, t);
```

So `--callout-icon: '<svg …>'` is a real, supported value, it is sanitised, and it is forced to 16×16 / `currentColor`. See the callout addendum for the full three-form contract.

### CONFIRMED

| §claim | verdict | source |
|---|---|---|
| `sanitizeHTMLToDom` returns a `DocumentFragment` deep-`importNode`'d into the current `document` (invariant 10) | confirmed, and now with the config | `1294-RF.js:3`, `1293-FF.js:4` |
| "The `EL`/`SL` configuration (allowlist of tags + attrs) lives in the bundle outside this directory and could not be inspected" | **refuted** — it is five keys and was always in the corpus | `1293-FF.js:2-8` |
| §12 "Security-boundary critical: Corbomite's sanitisation must match this config exactly" | still true, and the config is now known | — |

## Why noticed now

`rendering.md` §13 filed the DOMPurify config as the single highest-value un-extractable fact in the domain ("Pass 3 must extract the DOMPurify `SL` allowlist … A sanitiser-allowlist divergence is a security-bug class"). Restoring the 1.13.7 corpus made it a 30-second lookup, and the answer invalidates the doc's premise that Obsidian supplies an allowlist at all.

## Action taken

None yet — this is a verification pass. Corbomite's `sanitizeHtml` (still listed as *Partial* in `rendering.md` §11) should be implemented as DOMPurify-equivalent defaults plus the five `FF` keys plus the three hooks, with hook 2 registered exactly once rather than per view. The `ALLOW_UNKNOWN_PROTOCOLS: true` result needs a deliberate decision: matching Obsidian exactly means Corbomite's sanitiser will also not block `javascript:` hrefs, and the anchor `target`/`rel` rewriting plus the internal-link `nofollow` exemption have to be reproduced or external links in plugin-supplied HTML will behave differently.

## Unverifiable

- Whether DOMPurify 3.0.1's *default* allowlists are what a Corbomite port should use, or whether Corbomite should tighten them. Obsidian chose not to; that is a fact, not a recommendation.
- The exact class of the view at `1802-rZ.js` — its `DOMPurify.addHook` is in the constructor and its `site.pathprefix` use identifies it as a SITES-aware view, but the readable corpus does not name it. The hook behaviour itself is verbatim above regardless.