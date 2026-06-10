# Handoff → Markoff devs: frontmatter closing-fence `\n---` match lacks EOL check — false-close on `----` / `---foo`

**From:** Corbomite (downstream consumer of `Markoff::Parser`)
**Date:** 2026-06-10
**Corbomite branch:** `feature/phase0-data-safety`
**Markoff pin (current):** `ddf5e9a8` (`v0.7.0-freeze-125-gddf5e9a8`)
**Severity (our view):** P1 — silent frontmatter/body split corruption on round-trip for any document whose body contains a horizontal rule or `---`-prefixed line.

---

## TL;DR

`Document::extract()` (the frontmatter parser) searches for the closing
fence using `source.indexOf(QStringLiteral("\n---"), 3)`. The search
string `"\n---"` is a **prefix match only** — it matches any line that
*starts* with `---`, including `----` (a CommonMark thematic break /
setext rule), `---<!-- end -->`, `--- ` (trailing space), etc. The
CommonMark spec and Obsidian both require the closing frontmatter fence
to be **exactly** `---` on its own line (i.e. followed by `\n`, `\r\n`,
or EOF).

A document whose body contains `----` will have its frontmatter
boundary detected at that line. Everything between the opening `---`
and the false-close line is treated as frontmatter YAML; everything
after the false-close is treated as body. On a Corbomite
`withFrontmatter()` round-trip, the split is re-emitted with a correct
closing `---`, permanently corrupting the file.

## Root cause (`Document.cpp`, current pin `ddf5e9a8`)

File: `libs/markoff-parser/src/Document.cpp`

**Line 42 (primary search path):**
```cpp
int endPos = source.indexOf(QStringLiteral("\n---"), 3);
```

**Line 57 (EOF-close fallback):**
```cpp
} else if (source.endsWith(QStringLiteral("\n---"))) {
```

**Line 191 (same pattern in `markdownContent()`):**
```cpp
int endPos = d->source.indexOf(QStringLiteral("\n---"), 3);
```

All three locations match `\n---` without checking the character
*after* the three dashes. A compliant closing fence is `\n---\n`,
`\n---\r\n`, or `\n---` at EOF. Lines like `----`, `--- ` (trailing
space), or `---xyz` must **not** match.

The write-path amplification: `Document::withFrontmatter()` (`:403`)
calls `markdownContent()` (which has the same unfenced search at
`:191`) to get the body slice, then prepends a freshly-serialized
`---\n…\n---\n`. On round-trip, a false-close at `----` is
"corrected" to `---` — the file is silently mutated.

## Minimal repro

```markdown
---
title: My Note
---

Body paragraph.

----

More text.
```

**Expected:** `doc->frontmatter()` == `"title: My Note\n"`, body
starts at `"Body paragraph…"`, `----` is a thematic break in the body
and is completely unaffected by round-trip.

**Actual:** `source.indexOf("\n---", 3)` finds `"\n----"` at the
thematic break. `endPos` is set there. Frontmatter is extracted as:

```
title: My Note\n---\n\nBody paragraph.\n
```

…with the `---` separator and the preceding body fragment now inside
the YAML block. `parsedFrontmatter()` returns parse garbage. `markdownContent()` starts from `"\nMore text."`.

After a `withFrontmatter(parsedFrontmatter())` round-trip (Corbomite's
link-rewrite path via `FileManager::rewriteLinks`), the file on disk
reads:

```markdown
---
title: My Note
---: 

Body paragraph.

More text.
```

…with frontmatter corrupted and the original `----` thematic break
gone.

Second case — `---foo` in body (e.g. a YAML-block marker or just
`---` followed by text):

```markdown
---
tags: [work]
---

---this is not a fence
```

`indexOf("\n---")` matches at `"---this is not a fence"`. The body
fragment `"---this is not a fence"` is absorbed into the YAML block,
producing a parse error or spurious key.

## Suggested fix

After consuming `\n---`, check that the next character is `\r`, `\n`,
or that we are at EOF. The fix is in three places:

