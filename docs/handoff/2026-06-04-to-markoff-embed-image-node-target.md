# Handoff → Markoff devs: `![[…]]` embeds routed through the tree-sitter `image` node lose their target

**From:** Corbomite (downstream consumer of `Markoff::Parser`)
**Date:** 2026-06-04
**Corbomite branch:** `master`
**Markoff pin (current):** `ddf5e9a8` (`v0.7.0-freeze-125-gddf5e9a8`)
**Repro confirmed on:** the current pin and the prior pin (`8db7c5a9`) — pre-existing, not introduced by the recent parser commits (`a0d8f5b6`, `3b0ca22e`).

## TL;DR

`TreeSitterParser::extractLinkFromNode`'s **`image`-node path** is incomplete relative
to its `wiki_link` and `inline_link` siblings. When tree-sitter parses an Obsidian
wiki-embed `![[Target]]` as an `image` node (it does this for several shapes, e.g.
extension-less `![[Note]]` and `![[Image.png]]`), the resulting `LinkInfo` comes back
with an **empty `target`** and a **bracket-wrapped `displayText`** (`"[Target]"`),
instead of a clean embed (`target="Target"`). Downstream that produces a corrupt
`original` string and breaks embed resolution. Please normalize the `![[…]]` shape in
the image path the same way the `wiki_link` path already does.

## Repro

```cpp
Markoff::Document doc = Markoff::Document::fromMarkdown("![[Image.png]]\n");
const Markoff::LinkInfo &l = doc.links().at(0);
// Observed:
//   l.type        == LinkInfo::Image
//   l.target      == ""              ← empty
//   l.displayText == "[Image.png]"   ← still bracket-wrapped
// Expected (matching the wiki_link path's handling of "![[…]]"):
//   l.type        == LinkInfo::Embed
//   l.target      == "Image.png"
//   l.displayText == ""   (or "Image.png"; see "Open question" below)
```

Same failure for `![[Note]]` (no extension) and any `![[…]]` whose grammar match lands
on `image` rather than `wiki_link`.

## Root cause (current pin, `ddf5e9a8`)

`libs/markoff-parser/src/TreeSitterParser.cpp`, `extractLinkFromNode`:

- **`wiki_link` path (≈925-944)** — correct. Detects the embed via
  `raw.startsWith("![[")`, sets `type = Embed`, strips `![[` … `]]`, splits on `|`,
  and runs `decomposeWikilinkInner`.
- **`inline_link` path (≈946-973)** — defensive. Strips surrounding `[`…`]` from
  `link_text` (`:963-964`) **and** falls back `target = displayText` when
  `link_destination` is empty (`:970-971`).
- **`image` path (≈975-994)** — does **neither**. It copies `image_description`
  verbatim into `displayText` (so the `[`…`]` brackets survive) and never falls back
  `target`, so an `![[…]]` whose `link_destination` is empty yields `target=""`,
  `displayText="[…]"`. It also doesn't detect the `![[…]]` raw shape at all.

The node's raw bytes are right there (`ts_node_start_byte`/`end_byte` + `utf8`), so the
same `raw.startsWith("![[")` test the `wiki_link` path uses is available here.

## Downstream symptom (why this matters to us)

Corbomite's `MetadataParser` (`libs/storage/src/MetadataParser.cpp:373-377`) builds an
`original` string as `target + "|" + displayText` when `displayText` is non-empty and
differs from `target`. With `target=""` and `displayText="[Image.png]"` that yields:

```
original == "|[Image.png]"
```

`MetadataCache::drainOnePath` keys embed resolution off `original`/`link`, so the embed
silently fails to resolve. This is graph/backlink correctness, not cosmetic.

## The ask

In the `image` path of `extractLinkFromNode`, detect the wiki-embed shape from the raw
node bytes and route it through the same extraction the `wiki_link` path uses. Sketch
(names/shape your call):

```cpp
if (strcmp(type, "image") == 0) {
    int sb = ts_node_start_byte(node), eb = ts_node_end_byte(node);
    QString raw = QString::fromUtf8(utf8.mid(sb, eb - sb));
    out.sourceOffset = sb;
    out.sourceLength = eb - sb;

    if (raw.startsWith(QStringLiteral("![["))) {        // Obsidian wiki-embed
        out.type = LinkInfo::Embed;
        QString inner = raw.mid(3, raw.size() - 5);     // strip "![[" … "]]"
        const int pipe = inner.indexOf(QLatin1Char('|'));
        if (pipe >= 0) { out.target = inner.left(pipe); out.displayText = inner.mid(pipe + 1); }
        else           { out.target = inner; out.displayText.clear(); }
        out.structured = Markoff::Detail::decomposeWikilinkInner(QStringView{inner});
        return true;
    }
    // …existing standard-markdown-image handling unchanged…
}
```

Factoring the `wiki_link` body into a shared `extractWikiEmbed(raw, out)` helper and
calling it from both paths would be cleaner than duplicating, but either works.

## Open question for you (display-text convention)

The `wiki_link` path currently sets `displayText = inner` (== target) for the
alias-less case, and relies on consumers comparing `displayText != target`. The
`inline_link` path leaves `displayText` empty for the alias-less case. We don't care
which convention wins as long as it's **consistent for `![[…]]` across the `wiki_link`
and `image` paths** — today an alias-less `![[X]]` gives `displayText == "X"` via
`wiki_link` but `displayText == "[X]"` via `image`. Pick one; our `MetadataParser`
already tolerates `displayText == target` (treats it as "no alias").

## Falsifiable acceptance

1. `Document::fromMarkdown("![[Image.png]]\n").links().at(0)` →
   `type == Embed`, `target == "Image.png"`, and `displayText` is either empty or
   `"Image.png"` (no surrounding brackets).
2. `![[Note]]` (extension-less) → `target == "Note"`, same bracket-free guarantee.
3. `![[Target|Alias]]` → `target == "Target"`, `displayText == "Alias"`.
4. Existing standard markdown images `![alt](path.png)` are unchanged.

The two Corbomite-side oracles already encode (1)-(3):
`tests/storage/tst_metadataparser.cpp::testParseEmbedVsLink` and
`::testParseEmbedAsImageNode`. They are **currently red** and will go green when we
re-pin to the fixed Markoff commit. We're deliberately leaving them red (not working
around the quirk in `MetadataParser`) so they serve as the cross-repo acceptance signal
— per your "tests define expected behavior" convention, the normalization belongs at the
parser layer that owns the tree-sitter quirk, not in every consumer.

## Re-pin coordination

Land behind a tag if you can (`v0.7.0-…`) and point us at it; otherwise we'll pin to the
fixing master commit. We'll run the two `tst_metadataparser` oracles against the new pin
as the smoke check. Small surface, no public-API change expected — `LinkInfo`'s shape is
unchanged, only the values it carries for this one node shape.
