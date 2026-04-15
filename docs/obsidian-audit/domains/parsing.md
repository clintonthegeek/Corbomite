# `obsidian/parsing` — frontmatter + YAML + linktext

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/parsing/`
**File count:** 8
**Files:** `getFrontMatterInfo.js`, `parseFrontMatterAliases.js`, `parseFrontMatterEntry.js`, `parseFrontMatterStringArray.js`, `parseFrontMatterTags.js`, `parsePropertyId.js`, `parseYaml.js`, `stringifyYaml.js`

**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> Vault-format-critical text parsers. `getFrontMatterInfo(text)` returns the `---`-delimited frontmatter range. The `parseFrontMatter*` family extracts typed values (aliases, tags, generic entries, string-arrays). `parseYaml`/`stringifyYaml` wraps the underlying YAML library. `parsePropertyId` normalises a frontmatter key to its display id.

---

**De-minifier artifact note:** All 8 files have distinct `md5sum` hashes — no duplicate extraction. Each file carries a `// public API symbol:` and `// source: app.js lines <a>-<b>` header comment, and every file contains exactly its declared symbol with no bleed-over. `parsePropertyId` is sourced from a much later line range (133477–133494) than the other seven (79235–79334), reflecting its late addition (Bases feature). The domain has no de-minifier artifacts to discard.

**Cross-domain location note:** `parseLinktext` is declared in the vault domain's source range (`app.js:46369`, surfaced in `vault/parseLinktext.js` with `// public API symbol: parseLinktext`). It is audited in `domains/vault.md`. This document captures its semantics for completeness, because it is a compat-critical parser that the parsing domain's sibling functions depend on conceptually, and because the instructions name it a primary priority.

`resolveSubpath`, `stripHeading`, and `stripHeadingForLink` live in `obsidian/utils/` (`utils/resolveSubpath.js`, `utils/stripHeading.js`, `utils/stripHeadingForLink.js`) — not inside `obsidian/parsing/`. They are fully audited in this document under their respective sections (Priority 5) because the task instructions require it and they are tightly coupled to the frontmatter/link-parsing domain.

---

## 1. Public API surface

### `getFrontMatterInfo(text: string)`

- **Kind:** function
- **Exported as:** `getFrontMatterInfo`
- **Signature:** `(text: string) => { exists: boolean; frontmatter: string; from: number; to: number; contentStart: number }`
- **Source:** `parsing/getFrontMatterInfo.js:5`; inner regexes at `app.renamed.js:45995–45996`
- **Purpose:** Detects and locates the YAML frontmatter block in a markdown file's full text. Finds the opening `---(\r?\n)` (regex `Yx = /^---(\r?\n)/g` with sticky search from offset 0), then scans for the closing `---(\r?\n|$)` (regex `Qx = /---(\r?\n|$)/g`), requiring that the character immediately before the match is a newline (i.e., the closing `---` must begin at a line start). Returns an object with:
  - `exists`: `true` iff both opening and closing delimiters are found.
  - `frontmatter`: the text between the two `---` lines (exclusive of both delimiters and their newlines).
  - `from`: byte offset of `frontmatter[0]` inside `text` (the first char after the opening `---\n`).
  - `to`: byte offset of the closing `---` (i.e., `text.slice(from, to)` is the raw YAML).
  - `contentStart`: byte offset of the first char after the closing `---\n` (where the note body begins).
- **Invariant:** When `exists === false`, all numeric fields are `0` and `frontmatter` is `""`.
- **Corner cases:** The closing `---` regex is greedy with `lastIndex`; it will skip any `---` sequence that is not preceded by a newline character (e.g., `---` inside a YAML value on the same line is safe). The opening regex requires the very first characters to be `---\n` or `---\r\n`; a file beginning with a BOM or any whitespace has `exists === false`.
- **Mixes in:** N/A (pure function)

---

### `parseYaml(text: string)`

- **Kind:** function
- **Exported as:** `parseYaml`
- **Source:** `parsing/parseYaml.js:5`. Delegates to `xx(text, null, {})`.
- **Purpose:** Parses a YAML string into a JavaScript value. `xx` (at `app.renamed.js:45024`) is the `parse()` entry-point from the bundled **`yaml`** npm package (eemeli/yaml v2; identified by `Symbol.for("yaml.alias")`, `Symbol.for("yaml.document")`, etc. at `app.renamed.js:40676–40682`; module ID `n(2292)` in the webpack chunk). Options passed: empty object `{}` — all defaults, no custom schema, no `reviver`. Returns `null` for empty/null input; throws on parse errors in non-silent mode.
- **YAML version:** defaults to YAML 1.2 (see `app.renamed.js:40874`, `40845`). `yaml-1.1` / `yaml11` schema is available but not used for frontmatter parsing. This means Obsidian 1.12.7 parses frontmatter as YAML 1.2 (e.g., `no` is **not** automatically coerced to `false`, unlike YAML 1.1).
- **Tag handling:** `!!tag:yaml.org,2002:` prefixes are processed normally by the library. There is no custom schema applied, so Obsidian does not perform any special tag coercion beyond the `yaml` v2 defaults (core schema).
- **Mixes in:** N/A (pure function)

