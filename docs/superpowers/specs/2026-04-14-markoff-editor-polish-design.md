# Markoff Editor Polish — Workstream A Design

Five independent low-hanging-fruit improvements to the markoff editor
widget and parser, driven by the Obsidian audit (`docs/obsidian-audit/
01-markoff-gaps.md`). Each item can be implemented and committed
independently. No cross-dependencies between items.

---

## 1. CJK IME Autocorrect

**Gap:** Obsidian auto-replaces full-width CJK brackets with their
markdown equivalents. CJK users whose IME produces full-width characters
by default cannot type wikilinks without manually switching input modes.
(Audit ref: `01-markoff-gaps.md` §Editor, "CJK IME input corners";
§Pass 2 — editor, "CJK autocorrect is just three regexes".)

**Replacements:**

| Input | Output | Purpose |
|-------|--------|---------|
| `【【` | `[[` | Wikilink open |
| `】】` | `]]` | Wikilink close |
| `！【【` | `![[` | Embed open |

**Location:** `MarkdownTextItem::keyPressEvent()`, after `m_control->
processEvent(event)` returns.

**Behavior:**
- After every text-producing key event, read the last 2-3 characters
  before the cursor from the document.
- First-match-wins against the three patterns (longest match first:
  `！【【` before `【【`).
- Replace via `QTextCursor` using `beginEditBlock`/`endEditBlock` so
  the replacement and the original input form one undo unit.
- Only trigger on text-producing events (check `!event->text().isEmpty()`).

**Files touched:** `src/MarkdownTextItem.cpp`.

**Tests:** Unit test in `tests/` that inserts the CJK sequences into a
MarkdownTextItem and verifies the document text contains the replaced
ASCII equivalents.

---

## 2. `parseLinktext` Subpath Extraction

**Gap:** `[[Note#Heading]]` links store the full string `Note#Heading`
as the target. The `#Heading` subpath is never split off, so
scroll-to-heading on link-click cannot work. (Audit ref:
`01-markoff-gaps.md` §Pass 2 — parsing.)

**API:** New free function in `markoff-parser`:

```cpp
// In include/markoff-parser/LinkTextParser.h
namespace Markoff {

struct LinkTarget {
    QString path;       // "Note" or "Note.md"
    QString subpath;    // "#Heading", "#^blockid", or "" if none
};

LinkTarget parseLinktext(const QString &linktext);

} // namespace Markoff
```

