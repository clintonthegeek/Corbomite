# Obsidian Syntax Highlighting — Design Specification

## Overview

Extend the forked `qmarkdowntextedit` MarkdownHighlighter with Obsidian-flavored markdown syntax highlighting. All changes are in `libs/qmarkdowntextedit/markdownhighlighter.h` and `.cpp`.

## Existing Infrastructure

The highlighter already has:
- `WikiLink = 32` and `WikiLinkBroken = 33` enum states (unused — no highlighting code)
- `_formats` static hash for all text formats
- `initTextFormats()` for configuring colors
- `highlightMarkdown()` as the main dispatch
- `highlightInlineRules()` for character-triggered inline patterns
- `highlightAdditionalRules()` for regex-based block rules via `_highlightingRules`
- `initHighlightingRules()` for registering regex rules

## New HighlighterState Values

Add to the enum in `markdownhighlighter.h` after `LinkInternal = 34`:

| State | Value | Purpose |
|-------|-------|---------|
| `ObsidianTag` | 35 | `#tag` and `#nested/tag` |
| `ObsidianCallout` | 36 | `> [!type]` callout type keyword |
| `ObsidianHighlight` | 37 | `==highlighted text==` |
| `ObsidianComment` | 38 | `%%comment text%%` |
| `ObsidianBlockRef` | 39 | `^block-id` at end of line |
| `ObsidianEmbed` | 40 | `![[embed]]` (distinct from wikilink) |

Existing states we use as-is: `WikiLink = 32`, `WikiLinkBroken = 33`.

## Format Configuration

In `initTextFormats()`, add formats for each new state:

| State | Foreground | Background | Weight | Style |
|-------|-----------|-----------|--------|-------|
| `WikiLink` | `#7b6cd9` (purple/accent) | — | Normal | Underline |
| `WikiLinkBroken` | `#888888` (gray) | — | Normal | Underline, dashed |
| `ObsidianEmbed` | `#7b6cd9` (purple/accent) | — | Normal | Underline |
| `ObsidianTag` | `#e06c75` (red/rose) | — | Normal | — |
| `ObsidianCallout` | `#d19a66` (orange) | — | Bold | — |
| `ObsidianHighlight` | inherit | `#fff3b0` (yellow) | — | — |
| `ObsidianComment` | `#5c6370` (gray) | — | Normal | Italic |
| `ObsidianBlockRef` | `#5c6370` (gray) | — | Normal | — |

## Highlighting Methods

### 1. Wikilinks: `highlightObsidianWikiLinks(const QString &text)`

**Patterns:**
- `[[Note Name]]` — basic wikilink
- `[[Note Name|Display Text]]` — aliased wikilink
- `[[Note#Heading]]` — heading link
- `[[Note#^block-id]]` — block link
- `![[Note]]` — embed (note, image, etc.)

**Regex:** `(!?\[\[)([^\]|]+)(\|[^\]]+)?(\]\])`

**Highlighting:**
- `[[` and `]]` brackets: `MaskedSyntax` format (dimmed)
- Target path (group 2): `WikiLink` format
- `|` separator and display text (group 3): display text in `WikiLink`, pipe in `MaskedSyntax`
- `!` prefix for embeds: `ObsidianEmbed` format on the entire match
- Store range as `RangeType::Link` for later use by interactive features (Batch B)

**Called from:** `highlightInlineRules()` — triggered on `[` or `!` characters.

### 2. Tags: `highlightObsidianTags(const QString &text)`

**Pattern:** `#` followed by letters/numbers/underscores/hyphens/slashes, not preceded by `&` (HTML entities) or inside code spans.

**Regex:** `(?<![&\w])#([a-zA-Z_][a-zA-Z0-9_/-]*)`

