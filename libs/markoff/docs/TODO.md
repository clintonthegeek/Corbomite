# Markoff TODO

Polish items and known issues to address.

## Performance

- [ ] Incremental tree-sitter parsing — use `ts_tree_edit()` to update
  the old tree instead of full reparse on every keystroke. Tree-sitter
  is designed for this; only the changed region gets re-parsed.
- [ ] Incremental rehighlight — only rehighlight blocks whose spans
  actually changed, not the entire document. Compare old and new span
  maps to find dirty blocks.
- [ ] Reduce spurious `textChanged` signals — `rehighlight()` modifies
  document formatting which fires `contentsChanged` → `textChanged`
  (currently suppressed by `inReparse` guard but still 3 wasted signal
  emissions per keystroke).
- [ ] Consider `QSyntaxHighlighter::rehighlightBlock()` for targeted
  updates instead of full `rehighlight()` after reparse.

## Live Preview Polish

- [ ] Heading hash prefix visibility: when cursor is on a heading line
  but NOT adjacent to the hashes (e.g., at end of heading text), should
  the hashes be visible? Currently they hide. Obsidian shows them for
  the entire line. May want heading prefix to be line-level, not
  element-level.
- [ ] Footnote superscript rendering (QTextCharFormat::AlignSuperScript
  may not work with QPlainTextEdit's simplified layout — may need
  custom painting).

## Style / Theme API

- [ ] Consolidate all hardcoded colors, font families, and sizes into
  a single `MarkoffStyle` struct or similar. Currently scattered across:
  - `MarkdownHighlighter.cpp` (heading colors, link blue, tag orange, etc.)
  - `CodeAtomicBlock.cpp` (background #f5f5f5, border, label color)
  - `CalloutAtomicBlock.cpp` (13 callout type colors)
  - `Renderer.cpp` (HTML CSS colors)
- [ ] Expose a public `setStyle()` / `style()` API on Editor and ReadingView
- [ ] Support KDE color scheme integration (Breeze Dark, etc.)
- [ ] Monospace font family should come from style, not hardcoded
  "JetBrains Mono, Fira Code, monospace"

## Rendering

- [ ] Horizontal rules as actual graphical lines (not just styled `---` text)
- [ ] Task list checkboxes as graphical widgets
- [ ] Blockquote left border (visual indicator beyond just indent + gray text)
- [ ] List bullet rendering — styled bullet character instead of raw `-`

## Parser / Grammar

- [ ] Remove MD4C dependency (currently kept for Renderer/reading view)
- [ ] Migrate Renderer to use tree-sitter CST instead of MD4C AST
- [ ] Add `![[embed]]` syntax to grammar (embed prefix on wikilinks)
- [ ] Add `^block-id` block reference syntax to grammar
- [ ] Add `> [!type]` callout syntax to block grammar