---

### `stringifyYaml(obj: unknown)`

- **Kind:** function
- **Exported as:** `stringifyYaml`
- **Source:** `parsing/stringifyYaml.js:5`.
- **Signature:** `(obj: unknown) => string | undefined`
- **Purpose:** Serialises a JavaScript value back to a YAML string. Wraps the `yaml` package's `Document#toString()` or `new Document(obj).toString()` path (`yE` checks if input is already a `yaml.Document`; `AS` constructs a fresh one). Options:
  - `nullStr: ""` — YAML `null` values are serialised as the empty string (not `"null"` or `"~"`).
  - `lineWidth: 0` — no line-wrapping (long strings are never broken into multiple lines).
  - `aliasDuplicateObjects: false` — duplicate object references are serialised independently (no YAML anchors/aliases in output).
- **Return:** `undefined` when `obj === undefined` and `keepUndefined` is not set (i.e., `undefined` values do not appear in the written file).
- **Frontmatter round-trip note:** `stringifyYaml` is what `processFrontMatter` (`vI`) calls after mutation. Key ordering in the output is determined by the JS object's property insertion order (standard Map semantics). Tags that were originally `#tag` strings are written back as strings; the `yaml` library does not re-apply Obsidian-specific normalisation.
- **Mixes in:** N/A (pure function)

---

### `parseFrontMatterEntry(fm: object | null, key: string | RegExp)`

- **Kind:** function
- **Exported as:** `parseFrontMatterEntry`
- **Source:** `parsing/parseFrontMatterEntry.js:5`.
- **Signature:** `(fm: object | null, key: string | RegExp) => unknown`
- **Purpose:** Single-key getter against a parsed frontmatter object (`parseYaml` output). If `fm` is falsy, returns `null`. If `key` is a `string`, returns `fm[key]` (using `hasOwnProperty`), or `null` if absent. If `key` is a `RegExp`, iterates `fm`'s own keys and returns the value of the first key matching the regex. Returns `null` if no key matches.
- **Consumer:** `parseFrontMatterStringArray` uses this as its lookup primitive. Direct consumers can use any regex (e.g., `/^aliases$/i` for case-insensitive match).
- **Mixes in:** N/A (pure function)

---

### `parseFrontMatterStringArray(fm: object | null, key: string | RegExp)`

- **Kind:** function
- **Exported as:** `parseFrontMatterStringArray`
- **Source:** `parsing/parseFrontMatterStringArray.js:5`.
- **Signature:** `(fm: object | null, key: string | RegExp) => string[] | null`
- **Purpose:** Generic array-or-string getter. Calls `parseFrontMatterEntry(fm, key)`:
  - If result is a `string`: returns `[result.trim()]` (single-element array).
  - If result is an `Array`: filters to `string`-typed elements only (silently drops numbers, booleans, null), then `.trim()`s each. Returns the filtered-mapped array.
  - If result is anything else (null, number, boolean, object): returns `null`.
  - If `parseFrontMatterEntry` returns falsy: returns `null`.
- **Compat note:** Non-string array elements are silently dropped. A frontmatter `tags: [work, 42, null, "home"]` yields `["work", "home"]` (the integer and null are gone). This is intentional and must be matched by Corbomite.
- **Mixes in:** N/A (pure function)

---

### `parseFrontMatterTags(fm: object | null)`

- **Kind:** function
- **Exported as:** `parseFrontMatterTags`
- **Source:** `parsing/parseFrontMatterTags.js:5`.
- **Signature:** `(fm: object | null) => string[] | null`
- **Purpose:** Extracts and normalises tag values from frontmatter. Calls `parseFrontMatterStringArray(fm, /^tags$/i)` (case-insensitive key match, so both `tags:` and `Tags:` work). Post-processes:
  1. Filters out blank strings and tags containing a space character (tags with spaces are invalid in Obsidian).
  2. Prepends `#` to any tag that does not already start with `#`.
