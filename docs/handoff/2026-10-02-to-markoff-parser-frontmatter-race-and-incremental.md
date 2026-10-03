# Handoff → markoff-parser devs: a frontmatter cache that can publish an empty result, a stale-CST path, and no incremental parse

**From:** Corbomite (direct consumer of `markoff-parser`)
**Date:** 2026-10-02
**Corbomite HEAD:** `57e79fad`
**Markoff pin:** `5d8850369c91175fe51e3738602d5893a7db21b3`
**Severity (our view):** **P0** for §1 — an intermittent "my properties
vanished". **P0** for §2 — silent wrong offsets rather than an error. §3–§5 are
P1/P2. Nothing here blocks your roadmap.

We consume `Document::fromMarkdown()` from ten places directly — metadata
indexing, link resolution, Bases query parsing, rename rewriting — so the
threading and offset contracts matter to us more than to a pure-editor consumer.

---

## 0. How we verified §1 and §2

Both were read directly at the quoted lines; §1 is a five-line function small
enough to audit by eye. §2's mechanism depends on tree-sitter returning null,
which we have not forced — so §2 is **"reachable but not reproduced"** and we
have marked it accordingly. We would rather flag a possible issue than sit on
it.

---

## 1. (P0) `parsedFrontmatter()` is an unsynchronised cache that can publish an empty result

`src/Document.cpp:382-394`

```cpp
YamlValue Document::parsedFrontmatter() const
{
    if (d->parsedOnce)
        return d->cachedParsed;      // :384

    d->parsedOnce = true;            // :387  flag set FIRST
    const QString raw = d->frontmatter;
    if (raw.isEmpty())
        return {};

    d->cachedParsed = YamlValue::parse(raw, &d->cachedParseError);   // :392  filled 5 lines later
    return d->cachedParsed;
}
```

No mutex, no atomics, on `mutable` state (`Document.cpp:23-25`).

**Two independent defects:**

1. **Data race.** Two threads entering concurrently corrupt
   `cachedParsed`/`cachedParseError`.

2. **A guaranteed lost-result window even *with* a mutex.** `parsedOnce` is set
   at `:387` and the value is not written until `:392`. Any second reader in that
   window takes the `:384` branch and receives a **default-constructed
   `YamlValue`** — i.e. frontmatter appears absent. The mutex would not save you
   here; only the ordering is wrong.

**Why it matters to us:** `Corbomite/libs/storage/src/MetadataParser.cpp:318`
calls this on a worker thread, and `MetadataCache`/`MetadataWorker` share
`Document`s with the UI thread. The user-visible symptom is intermittent,
non-reproducible **"my note properties disappeared"**.

**What we would like:**

```cpp
YamlValue Document::parsedFrontmatter() const
{
    std::call_once(d->parseOnce, [this] {
        d->cachedParseError.clear();
        d->cachedParsed = d->frontmatter.isEmpty()
            ? YamlValue{}
            : YamlValue::parse(d->frontmatter, &d->cachedParseError);
    });
    return d->cachedParsed;
}
```

with `std::once_flag parseOnce;` added to `Private`. The parse moves *inside* the
initializer, so the flag cannot be observed before the value exists.

**Please also document the threading contract in `Document.h`.** It currently
says nothing about it at all — the only mention anywhere is incidental, in
`LinkTextParser.h:13`. We read "unspecified" as "anything goes", and we are
guessing.

**Test:** parse the same `Document` from two threads with a frontmatter-bearing
body; assert both get the same non-empty value. Under TSAN this should currently
report the race.

---

## 2. (P0, reachable-but-not-reproduced) A failed parse leaves the *previous* document's inline trees bound to the *new* byte map

`src/TreeSitterParser.cpp:254-269`

```cpp
if (m_blockTree) ts_tree_delete(m_blockTree);
m_blockTree = ts_parser_parse_string(m_blockParser, nullptr, ...);
if (!m_blockTree)
    return false;                                  // :257  EARLY RETURN
...
for (TSTree *t : m_inlineTrees) ts_tree_delete(t);  // :268  never reached
m_inlineTrees.clear();
m_inlineRanges.clear();
```

On a failed block parse, `m_utf8` and `m_byteToChar` are already the **new**
document's, but `m_inlineTrees`/`m_inlineRanges` still hold the **old**
document's CSTs. `inlineSpansFor` then reads old trees against a new byte map,
producing stale, out-of-range `SourceSpan`s — **wrong highlight ranges, wrong
link/footnote offsets, rather than an error.**

tree-sitter returns null on cancellation, timeout or memory pressure, all of
which are reachable on a large vault file.

**What we would like:** move the inline-tree teardown to the **top** of
`parse()`, before the block parse, and add `bool m_valid` flipped true only on
success, with `inlineSpansFor()`/`buildDocumentQueries()` returning empty when
`!m_valid`. Belt-and-braces: if `m_inlineTrees` is empty after a failed parse,
there is nothing stale to read.

**Test:** parse document A (rich in inline spans), then feed a truncated /
cancelling parse of document B, then call `inlineSpansFor()` and assert empty
rather than A's spans.

