# Sanitiser: Corbomite deliberately does **not** match Obsidian's DOMPurify config

**Date:** 2026-10-03
**Discovered during:** `rendering.md` verification pass against the restored 1.13.7 corpus
**Supersedes / extends:** `domains/rendering.md` §11 (final bullet) and §14 item 1, which
instruct "Corbomite's sanitisation must match this config exactly" and classify a
divergence as "a security-bug class". **This addendum rejects that instruction** for
one specific flag, on security grounds. Extends
`2026-10-03-rendering-sanitiser-policy.md` (which establishes the facts) and
`2026-06-10-…` batches.
**Relevant cluster plans:** none yet — this needs a home; see *Action taken*.

## Finding

### The verified Obsidian behaviour

`app/own/1293-FF.js:2-8` is the entire sanitiser configuration:

```js
// app/own/1293-FF.js:2
var FF = {
  ALLOW_UNKNOWN_PROTOCOLS: !0,
  RETURN_DOM_FRAGMENT: !0,
  FORBID_TAGS: ["style"],
  ADD_TAGS: ["iframe"],
  ADD_ATTR: ["frameborder", "allowfullscreen", "allow", "sandbox", "data-tooltip-position"]
};
```

It is stock DOMPurify 3.0.1 (there is **no** `ALLOWED_TAGS`/`ALLOWED_ATTR`, so the
upstream defaults apply) plus these five keys. `ALLOW_UNKNOWN_PROTOCOLS` is read at
`app/own/1290-LF.js:128` into `X`, and consumed in `_isValidAttribute`:

```js
// app/own/1290-LF.js:239-243
else if ("src" !== t && "xlink:href" !== t && "href" !== t || "script" === e || 0 !== nF(n, "data:") || !he[e]) {
  if (X && !rF(B, tF(n, V, "")));
  else if (n) return !1;
}
```

`B` is DOMPurify's allowed-URI regexp. The guard reads: *if the flag is set and the
attribute value does not match the allowed-URI regexp, **keep the attribute**.* So
`<a href="javascript:…">` is not stripped. This runs on the **main render path**
(`app/own/1087-GT.js:3`, `allowDangerousHtml: true`), not only the plugin-HTML path.

Two further observations from the same pass:

- Three `afterSanitizeAttributes` hooks are registered, **one of them inside a
  constructor** (`app/own/1802-rZ.js:18-21`), so hooks accumulate per instantiation.
- The internal-link anchor is exempted from `nofollow`.

### The decision

**Corbomite will not set `ALLOW_UNKNOWN_PROTOCOLS`.** We keep DOMPurify's URI-scheme
allowlist on, so `javascript:`, `vbscript:`, `data:text/html` and unknown schemes are
removed from `href`/`src`/`xlink:href`. Everything else in `FF` we do match:
`RETURN_DOM_FRAGMENT`, `FORBID_TAGS: ["style"]`, `ADD_TAGS: ["iframe"]`, `ADD_ATTR`.

This is a **knowing, documented divergence**, and `rendering.md` §11's "must match this
config exactly" does not apply to it.

### Why

The instruction to match exactly is sound as a default and wrong as an absolute, for
three reasons:

1. **The threat model is different.** Obsidian's threat model is a plugin or a synced
   note from a semi-trusted vault. Corbomite is GPLv3, reads arbitrary third-party
   vaults, and renders in a desktop webview. Copying a known scheme-check bypass buys
   compatibility nothing observable: no Obsidian feature this affects is one Corbomite
   intends to ship. The divergence costs us a fidelity point on a config nobody diffs
   against a reference implementation.
2. **"Security-bug class" cuts both ways.** §14 calls divergence a security-bug class.
   So does deliberately reproducing an XSS vector in an application that opens files it
   did not author. Where fidelity and security conflict on a *config value* rather than
   on *observable behaviour*, security wins — and the divergence is recorded here so it
   is a decision rather than an oversight.
3. **It is not a format contract.** Nothing in the on-disk vault format depends on it.
   Links Corbomite writes are unaffected; only what it will *render from someone else's
   note* changes.

### What this costs, stated plainly

If a vault relies on a non-http scheme in rendered HTML — `obsidian://`, a custom app
scheme, a `mailto:`-adjacent workflow — Corbomite will strip it where Obsidian would
keep it. If that ever becomes a real complaint, the correct fix is a **narrow,
explicit scheme allowlist** (`ALLOWED_URI_REGEXP` or `ADD_URI_SAFE_ATTR`), not a
blanket re-enable of the bypass. `obsidian://` is the one scheme worth pre-approving,
because it is Obsidian's own and Corbomite may eventually implement a handler for it.

## Why noticed now

The `rendering.md` verification pass read §11's instruction and found it would have
been implemented literally. Both the rendering-domain agent and the plugin-domain agent
flagged it independently and unprompted, the latter noting that "a native ABI inheriting
that policy inherits the `javascript:`-href hole".

## Action taken

- **This is a documentation decision; no code changed** — Corbomite has no
  DOMPurify-equivalent wired to this config yet, so there was nothing to fix.
- `Markoff::sanitizeHtml()` (the Corbomite counterpart named in `rendering.md` §11)
  must **not** accept an allow-unknown-protocols mode. If a flag is ever added, it
  should be `allowUnknownProtocols` and default **off**, with the reasoning here as
  its comment.
- Punched as a P0 so the decision is enforced at implementation time rather than
  rediscovered.
- The remaining `FF` keys are recorded as **match-exactly**, so this addendum narrows
  §11 to one flag instead of discarding it.
- Hook-accumulation (`app/own/1802-rZ.js:18-21`) is noted but not decided: Corbomite's
  hooks will be registered once at plugin load, which is the correct behaviour, so this
  is an Obsidian bug not worth replicating and not worth an addendum of its own.

## Residual risk accepted

A `<a href="javascript:…">` in a note renders as an inert anchor in Corbomite instead of
a live link. That is the intended, documented behaviour. The reverse — a live
`javascript:` href — is what we are refusing to ship.