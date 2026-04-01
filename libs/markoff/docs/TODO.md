# Markoff TODO

Polish items and known issues to address.

## Live Preview Polish

- [ ] Suppress rehighlight during mouse drag selection — only update
  delimiter visibility on mouseRelease, not on every mouseMoveEvent.
  Keyboard shift-selection should continue to update in real-time
  (matches Obsidian behavior).

- [ ] Heading hash prefix visibility: when cursor is on a heading line
  but NOT adjacent to the hashes (e.g., at end of heading text), should
  the hashes be visible? Currently they hide. Obsidian shows them for
  the entire line. May want heading prefix to be line-level, not
  element-level.

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