---

## 3. (P1) UTF-8 sequence length is inferred from the lead byte alone

`src/TreeSitterParser.cpp:205-225`

```cpp
if (c < 0x80)       seqLen = 1;
else if (c < 0xE0)  seqLen = 2;
else if (c < 0xF0)  seqLen = 3;
else                 seqLen = 4;
```

No validation that the following `seqLen-1` bytes are continuation bytes
(`0x80..0xBF`), and no overlong/surrogate check. A lone `0xC3`, a truncated
sequence, or a Latin-1-encoded byte makes `charIdx` diverge from `QString`'s real
character count — and from there **every** `utf8ToCharOffset()` result
(`:288-292`) is wrong. That function feeds headings, links, tags, footnote refs
and inline spans.

**Why it matters to us:** we parse real user vault files, and Obsidian tolerates
Latin-1-encoded notes. The offset skew is silent: you get wrong positions rather
than an error, which is much harder to notice than a parse failure.

**What we would like:** validate continuation bytes, treat an invalid sequence
as 1 byte, and add a release-build assertion that the final `charIdx ==
text.size()`. **Test:** a Latin-1 fixture and a truncated multibyte sequence.

---

## 4. (P1) Every parse is a whole-document reparse with a fresh 4N-byte map

`src/TreeSitterParser.cpp:247-284`

```cpp
m_utf8 = text.toUtf8();                                  // :248  full re-encode
m_byteToChar = buildByteToCharMap(m_utf8);               // :249  QList<int>, 4*(N+1) bytes
...
m_blockTree = ts_parser_parse_string(m_blockParser, nullptr, ...);   // :256  nullptr = no old tree
```

Passing `nullptr` as the previous tree **disables tree-sitter's incremental
reuse entirely**. And `buildByteToCharMap` zero-fills then overwrites
(`QList<int> map(utf8.size() + 1, 0)` at `:207`), so every byte is written twice.

**Why it matters to us:** `MetadataParser.cpp:301` calls
`Document::fromMarkdown()` once per file; `libs/bases` re-parses on every filter
evaluation; `libs/vault/FileManager` re-parses on link rewrites. So this is
per-keystroke-adjacent for the properties sidebar.

**What we would like:**
- pass `m_blockTree` as the previous tree so tree-sitter reuses subtrees;
- expose `bool reparse(const QString &text, quint32 editOffset, …)`;
- `resize()` instead of zero-fill;
- build the map incrementally from the edit.

---

## 5. (P2) `PerfProbe` costs a mutex and a heap allocation per site, in every build

`include/markoff/parser/PerfProbe.h`

`record()` takes a `QMutexLocker`, then `QString::fromLatin1(name)` — a heap
allocation — into a `QHash<QString, Stat>`. The header states the cost itself:
*"Probe sites are unconditional (cost: one QElapsedTimer + one hash insert) and
benign to leave in non-benchmark builds."*

**They are not benign.** Worse, `MARKOFF_PERF_SCOPE("parser.inline_grammar_one_range")`
sits **inside** the per-inline-region loop (`:279`), so a document with 500
headings costs 500 mutex round-trips per parse — perturbing the very numbers it
exists to measure.

**What we would like:** compile out unless `MARKOFF_PERF` is defined; key the map
on `quintptr` (the string literal's address) instead of `QString` so there is no
allocation on any path.

---

## 6. Test and packaging notes

- **No `LABELS` on the canvas perf tests.** We asked for this on the Canvas side
  earlier (`2026-10-01-to-markoff-frontmatter-undo-and-test-labels.md`, ask 3) and
  it is still outstanding: `ctest -L benchmark` cannot exclude them, so Corbomite
  has to exclude perf tests by **regex on the test name** instead, which is
  fragile and has already let one unrelated timing test through.
- `tests/CMakeLists.txt` does
  `target_include_directories(tst_link_target_decomposition PRIVATE …/../src)`,
  which makes an internal header look like public API and breaks on any
  refactor. Promoting `WikilinkDecomposition.h` to
  `include/markoff/parser/` would fix it. There is also a nested `project()` +
  `enable_testing()` inside `tests/`, unlike the rest of the family.
- **rapidoaml provenance:** `rapidyaml/changelog/` ends at `0.9.0.md` plus
  `current.md`, but the vendored version is 0.11.1 — so two releases of notes are
  missing from the tree and the vendored copy cannot be audited against its own
  changelog. A line in `libs/rapidyaml/README.md` noting the gap would be
  enough.

---

## 7. Suggested triage order

1. **§1** — smallest possible diff (an `std::call_once`), largest user-visible
   payoff, and the TSAN test that proves it.
2. **§2** — also small, and it converts a class of silent wrongness into a
   visible empty result.
3. **§3** and **§4** — independent, and both are about not doing O(document)
   work per call.
4. §5, §6.

If §1's race is already known and deliberately accepted, we would rather know
the reasoning than keep a workaround — but we could not construct a
single-threaded path where the five-line ordering is correct, so we think the
window is real even single-threaded if `parsedOnce` is ever set before the value
is stored elsewhere.