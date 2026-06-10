# Handoff → Markoff devs: frontmatter closing-fence `\n---` match lacks EOL check — false-close on `----` / `---foo`

**From:** Corbomite (downstream consumer of `Markoff::Parser`)
**Date:** 2026-06-10
**Corbomite branch:** `feature/phase0-data-safety`
**Markoff pin (current):** `ddf5e9a8` (`v0.7.0-freeze-125-gddf5e9a8`)
**Severity (our view):** P1 — silent frontmatter/body split corruption on round-trip for any document whose **frontmatter closing fence** is written malformed (`----`, `--- ` with a trailing space, or `---foo`): the malformed line is accepted as a valid close and the start of the body is mangled.

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

A document whose **intended closing fence is written as `----`,
`--- ` (trailing space), or `---foo`** — i.e. the first/only `\n---`
in the source is malformed — has that line wrongly accepted as a valid
close. A compliant close must be exactly `---` followed by `\n`,
`\r\n`, or EOF. The prefix match accepts the malformed line, then the
`afterFm = endPos + 4` advance (which consumes exactly `\n---` plus an
optional `\r` and an optional `\n`) eats three of the four dashes /
the dashes-plus-marker and mangles the start of the body. On a
Corbomite `withFrontmatter()` round-trip the mangled split is written
back to disk, permanently corrupting the file.

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

## Minimal repros

The bug fires when the **frontmatter's intended closing fence is itself
malformed** — i.e. it is the first (and here only) `\n---` in the
source, but it is written as `----`, `--- ` (trailing space), or
`---foo` rather than an exact `---` line. The prefix match accepts it
as the close; a correct parser must reject it (treating the block as
unterminated → no frontmatter, or continuing the scan). All three
traces below were derived directly from `extract()` (the offsets and
output were checked against the code, not invented).

### Case A — `----` (extra dash)

```markdown
---
title: My Note
----
body text
```

source = `"---\ntitle: My Note\n----\nbody text"`.

`source.indexOf("\n---", 3)` returns **18** — the `\n` immediately
before `----`. The four-dash line is wrongly accepted as the close.
`afterFm = endPos + 4 = 22` consumes `\n---` (the leading `\n` plus
three of the four dashes); `source[22]` is the remaining `-`, which is
neither `\r` nor `\n`, so the advance stops. Result:

- `frontmatter()` → `"title: My Note"` (parses fine — `title` is a
  valid key, so YAML gives no error to flag the problem).
- `markdownContent()` → `"-\nbody text"` — **three of the four dashes
  have been swallowed**; the `----` line is mangled to a lone `-`.

A correct close requires `---` at line-end; `----` must NOT match, so
the block should be treated as unterminated (no frontmatter) and the
whole text kept as body.

### Case B — `--- ` (trailing space) — cleanest trigger

```markdown
---
title: My Note
--- 
body text
```

source = `"---\ntitle: My Note\n--- \nbody text"`.

`indexOf("\n---", 3)` → 18; `afterFm = 22`; `source[22]` is `' '`
(the trailing space), not `\r`/`\n`, so the advance stops there.
Result:

- `frontmatter()` → `"title: My Note"`.
- `markdownContent()` → `" \nbody text"` — the three dashes vanished,
  leaving a stray leading space on the first body line.

### Case C — `---foo` — cleanest trigger

```markdown
---
title: My Note
---foo
body text
```

source = `"---\ntitle: My Note\n---foo\nbody text"`.

`indexOf("\n---", 3)` → 18; `afterFm = 22`; `source[22]` is `'f'`.
Result:

- `frontmatter()` → `"title: My Note"`.
- `markdownContent()` → `"foo\nbody text"` — the dashes are consumed
  and `foo` is welded onto the front of the body.

### Round-trip amplification

Corbomite's link-rewrite path (`FileManager::rewriteLinks`) calls
`withFrontmatter(parsedFrontmatter())`, which prepends a freshly
serialized `---\n…\n---\n` to `markdownContent()`. For Case A, the
file on disk after round-trip becomes:

```markdown
---
title: My Note
---
-
body text
```

The author's intended `----` line is permanently destroyed (reduced to
a lone `-`), and a spurious correct close has been written. The
frontmatter YAML itself is intact, so nothing surfaces the corruption
until the body is inspected.

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