- **Key facts:**
  - The key match is case-insensitive (`/^tags$/i`), but not a plural/singular fallback — only `tags` (not `tag`).
  - A frontmatter `tags: work` (bare string) yields `["#work"]`.
  - A tag value `"my tag"` (contains space) is **silently dropped** (not an error).
  - Returns `null` when `fm` is falsy or the `tags` key is absent/non-string-array.
- **Cross-ref:** `getAllTags` in `metadata/` calls this function and appends inline `#tag` occurrences to the result.
- **Mixes in:** N/A (pure function)

---

### `parseFrontMatterAliases(fm: object | null)`

- **Kind:** function
- **Exported as:** `parseFrontMatterAliases`
- **Source:** `parsing/parseFrontMatterAliases.js:5`.
- **Signature:** `(fm: object | null) => string[] | null`
- **Purpose:** Extracts alias values from frontmatter. Calls `parseFrontMatterStringArray(fm, /^aliases$/i)` then `.trim()`s each result and filters out empty strings. Key match is case-insensitive.
- **Key facts:**
  - A bare-string `aliases: "Old Name"` is valid and yields `["Old Name"]`.
  - `aliases: ["", "  "]` yields `null` (all trimmed to empty, then filtered).
  - Returns `null` when the key is absent or the frontmatter object is falsy.
- **Cross-ref:** `MetadataCache.getLinkSuggestions` uses this to populate the quick-switcher with alias matches.
- **Mixes in:** N/A (pure function)

---

### `parsePropertyId(key: string)`

- **Kind:** function
- **Exported as:** `parsePropertyId`
- **Source:** `parsing/parsePropertyId.js:5`. Line range 133477–133494 (Bases feature, later in the bundle).
- **Signature:** `(key: string) => { type: "note" | "formula" | "file"; name: string }`
- **Purpose:** Normalises a Bases property column identifier from the `.base` schema into `{ type, name }`. Splits at the first `.`. If there is no `.`, or if the prefix is not one of `"note"`, `"formula"`, or `"file"`, the entire key is treated as `type: "note"` with `name: key`. Otherwise splits into `{ type: prefix, name: rest }`.
- **Examples:**
  - `"title"` → `{ type: "note", name: "title" }`
  - `"file.ctime"` → `{ type: "file", name: "ctime" }`
  - `"formula.myCalc"` → `{ type: "formula", name: "myCalc" }`
  - `"unknown.foo"` → `{ type: "note", name: "unknown.foo" }` (unrecognised prefix → full key as note name)
- **Consumer:** Bases view resolvers; `metadata/MetadataCache.getAllPropertyInfos` for the property-type registry. Cross-reference: `domains/bases.md` for the full property-type system.
- **Mixes in:** N/A (pure function)

---

### `parseLinktext(linktext: string)` — cross-domain: defined in `vault/`

- **Defined in:** `vault/parseLinktext.js` (`app.renamed.js:46369`). Audited in `domains/vault.md §1`.
- **Signature:** `(linktext: string) => { path: string; subpath: string }`
- **Purpose:** Decomposes a wikilink interior string at its first `#`. `path` is everything before `#` (may be `""` for a pure subpath link like `#heading`). `subpath` is everything from `#` inclusive (may be `""` when no `#` present).
- **Subpath shapes handled:**
  - `"Note#Heading"` → `{ path: "Note", subpath: "#Heading" }`
  - `"Note#^blockid"` → `{ path: "Note", subpath: "#^blockid" }`
  - `"Folder/Note#H1#H2"` → `{ path: "Folder/Note", subpath: "#H1#H2" }` (multi-level subpath; `resolveSubpath` handles the nesting)
  - `"#heading"` → `{ path: "", subpath: "#heading" }` (pure anchor)
  - `"Note"` → `{ path: "Note", subpath: "" }` (no subpath)
- **Display/alias parsing:** `parseLinktext` does NOT split `[[target|display]]` — the `|` is stripped upstream by the markdown parser before `parseLinktext` is called. The full wikilink text passed to `parseLinktext` is always `target` (possibly with `#subpath`) only.
- **Consumer:** `FileManager.processFrontMatter` link-rewriting, `MetadataCache` link resolution, `MarkdownView.setEphemeralState` subpath navigation.

---

### `resolveSubpath(cache, subpath)` — cross-domain: defined in `utils/`