**Rules** (matching Obsidian's `parseLinktext.js`):
- First `#` splits path from subpath: `Note#Heading` becomes
  `{"Note", "#Heading"}`.
- `#^blockid` is a block reference (subpath starts with `#^`).
- No `#` in input: subpath is empty.
- `#Heading` alone (empty path): refers to current note's heading.
- Pipe display text (`Note|display`) is already stripped before
  reaching this function (the tree-sitter parser handles that).

**Consumers:**
- `ResourceProvider::resolveLink()` — use path portion for file
  resolution.
- Link-click handler in `Editor` — use subpath for scroll-to-heading.
- `SQLiteIndex` (Corbomite, not markoff) — index the split path.

**Files touched:** New `include/markoff-parser/LinkTextParser.h`,
new `src/LinkTextParser.cpp`, `CMakeLists.txt` (add source).

**Tests:** New `tests/tst_linktext.cpp` covering: `Note#Heading`,
`Note#^blockid`, `Note`, `#Heading`, `#^block`, empty string,
`Note.md#Sub heading with spaces`, path with multiple `#` (only
first splits).

---

## 3. Triple-Click Line-Extend Selection

**Gap:** Triple-click should select the current line and subsequent
drag should extend selection one line at a time. Qt's default
triple-click selects the entire block (paragraph). (Audit ref:
`01-markoff-gaps.md` §Pass 2 — editor, "Triple-click line-extend
selection style".)

**Approach:** Investigate-first.

1. Test current behavior in the test app (triple-click, then drag).
2. If drag-extends by line already: no code change needed (inherited
   from Qt's `QWidgetTextControl`).
3. If drag-extends by character: add a `m_tripleClickActive` flag in
   `TextControl` that switches `mouseMoveEvent` to line-granularity
   selection while the mouse button is held after triple-click. Reset
   on `mouseReleaseEvent`.

**Configurable behavior:** Add to `EditorSettings`:

```cpp
struct EditorSettings {
    // ... existing fields ...
    bool tripleClickSelectsLine = true; // false = Qt default (paragraph)
};
```

Default `true` for Obsidian-compatible behavior. Consumers who want
the Qt default set it to `false`. The `TextControl` reads this flag
via the owning `Editor` to decide selection granularity.

**Files touched:** `include/markoff/EditorSettings.h`,
`src/TextControl.cpp`, `src/TextControl_p.h` (if state flag needed).

**Tests:** Manual verification primary. If code changes are made, add
a test that simulates triple-click + cursor-move and verifies selection
boundaries are line-aligned.

---

## 4. `processLines` Atomic Undo Grouping

**Gap:** Multi-line editor operations (toggle heading on a 5-line
selection, toggle list markers on multiple lines) may create separate
undo entries per line. `Ctrl+Z` then only undoes one line instead of
the entire operation. (Audit ref: `01-markoff-gaps.md` §Pass 2 —
editor, "`processLines` is the single atomic-multi-line-edit
primitive".)

**Approach:** Audit each toggle method in `Editor.cpp` and verify
whether it wraps multi-line edits in `QTextCursor::beginEditBlock()` /
`endEditBlock()`. Any that don't: add the wrapping.

**Methods to audit:**
- `toggleBold`, `toggleItalic`, `toggleStrikethrough`, `toggleInlineCode`
- `increaseHeadingLevel`, `decreaseHeadingLevel`
- `insertBlockQuote`
- `toggleCheckbox`
- `insertCodeBlock`
- Any other method that iterates lines in a selection

**Additional detail from audit:** Obsidian's `processLines` has a
special-case anchor nudge: when the selection starts at column 0 with
a single cursor, the cursor stays at the same absolute character
position after the toggle. This prevents the cursor from jumping when
toggling list markers. Port this behavior.

**Files touched:** `src/Editor.cpp`.

**Tests:** Test that toggling heading level on a 3-line selection, then
`Ctrl+Z`, restores all 3 lines atomically. Test the column-0 anchor
nudge.

---

## 5. YAML Frontmatter Parsing

**Gap:** `Document::frontmatter()` returns raw `QString`. The
properties panel, live-preview properties editor, and plugin
frontmatter access all need parsed key-value data. (Audit ref:
`01-markoff-gaps.md` §Pass 2 — parsing, "No YAML library in
Corbomite"; §Pass 2 — editor-markdown, "`canShowProperties()` false
in raw-source mode only".)

**External dependency:** `yaml-cpp` — mature C++ YAML 1.2 library,
MIT licensed, widely packaged (`pacman -S yaml-cpp` on Arch,
`apt install libyaml-cpp-dev` on Debian/Ubuntu).

**API:**

```cpp
// In include/markoff-parser/Document.h
struct FrontmatterProperty {
    QString key;
    QVariant value;  // QString, QStringList, bool, int, double,
                     // QVariantMap for nested
};

class Document {
    // existing
    QString frontmatter() const;

    // new
    QList<FrontmatterProperty> parsedFrontmatter() const;
};
```

**Design decisions:**
- **`QVariant` for values:** YAML values can be strings, lists,
  booleans, numbers, or nested maps. `QVariant` is Qt's standard
  heterogeneous container and integrates with model/view.
- **Flat `QList` (not `QVariantMap`):** Preserves key ordering, which
  is meaningful to users (title first, tags second, etc.).
- **`yaml-cpp` linked `PRIVATE`:** The public API uses only Qt types.
  No yaml-cpp headers leak into consumers.

**Parsing rules** (matching Obsidian's `parseYaml.js`):
- Strip `---` fences before parsing.
- Top-level must be a YAML mapping. Reject if scalar or sequence
  (return empty list).
- `tags` value: accept YAML list `[a, b]` or comma-separated string
  `"a, b"` — normalize to `QStringList`.
- `aliases` value: same treatment as `tags`.
- Null/missing values: `QVariant()` (invalid/null variant).
- Invalid YAML: return empty list (no crash, no exception).

**Build integration:**
- `find_package(yaml-cpp REQUIRED)` in `libs/markoff-parser/
  CMakeLists.txt`.
- Link as `PRIVATE` — no yaml-cpp in public interface.

**Files touched:** `markoff-parser/CMakeLists.txt`,
`markoff-parser/include/markoff-parser/Document.h`,
`markoff-parser/src/Document.cpp`.

**Tests:** New test cases in existing `tests/tst_document.cpp` or new
file: standard frontmatter (title + tags + aliases), list-style tags,
comma-style tags, empty frontmatter, invalid YAML returns empty list,
nested values, boolean values, numeric values.

---

## Reference Documents

- `docs/obsidian-audit/01-markoff-gaps.md` — gap signals (primary)
- `docs/obsidian-audit/domains/editor.md` — Editor API surface
- `docs/obsidian-audit/domains/editor-markdown.md` — three-mode,
  properties
- `docs/obsidian-audit/domains/rendering.md` — rendering pipeline
- `libs/markoff/docs/rich-element-strategy.md` — existing architecture
- `docs/obsidian-audit/FEATURE-MATRIX.md` — feature status inventory