**Validation of the regex.** We checked `\n---(?:\r\n|\n|$)` against the
relevant inputs: it **rejects** `\n----`, `\n--- ` (trailing space),
and `\n---foo` (the trailing `?:\r\n|\n|$` group fails on `-`, ` `, and
`f` respectively), while still **matching** a correct close in all its
forms — `\n---\n`, `\n---\r\n`, and `\n---` at EOF (`$`). The
`afterFm`/`frontmatterBlockEnd` arithmetic downstream of the match is
unchanged because `capturedStart()` returns the position of the `\n`
before the dashes, exactly like `indexOf`'s return value.

## Acceptance oracle (test Markoff can add to `tst_frontmatter.cpp`)

Every case below exercises the bug: the malformed `----` / `--- ` /
`---foo` line is the document's **intended closing fence** (the first
`\n---` in the source). Each assertion **fails at the current pin**
(`ddf5e9a8`) — where the malformed line is wrongly accepted and the
body is mangled — and **passes after the fix**, where a malformed
close leaves the block unterminated so the whole text is body and no
frontmatter is produced.

Note on the API: assertions use `parsedFrontmatter()` (the
YamlValue surface mirrored from the rest of this file) and
`markdownContent()`. We deliberately do **not** assert
`frontmatter()` against a trailing-`\n` literal — `frontmatter()`
returns the raw inner slice with no trailing newline, so that style
of assertion is brittle and unnecessary here.

```cpp
void TestFrontmatter::malformedClosingFenceIsNotAClose()
{
    // Case A: "----" is not a valid close. The block is unterminated,
    // so there is NO frontmatter and the whole text is body.
    // At the current pin this FAILS: "----" is accepted, three dashes
    // are swallowed, and markdownContent() == "-\nbody text".
    {
        auto doc = Document::fromMarkdown(
            QStringLiteral("---\ntitle: My Note\n----\nbody text"));
        QVERIFY(doc->parsedFrontmatter().isNull());          // no frontmatter
        QVERIFY(doc->markdownContent().contains(
            QStringLiteral("----")));                        // intact in body
    }

    // Case B: "--- " (trailing space) is not a valid close.
    // Current pin FAILS: markdownContent() == " \nbody text".
    {
        auto doc = Document::fromMarkdown(
            QStringLiteral("---\ntitle: My Note\n--- \nbody text"));
        QVERIFY(doc->parsedFrontmatter().isNull());
        QVERIFY(doc->markdownContent().contains(QStringLiteral("--- ")));
    }

    // Case C: "---foo" is not a valid close.
    // Current pin FAILS: markdownContent() == "foo\nbody text".
    {
        auto doc = Document::fromMarkdown(
            QStringLiteral("---\ntitle: My Note\n---foo\nbody text"));
        QVERIFY(doc->parsedFrontmatter().isNull());
        QVERIFY(doc->markdownContent().contains(QStringLiteral("---foo")));
    }

    // Case D: round-trip must not destroy the malformed line. Because
    // there is no frontmatter to re-emit, withFrontmatter(parsed) on a
    // null value is the strip path and returns the body unchanged —
    // byte-identical to the source. Current pin FAILS (the "----" line
    // is collapsed to "-" and a spurious correct close is prepended).
    {
        const QString src =
            QStringLiteral("---\ntitle: My Note\n----\nbody text");
        auto doc = Document::fromMarkdown(src);
        QString rt = doc->withFrontmatter(doc->parsedFrontmatter());
        QCOMPARE(rt, src);
    }
}
```

Optional control (passes today, kept only to document the boundary —
it does NOT exercise the bug, so it is not part of the failing oracle):
an exact `---` close still works.

```cpp
    // Control: exact "---" on its own line is a valid close.
    {
        auto doc = Document::fromMarkdown(
            QStringLiteral("---\ntitle: My Note\n---\nbody text"));
        QCOMPARE(doc->parsedFrontmatter().get(
                     QStringLiteral("title")).asString(),
                 QStringLiteral("My Note"));
        QCOMPARE(doc->markdownContent(), QStringLiteral("body text"));
    }
```

## Re-pin coordination

No public API change. `Document::frontmatter()`, `markdownContent()`,
`withFrontmatter()`, and `parsedFrontmatter()` all have unchanged
signatures; only the private matching logic changes. We will update the
Corbomite pin and add a Corbomite-side regression test
(`tst_setfrontmatter` or a new `tst_frontmatter_fence_eol`) that encodes
Case 5 against the live Corbomite `FileManager::rewriteLinks` path.