- **Defined in:** `utils/resolveSubpath.js` (`app.js:79809–79883`). Lives in `obsidian/utils/`, not `obsidian/parsing/`.
- **Signature:** `(cache: CachedMetadata | null, subpath: string | null) => ResolvedSubpath | null`
- **Purpose:** Resolves a `#subpath` string against a file's `CachedMetadata`. Returns structured position info (byte offsets and line info). Returns `null` if `cache` or `subpath` is falsy, or if no match is found.
- **Dispatch logic:**
  1. Splits `subpath` on `#`, filters empty strings. If empty after split, returns `null`.
  2. If `subpath` is a single segment starting with `^` (block ID), searches `cache.blocks` using case-insensitive key comparison. If a matching `BlockCache` is found, additionally searches `cache.listItems` for a list item with the same `id`. Returns `{ type: "block", block, list, start, end }`.
  3. If `subpath` is a single segment starting with `[^` (footnote ref), searches `cache.footnotes` by `id`. Returns `{ type: "footnote", footnote, start, end }`.
  4. Otherwise, resolves as a heading path. Iterates `cache.headings` in source order, matching each segment of the `subpath` array in sequence (using `stripHeading` for normalisation, case-insensitive). The current heading's level must be greater than the previous match's level (ensures `## Sub` is matched after `# Parent`). The "end" of the resolved heading section is the start of the **next** heading at equal or lower level. Returns `{ type: "heading", current, next, start, end }` where `end` is `null` if the heading runs to end-of-file.
- **Consumer:** `MarkdownView.setEphemeralState` (scroll-to-subpath on open), `![[Note#heading]]` embed rendering in `MarkdownPreviewRenderer` (`NT` helper uses this).

---

### `stripHeading(heading: string)` — cross-domain: defined in `utils/`

- **Defined in:** `utils/stripHeading.js` (`app.js:79799–79801`).
- **Signature:** `(heading: string) => string`
- **Purpose:** Strips markdown formatting characters from a heading string for comparison purposes. Regex `AT = /[!"#$%&()*+,.:;<=>?@^`{|}~\/\[\]\\\r\n]/g` — replaces each matched char with a space, then collapses consecutive spaces with `/\s+/g` → `" "`, then trims. The `#` char itself is in `AT` (heading `##` markers are stripped).
- **Used by:** `resolveSubpath` heading-path matching; must be applied to both the stored heading text and the query segment before case-insensitive comparison.

---

### `stripHeadingForLink(heading: string)` — cross-domain: defined in `utils/`