**`:42` (primary path in `extract()`):**
```cpp
// Instead of:
int endPos = source.indexOf(QStringLiteral("\n---"), 3);

// Use a loop or QRegularExpression:
static const QRegularExpression closeFence(
    QStringLiteral(R"(\n---(?:\r\n|\n|$))"));
auto m = closeFence.match(source, 3);
int endPos = m.hasMatch() ? m.capturedStart() : -1;
// Note: endPos is the position of the \n before the dashes,
// same semantics as indexOf's return value, so the rest of the
// block (afterFm calculation) is unchanged.
```

**`:57` (EOF-close branch in `extract()`):**
```cpp
// Already correct — endsWith("\n---") with no trailing char
// is an exact fence at EOF. No change needed here.
```

**`:191` (same in `markdownContent()`):**
Apply the same regex approach as `:42`.

A single private helper resolves all three sites:

```cpp
// Returns the position of the '\n' before the closing ---, or -1.
static int findClosingFence(const QString &src, int from = 3) {
    static const QRegularExpression re(
        QStringLiteral(R"(\n---(?:\r\n|\n|$))"));
    auto m = re.match(src, from);
    return m.hasMatch() ? m.capturedStart() : -1;
}
```

Replace both `indexOf(QStringLiteral("\n---"), 3)` call sites with
`findClosingFence(source)` / `findClosingFence(d->source)`.

## Acceptance oracle (test Markoff can add to `tst_frontmatter.cpp`)

```cpp
void TestFrontmatter::closingFenceRequiresExactLine()
{
    // Case 1: thematic break (----) must NOT close frontmatter.
    {
        auto doc = Document::fromMarkdown(
            QStringLiteral("---\ntitle: Note\n---\n\n----\n\nBody."));
        QCOMPARE(doc->frontmatter(), QStringLiteral("title: Note\n"));
        QVERIFY(doc->markdownContent().contains(QStringLiteral("----")));
    }

    // Case 2: ---foo must NOT close frontmatter.
    {
        auto doc = Document::fromMarkdown(
            QStringLiteral("---\ntags: [x]\n---\n\n---foo\n"));
        QCOMPARE(doc->frontmatter(), QStringLiteral("tags: [x]\n"));
        QVERIFY(doc->markdownContent().contains(QStringLiteral("---foo")));
    }

    // Case 3: "--- " (trailing space) must NOT close frontmatter.
    {
        auto doc = Document::fromMarkdown(
            QStringLiteral("---\nk: v\n---\n\n--- trailing\n"));
        QCOMPARE(doc->frontmatter(), QStringLiteral("k: v\n"));
        QVERIFY(doc->markdownContent().contains(QStringLiteral("---")));
    }

    // Case 4: exact closing --- on its own line must still work.
    {
        auto doc = Document::fromMarkdown(
            QStringLiteral("---\nk: v\n---\nBody."));
        QCOMPARE(doc->frontmatter(), QStringLiteral("k: v\n"));
        QCOMPARE(doc->markdownContent(), QStringLiteral("Body."));
    }

    // Case 5: round-trip with thematic break in body must be byte-identical
    // on withFrontmatter().
    {
        const QString src = QStringLiteral(
            "---\ntitle: X\n---\n\nParagraph.\n\n----\n\nMore.\n");
        auto doc = Document::fromMarkdown(src);
        QString rt = doc->withFrontmatter(doc->parsedFrontmatter());
        QCOMPARE(rt, src);
    }
}
```

All five assertions fail at the current pin; they should pass after the
fix.

## Re-pin coordination

No public API change. `Document::frontmatter()`, `markdownContent()`,
`withFrontmatter()`, and `parsedFrontmatter()` all have unchanged
signatures; only the private matching logic changes. We will update the
Corbomite pin and add a Corbomite-side regression test
(`tst_setfrontmatter` or a new `tst_frontmatter_fence_eol`) that encodes
Case 5 against the live Corbomite `FileManager::rewriteLinks` path.
