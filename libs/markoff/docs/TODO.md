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

## Rendering

- [ ] List bullet rendering in live preview — currently shows raw `-`
  instead of a styled bullet when cursor is elsewhere on the line.

- [ ] Blockquote `>` prefix rendering — could render as left border
  instead of raw `>` character.