**Rules:**
- Must start with letter or underscore after `#`
- Can contain: letters, numbers, `-`, `_`, `/`
- Cannot start with a number
- Terminated by space, punctuation (except `-_/`), or end of line
- Must not be inside a code span (check via `isPosInACodeSpan()`)
- Must not be at start of line (that's a heading `# `)

**Highlighting:** Entire match including `#` in `ObsidianTag` format.

**Called from:** `highlightInlineRules()` — triggered on `#` character, after checking it's not a heading.

### 3. Callouts: `highlightObsidianCallouts(const QString &text)`

**Pattern:** `> [!type]` at the start of a blockquote line, optionally followed by `+` or `-` (fold indicator) and title text.

**Regex:** `^>\s*\[!([a-zA-Z]+)\]([+-])?`

**Highlighting:**
- `[!` and `]` delimiters: `MaskedSyntax` format
- Type keyword (group 1: note, tip, warning, etc.): `ObsidianCallout` format
- Fold indicator (`+` or `-`): `MaskedSyntax` format

**Called from:** `highlightMarkdown()` as a standalone call (block-level pattern).

### 4. Highlight: `highlightObsidianHighlight(const QString &text)`

**Pattern:** `==text==` — double equals wrapping highlighted text.

**Regex:** `(==)(.+?)(==)`

**Highlighting:**
- `==` delimiters: `MaskedSyntax` format
- Content between delimiters: `ObsidianHighlight` format (yellow background)
- Must not be inside code span

**Called from:** `highlightInlineRules()` — triggered on `=` character.

### 5. Comments: `highlightObsidianComments(const QString &text)`

**Pattern:** `%%comment text%%` — double percent wrapping hidden text. Can span multiple lines (block state).

**Single-line regex:** `(%%)(.+?)(%%)`

**Multi-line handling:**
- Opening `%%` without closing on same line: set block state to `ObsidianComment`
- Next lines in comment state: format entire line, look for closing `%%`
- Closing `%%`: format up to and including `%%`, reset state

**Highlighting:**
- `%%` delimiters: `MaskedSyntax` format
- Content: `ObsidianComment` format (gray italic)

**Called from:** `highlightMarkdown()` — needs block-state handling like code blocks.

### 6. Block References: `highlightObsidianBlockRef(const QString &text)`

**Pattern:** `^block-id` at the end of a line, preceded by a space.

**Regex:** `\s(\^[a-zA-Z0-9-]+)$`

**Highlighting:** Entire `^block-id` in `ObsidianBlockRef` format (gray, understated).

**Called from:** `highlightMarkdown()` as a standalone call.

## Integration Points in Existing Code

### `highlightMarkdown()` — add calls:
```
highlightObsidianCallouts(text);     // After highlightHeadline()
highlightObsidianComments(text);     // After highlightFrontmatterBlock()
highlightObsidianBlockRef(text);     // After highlightLists()
```

### `highlightInlineRules()` — add character triggers:
```
'[' or '!' → highlightObsidianWikiLinks()
'#'        → highlightObsidianTags()  (after checking not a heading)
'='        → highlightObsidianHighlight()
```

## Testing

Unit tests in `tests/editor/tst_obsidian_highlighting.cpp`:

Attach `MarkdownHighlighter` to a `QTextDocument`, set text, then inspect format ranges on `QTextBlock` via `QTextLayout::formats()`.

**Test cases:**
- `[[Note]]` gets WikiLink format on "Note", MaskedSyntax on brackets
- `[[Note|Display]]` gets WikiLink on both parts, MaskedSyntax on `|`
- `![[image.png]]` gets ObsidianEmbed format
- `#tag` gets ObsidianTag format
- `#nested/tag` gets ObsidianTag format
- `# Heading` does NOT get ObsidianTag (it's a heading)
- `> [!warning] Title` gets ObsidianCallout on "warning"
- `==highlighted==` gets ObsidianHighlight on content, MaskedSyntax on `==`
- `%%comment%%` gets ObsidianComment format
- `^block-id` at end of line gets ObsidianBlockRef format
- Tags inside code spans are NOT highlighted
- Wikilinks inside code spans are NOT highlighted

## What This Does NOT Include

- Wikilink autocomplete popup (Batch B — will use WikiLink ranges for trigger detection)
- Tag autocomplete popup (Batch B)
- Ctrl+Click link navigation (Batch B — will use stored Link ranges)
- Hover preview (Batch B)
- Callout block rendering/folding (future)
- Math/LaTeX highlighting (Phase 4)
- Mermaid highlighting (Phase 4)