- **Defined in:** `utils/stripHeadingForLink.js` (`app.js:79802–79804`).
- **Signature:** `(heading: string) => string`
- **Purpose:** Strips characters that are illegal in the `#heading` portion of a wikilink. Regex `PT = /([:#|^\\\r\n]|%%|\[\[|]])/g` — specifically strips `:`, `#`, `|`, `^`, `\`, `\r`, `\n`, `%%`, `[[`, `]]`. Much narrower than `stripHeading` (which is for comparison); this is for **generating** a safe linkable heading. Replaces each match with a space, collapses/trims same way.
- **Used by:** `Vault.getAvailablePathForAttachments` (basename sanitisation) and any code that generates `[[Note#Heading]]` links from heading text.
- **Key distinction from `stripHeading`:** `stripHeadingForLink` removes only link-unsafe chars (six characters/sequences); `stripHeading` removes all markdown punctuation. Same output structure and trailing space-collapse pattern. Use `stripHeading` for heading resolution/comparison; use `stripHeadingForLink` for generating link text.

---

### `processFrontMatter(file, mut, opts?)` — defined in `vault/FileManager`; serialisation via `parsing/`

- **Defined in:** `vault/FileManager.js:56895` (`vI` helper at `56958`). Not in `parsing/` but is the public contract for frontmatter round-trip.
- **Signature:** `(file: TFile, mut: (fm: object) => void, opts?) => Promise<void>`
- **Purpose:** Atomic read-parse-mutate-write cycle for frontmatter. Delegates to `vault.process(file, vI(text, mut), opts)`. The inner `vI` function:
  1. Calls `getFrontMatterInfo(text)` to locate the frontmatter block.
  2. Calls `parseYaml(frontmatter)` → JS object. If parse fails or result is non-object, resets to `{}`.
  3. Calls `mut(fm)` — the caller mutates the object in-place (add/change/delete keys).
  4. After mutation: if `Object.keys(fm).length === 0`, the frontmatter block is removed entirely (body text only is returned). Otherwise calls `stringifyYaml(fm)` and splices the result back at `[from, to]`.
  5. If the file had no frontmatter and the mutated object is non-empty, prepends `"---\n" + yaml + "---\n"`.
- **Key ordering:** JS object property insertion order. Keys that were in the original frontmatter retain their position only if `mut` does not delete and re-add them. `yI` (the ordered-assign helper at `app.renamed.js:56998`) is used in `insertIntoFile` (not in `processFrontMatter`); `processFrontMatter` itself does **not** call `yI` — mutation is purely in-place on the JS object. Corbomite must match this: existing key order is stable only if the mutator does not remove and re-add keys.
- **Comment preservation:** `stringifyYaml` uses the `yaml` v2 library, which does NOT preserve YAML comments. Comments in the original frontmatter are lost on any `processFrontMatter` call that produces a non-empty result.
- **Whitespace:** `lineWidth: 0` in `stringifyYaml` means no wrapping. Multi-line strings use block scalars as chosen by the `yaml` library.

---

## 2. Data structures

### `FrontMatterInfo`

```typescript
{
  exists: boolean;       // true iff opening AND closing --- found
  frontmatter: string;   // raw YAML text between delimiters (no --- lines)
  from: number;          // byte offset: start of frontmatter (after opening ---\n)
  to: number;            // byte offset: start of closing --- line
  contentStart: number;  // byte offset: first char of note body (after closing ---\n)
}
```

When `exists === false`: `from = to = contentStart = 0`, `frontmatter = ""`.

### `ResolvedSubpath`

Three variants, returned by `resolveSubpath`:

```typescript
// Block reference (#^blockid)
{
  type: "block";
  block: BlockCache;     // { id, position: { start, end } }
  list: ListItemCache | null;  // populated when block is a list item
  start: Pos;            // { line, col, offset }
  end: Pos;
}

// Footnote reference (#[^id])
{
  type: "footnote";
  footnote: FootnoteCache;   // { id, position }
  start: Pos;
  end: Pos;
}

// Heading reference (#Heading or #H1#H2 nested)
{
  type: "heading";
  current: HeadingCache;     // { heading, level, position }
  next: HeadingCache | null; // the next same-or-higher-level heading (null = EOF)
  start: Pos;
  end: Pos | null;           // null = runs to end of file
}
```

### `ParsedPropertyId`

```typescript
{
  type: "note" | "formula" | "file";
  name: string;   // everything after the prefix dot, or the full key if no valid prefix
}
```

---

## 3. On-disk contracts

The `parsing/` domain produces and consumes **no files directly** — it is a pure-function library. However, its output dictates the on-disk wire format for any code writing frontmatter.

### Frontmatter wire format (written by `processFrontMatter` via `stringifyYaml`)

- **Path:** Any `.md` file's frontmatter block (opening `---\n`, YAML, closing `---\n`).
- **Delimiter regex (read):** `Yx = /^---(\r?\n)/g` (opening), `Qx = /---(\r?\n|$)/g` (closing, requiring preceding `\n`).
- **Delimiter format (write):** Always `---\n` (LF only; CRLF-original files get LF delimiters after write-back — CRLF preservation is lost).
- **YAML engine:** eemeli/yaml v2, YAML 1.2, core schema, no anchors, `nullStr: ""`, `lineWidth: 0`.
- **Null values:** serialised as empty string `""` (YAML: `key: \n` or `key: ""`).
- **Lists:** serialised as YAML block sequences (one item per line with `- ` prefix) for arrays, inline flow for short scalar arrays per yaml v2 defaults.
- **Tags:** written back as plain strings without `#` prefix (the `#` is injected at read-time by `parseFrontMatterTags`; it is NOT stored).
- **Comment stripping:** Any YAML comments in the original file are silently dropped on any `processFrontMatter` call.
- **Key order:** JS object property insertion order — stable for existing keys if not deleted/re-added.
- **Empty frontmatter after mutation:** The block is removed entirely (no `---\n---\n` residue).
- **No frontmatter before mutation, non-empty after:** `"---\n" + yaml + "---\n"` is prepended.

---

## 4. Events emitted

N/A — this domain consists entirely of pure functions. No event emitters.

---

## 5. Events consumed

N/A — pure functions; no event subscriptions.

---

## 6. Commands registered

No commands registered here.

---

## 7. Registries owned

N/A — this domain owns no registries.

---

## 8. Invariants

- `getFrontMatterInfo(text).exists === true` requires both `Yx` (opening `---\r?\n`) and `Qx` (closing `---\r?\n|$`) to match, with the closing `---` preceded by a `\n`. A file whose first line is `---` but has no closing `---` has `exists === false`.
- `parseFrontMatterTags` returns tags with `#` prepended and no spaces. A tag containing a space is silently dropped, not an error. The key regex is `/^tags$/i` — `tag:` (singular) is NOT matched.
- `parseFrontMatterAliases` always `.trim()`s aliases before filtering; `aliases: " "` is treated as absent.
- `parseFrontMatterStringArray` silently converts a bare string value to a single-element array. Non-string array members (numbers, booleans, null, objects) are silently dropped.
- `stringifyYaml` with `nullStr: ""` means `{ key: null }` serialises as `key: \n` (or `key: ` depending on context) — **not** as `key: null` or `key: ~`. Corbomite must match.
- `parseYaml` uses YAML 1.2 defaults. Unlike YAML 1.1, `yes`/`no`/`on`/`off` are NOT coerced to booleans. `true`/`false`/`null` are still special.
- `resolveSubpath` heading matching uses `stripHeading` on both sides before case-insensitive comparison. A heading `## **Bold**` matches a subpath `bold` after `stripHeading` collapses the `*` chars to spaces and trims.
- `resolveSubpath` block-id comparison is case-insensitive (`r = i.substr(1).toLowerCase()`; key loop also `.toLowerCase()`).
- `processFrontMatter` is a no-op (no write, no `mut` call) for non-`.md` files.
- `processFrontMatter` removes the entire frontmatter block when `mut` empties the object.
- `stringifyYaml` returns `undefined` (not `""`) when `obj === undefined`.

---

## 9. Observable user features

- When a user edits frontmatter properties in the Properties panel, `processFrontMatter` is called per-keystroke (debounced). The user sees live property-value updates without manual YAML editing.
- When a user renames a note, link refactoring in `FileManager.renameFile` uses `parseLinktext` to locate and rewrite link occurrences vault-wide.
- Aliases added to `aliases:` frontmatter appear in the Quick Switcher and search results (via `parseFrontMatterAliases` → `MetadataCache.getLinkSuggestions`).
- Tags in `tags:` frontmatter appear in the tag pane and tag-based search (via `parseFrontMatterTags` → `getAllTags` → `MetadataCache.getTags`).
- Clicking a `[[Note#Heading]]` link scrolls to the heading (via `parseLinktext` → `resolveSubpath` → `setEphemeralState`).
- Clicking a `[[Note#^blockid]]` link scrolls to the block (same path; `resolveSubpath` dispatches on `^` prefix).
- Bases columns reference properties by qualified ID `"note.title"`, `"file.ctime"`, `"formula.calc"` — `parsePropertyId` resolves these to the correct data source.

---

## 10. Extension surfaces exposed

No extension surfaces here. The `parsing/` functions are plain utility functions — not registries, not events, not subclass hooks. Plugins consume `parseYaml`, `stringifyYaml`, `getFrontMatterInfo`, `parseFrontMatterTags`, `parseFrontMatterAliases`, `parseFrontMatterEntry`, `parseFrontMatterStringArray`, and `parseLinktext` directly from the `obsidian` module as stateless helpers; there is nothing to "register" or override.

`processFrontMatter` on `app.fileManager` is the one exception: it is the plugin-facing API for frontmatter mutation (documented in `domains/vault.md §10`).

---

## 11. Corbomite mapping

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `getFrontMatterInfo(text)` | `Markoff::Document::fromMarkdown` (frontmatter extraction in `Document.cpp:35–47`) | Partial | Corbomite extracts frontmatter string but does not return `{ from, to, contentStart }` offsets. Offset fields needed for in-place splice in `processFrontMatter` equivalent. |
| `parseYaml(text)` — eemeli/yaml v2, YAML 1.2 | No equivalent | **Missing** | Corbomite has no YAML library. `document.frontmatter()` returns raw YAML string only. Recommend linking `yaml-cpp` (YAML 1.2) or `libyaml` as `libs/core/FrontMatterParser.{h,cpp}`. |
| `stringifyYaml(obj)` with `nullStr: ""`, `lineWidth: 0`, no aliases | No equivalent | **Missing** | Round-trip serialisation is absent. Any `processFrontMatter` analogue requires this. `yaml-cpp` emit API can be configured similarly. |
| `parseFrontMatterTags(fm)` | No equivalent | **Missing** | Corbomite reads the frontmatter string but does not parse `tags:` out of it. Needed for `SQLiteIndex` tag population and search compat. |
| `parseFrontMatterAliases(fm)` | No equivalent | **Missing** | Needed for alias-aware search and Quick Switcher. |
| `parseFrontMatterEntry(fm, key)` | No equivalent | **Missing** | Generic key getter; needed once YAML is parsed. |
| `parseFrontMatterStringArray(fm, key)` | No equivalent | **Missing** | Generic array-or-string normaliser; needed for tags and aliases. |
| `parseLinktext(linktext)` | `SQLiteIndex.cpp` has inline split-at-`#` logic implicitly | **Missing (as standalone)** | The wikilink patterns in `SQLiteIndex.cpp:462–464` strip `#subpath` via the regex `[^\]|]+` (which stops at `|` but not at `#`). Subpath is NOT extracted from wikilinks today — only the `target` before `|`. No `parseLinktext` equivalent exists. Needed for: link resolution with subpath, rename refactoring, embed scroll-to-heading. |
| `resolveSubpath(cache, subpath)` | `Markoff::Document::extractSubpath(subpath)` (partial, `Document.h:45`) | Partial | `extractSubpath` exists in the header but its implementation in `Document.cpp` is not shown in extracted lines. Heading/block/footnote dispatch and the nested-heading-path algorithm need verification against Obsidian's exact logic. |
| `stripHeading(heading)` / `stripHeadingForLink(heading)` | No equivalent | **Missing** | Both are needed for heading-link generation and subpath resolution. Trivial to port as static helpers in a `libs/core/LinkUtils.h`. |
| `parsePropertyId(key)` | No equivalent | **Missing** | Relevant only when Corbomite implements Bases-style typed properties. |
| `processFrontMatter` (FileManager) | No equivalent | **Missing** | Full atomic read-parse-mutate-write cycle absent. See `domains/vault.md`. |
| Frontmatter delimiter regex (`/^---(\r?\n)/g`, `/---(\r?\n|$)/g`) | `Document.cpp:37` uses `source.startsWith("---\n")` / `source.indexOf("\n---", 3)` | Partial | Corbomite's detection is functionally close but misses: (a) CRLF support in the opening delimiter (Corbomite checks `---\r\n` separately on line 37 — actually present, OK), (b) the closing `---` must be preceded by a newline — Corbomite uses `indexOf("\n---", 3)` which is equivalent. Main gap: Corbomite does not handle `---\r?\n$` (closing delimiter at EOF without trailing newline). |

---

## 12. Markoff gap confirmations / discoveries

N/A — no editor/rendering surface in this domain. The parsing domain is pure-function utilities that feed `MetadataCache` and `FileManager`. The frontmatter-related Markoff gap (Properties panel, live frontmatter editing) is documented in `domains/editor-markdown.md §12`.

---

## 13. Open questions

1. **`yaml-cpp` vs `libyaml` for Corbomite YAML parsing.** Which library produces the closest output to eemeli/yaml v2 for Obsidian compat (especially `nullStr: ""`-equivalent, no-line-wrap, no anchors)? A brief spike using `yaml-cpp` emit API against a set of Obsidian-generated frontmatter samples would answer this.
2. **`resolveSubpath` in `Markoff::Document`.** The header declares `extractSubpath(subpath)` but the audit scope only read `Document.cpp` up to line 80. Does the current implementation handle block IDs (`#^id`), footnotes (`#[^id]`), and nested heading paths (`#H1#H2`)? If it does only heading-by-text, parity is partial.
3. **Corbomite frontmatter delimiter at EOF.** Obsidian's closing `Qx = /---(\r?\n|$)/g` accepts `---` at the very end of file with no trailing newline. Corbomite's `indexOf("\n---", 3)` pattern requires a preceding `\n`, which means a file ending with `\n---` (no trailing newline) is handled, but a file ending with just `---` (no newline before it) is not. Need to confirm whether real Obsidian vaults ever produce this shape.
4. **Tag key casing.** `parseFrontMatterTags` uses `/^tags$/i` — `Tags:` and `TAGS:` are accepted. Does Corbomite's frontmatter tag scanner currently match case-insensitively? The `Document.cpp` code was not shown to include any tag parsing.
5. **`stringifyYaml` `nullStr: ""`** — does the chosen YAML library for Corbomite support an equivalent option? `yaml-cpp` does not expose `nullStr` natively; a custom emitter would be required.
6. **YAML 1.1 vs 1.2 coercion.** Obsidian uses YAML 1.2. Does any existing Corbomite vault scanner or frontmatter reader coerce `yes`/`no` as booleans (YAML 1.1 behavior)? If so, it would silently corrupt `yes:` and `no:` keys.

---

## 14. Recommended Pass 3 synthesis input

1. **Hoist the frontmatter wire format into `VAULT-FORMAT.md` immediately.** The `getFrontMatterInfo` delimiter regexes, the `stringifyYaml` options (`nullStr: ""`, `lineWidth: 0`, `aliasDuplicateObjects: false`), the YAML 1.2 / core-schema semantics, and the comment-stripping behaviour are collectively the **exact on-disk contract** for every note's frontmatter. Any Corbomite code that writes frontmatter must replicate these byte-for-byte. Pass 3 should create a `VAULT-FORMAT.md §frontmatter` section with the regexes and YAML-library options as the normative spec.
2. **Flag "YAML library + `parseFrontMatterTags`/`Aliases` + `parseLinktext` subpath extraction" as a single Priority-1 gap cluster in `GAP-ANALYSIS.md`.** These four missing pieces share one prerequisite (a YAML library) and together unlock: correct tag indexing, alias-aware search, correct wikilink rename refactoring, and subpath navigation. They are individually small but collectively gating. Recommend implementing as `libs/core/FrontMatter.{h,cpp}` (YAML parse/stringify) + `libs/core/LinkUtils.{h,cpp}` (`parseLinktext`, `stripHeading`, `stripHeadingForLink`, `resolveSubpath`).
3. **Direct Pass 3 to extract the linktext grammar into `VAULT-FORMAT.md`.** The `parseLinktext` split-at-`#` rule, the `|` alias split (done by the markdown parser), the `^blockid` and `[^footnote]` subpath shapes, and the nested `#H1#H2` heading path are the **wikilink grammar**. This is vault-format, not UI. `VAULT-FORMAT.md` should have a wikilink grammar section (BNF-level) so any Corbomite parser can be validated against it without re-reading JS.

---

## 15. Cross-domain references

| Other domain | Reference type | Brief description |
|---|---|---|
| `vault` | sibling / consumer | `parseLinktext` is declared in `vault/` (audited in `domains/vault.md §1`). `FileManager.processFrontMatter` + helper `vI` is the primary consumer of `getFrontMatterInfo`, `parseYaml`, `stringifyYaml`. |
| `metadata` | consumer | `parseFrontMatterTags` and `parseFrontMatterAliases` are called by `metadata/getAllTags` and `MetadataCache.getLinkSuggestions`. `parseYaml` is used by `MarkdownView.loadFrontmatter` (via `editor-markdown`). |
| `editor-markdown` | consumer | `getFrontMatterInfo` and `parseYaml` used by `MarkdownView.loadFrontmatter`. `resolveSubpath` used by `setEphemeralState` for scroll-to-heading. `parseLinktext` used by embed/hover-link resolution. Referenced at `domains/editor-markdown.md §15`. |
| `bases` | consumer | `parsePropertyId` is the column-key resolver for Bases view; `parseFrontMatterEntry` and `parseFrontMatterStringArray` are used by Bases for typed property access. |
| `utils` | hosts subpath/heading helpers | `resolveSubpath`, `stripHeading`, `stripHeadingForLink` live in `obsidian/utils/` not `obsidian/parsing/`, but are tightly coupled to the parsing domain. Future Corbomite implementation of these should colocate them with the frontmatter helpers in `libs/core/`. |

| Short symbol | Defined in | Used here for |
|---|---|---|
| `Yx` | `parsing/` (module scope, `app.renamed.js:45995`) | Opening frontmatter delimiter regex (`/^---(\r?\n)/g`) |
| `Qx` | `parsing/` (module scope, `app.renamed.js:45996`) | Closing frontmatter delimiter regex (`/---(\r?\n|$)/g`) |
| `Jx` | `parsing/` (module scope, `app.renamed.js:46043`) | `Object.prototype.hasOwnProperty` alias used by `parseFrontMatterEntry` |
| `AT` | `utils/` (module scope, `app.renamed.js:46404`) | `stripHeading` replacement regex — all markdown punctuation |
| `PT` | `utils/` (module scope, `app.renamed.js:46405`) | `stripHeadingForLink` replacement regex — link-unsafe chars only |
| `xx` | `parsing/` (module scope, `app.renamed.js:45024`) | YAML parse function from the bundled `yaml` v2 npm package |
| `yE`, `AS` | `parsing/` (module scope, bundled `yaml` v2) | `yaml.Document` type-check and constructor used by `stringifyYaml` |
| `vI` | `vault/FileManager.js:56958` | Inner helper implementing frontmatter splice logic for `processFrontMatter` |
| `yI` | `vault/FileManager.js:56998` | Ordered-assign merge helper (used by `insertIntoFile`, not `processFrontMatter`) |
