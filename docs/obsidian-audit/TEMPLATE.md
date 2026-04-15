# Pass 2 domain-audit template

Every Pass 2 agent produces one document following this exact structure, saved to `docs/obsidian-audit/domains/<domain>.md`. The shape is rigid on purpose: Pass 3 synthesis is mechanical only if every doc is mechanical.

Word budget: **2000–6000 words** depending on domain size. On-disk-heavy and big-API domains (vault, workspace, bases, editor/markdown, plugin, core) trend toward the ceiling; utils/platform/secrets/network near the floor. Do not compress past the point of clarity — the spec must be implementable. **Pass 1 already produced a routing-table summary for every domain in `00-taxonomy.md` — copy that domain's section verbatim into the header and deepen everything else.**

---

## Agent rules of engagement

1. **Read every file in the assigned domain.** Pass 1 was the skim. Pass 2 is the audit.
2. **De-minifier artifact pre-flight (critical — do this first):**
   - Run `md5sum` (via Bash) on every file in the assigned directory. Any two files with matching `md5sum`, or with sizes within ~5% of each other, are candidates for duplicate extraction. `diff` them to confirm.
   - Look at the first few lines of each file for a `// source: app.js lines <a>-<b>` range comment (or `// public API symbol: <Name>`). **Only audit code inside the declared source-line range.** Code after a noticeable gap at the end of a file is adjacent leftover from the extraction window and belongs to a different domain — ignore it, note it in a "De-minifier artifact note" in the header.
   - If N files are near-duplicates, pick one as canonical and say so. Do not audit the others independently.
   - Write a one-paragraph "**De-minifier artifact note:**" immediately after the Pass 1 summary in the document header, summarising what you found.
3. **Cross-reference before asserting.** If you claim "`ViewRegistry` is consumed by `Workspace`", grep the source tree (`rg '\bViewRegistry\b' obsidian/workspace/`) to confirm.
4. **Document the vault-disk contract precisely.** If this domain reads or writes any file in the vault (including `.obsidian/*.json`, `.obsidian/plugins/*/data.json`, frontmatter conventions, note-body conventions like `%%comment%%`, `.base` schemas, `.canvas` schemas), document the *schema in detail* — field names, types, optionality, defaults, ordering rules, migrations. Corbomite's compatibility depends on this.
5. **Enumerate events exhaustively.** For every `Events`-derived emitter in the domain, list every `.trigger(...)` call site with the event name and *inferred* payload shape. Mark unknowns as `payload: unknown — see OPEN QUESTIONS`.
6. **Enumerate commands exhaustively.** For every `commands.addCommand({ id, name, … })` in the domain, list the id, display name, hotkey hint, and effect in one line.
7. **No invention.** If you cannot determine something from the source, say so in OPEN QUESTIONS. Never guess.
8. **Scope discipline.** Do *not* `Read` files outside your assigned domain. You may `Grep` other directories to **confirm consumers** (`output_mode: files_with_matches` or a small `-C 2` context) — this is not "reading". Resist reading `vendor/codemirror/` entirely — if an Obsidian file imports a CM symbol, document what Obsidian *does with it*, not what CM's implementation is.
9. **Resolve short-name references in-tree before filing them as open questions.** If you see a reference like `PC`, `AC`, `UT`, `ru` and don't know what it is, *first* grep your own domain for `\bPC\s*=` / `const PC` / `function PC` / `var PC`. If it's genuinely not defined in your domain, *then* grep the rest of `obsidian/` with `output_mode: files_with_matches` to locate the owning domain — and document the reference as `PC (defined in <other-domain>)` in Section 15. Only if it is nowhere in the tree do you log it in OPEN QUESTIONS.
10. **Cite file:line as `<domain>/<file>.js:<line>`** (short form, no leading path). E.g. `vault/Vault.js:440`, not `/home/clinton/bin/ObsidianRAW/...`.
11. **Translate, don't just transcribe.** Every assertion should be phrased so a Corbomite engineer can act on it without re-opening the JS. "The function does X" is transcription; "When the user renames a file, the MetadataCache emits `changed` with payload `{file, cache, previousCache}` *after* the link re-resolution completes; plugins that listen for rename-events should prefer this over Vault's `rename` because the latter fires pre-resolution" is translation.

---

## Required document structure

**Section count: 15.** All sections must appear in order. If a section is genuinely inapplicable write `N/A — <one-line reason>` under the heading; do not skip.

The document opens with the standard header (title, source, file count, files, Pass 1 summary blockquote, **De-minifier artifact note** paragraph), then Sections 1–15.


```markdown
# `obsidian/<domain>` — <one-line purpose>

**Source:** `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/<domain>/`
**File count:** <n>
**Files:** `file1.js`, `file2.js`, …
**Pass 1 summary (verbatim from `00-taxonomy.md`):**
> <copy the Pass 1 paragraph here as a blockquote>

---

## 1. Public API surface

Every exported symbol, in declaration order. For each:

### `<SymbolName>`

- **Kind:** class | function | constant | type-alias
- **Exported as:** `<name>` (via `// public API symbol: <name>` comment or `module.exports` / `exports.X`)
- **Signature:** constructor args / function args / key instance methods with types inferred from callers
- **Purpose:** one paragraph
- **Lifecycle (if class):** how it's instantiated, what owns it, when it's destroyed
- **Mixes in:** `Component` | `Events` | both | neither

…repeat for each symbol. If the domain has >15 symbols, group by sub-topic and provide a one-line bullet per symbol plus full subsections for the architecturally important ones. **If a single class has >20 methods, group its methods by role (e.g. CRUD / events / config / internals) with a short paragraph per role — not one bullet per method.** A per-method enumeration is fine for small classes; it drowns the reader in large ones.

---

## 2. Data structures

Internal shapes that cross a domain boundary, persist to disk, or show up in emitted event payloads. For each:

### `<ShapeName>`

```typescript
// inferred TypeScript-ish description
{
  field1: Type;     // semantics
  field2?: Type;    // optional; default if any
  ...
}
```

Notes on invariants, ordering, normalisation.

---

## 3. On-disk contracts

What this domain reads from / writes to the vault and its `.obsidian/` config. **This section is load-bearing for Corbomite compatibility.**

For each file / directory touched:

- **Path:** `relative/path.json` or `.obsidian/<whatever>/**.json`
- **Written by:** function / code path
- **Read by:** function / code path
- **Schema:**
  ```typescript
  {
    version?: number;
    … // full field-by-field description
  }
  ```
- **Lifecycle:** when created, when updated, when deleted; whether absence is valid.
- **Migration behaviour:** how the code handles an unknown version or missing fields.

If domain is internal-only, write: `No on-disk contracts.`

---

## 4. Events emitted

Table per emitter class:

### `<EmitterClass>` (extends `Events`)

| Event name | Payload (inferred) | Triggered when | Typical consumers |
|---|---|---|---|
| `layout-ready` | `()` | first frame after workspace JSON restored | plugins initialising UI chrome |
| … | … | … | … |

Cite file:line for at least one `.trigger(...)` site per event.

If domain has no emitters: `No events emitted.`

---

## 5. Events consumed

What this domain listens to (on other domains or its own). Table:

| Listener file | Subscribes to | Why |
|---|---|---|

---

## 6. Commands registered

| Command ID | Display name | Default hotkey | Effect | Registered in |
|---|---|---|---|---|

If none: `No commands registered here.`

---

## 7. Registries owned

If this domain hosts a registry (`ViewRegistry`, `EmbedRegistry`, `HoverLinkSource`, menu section registry, etc.):

### `<RegistryName>`

- **Stores:** what type of value
- **Populated by:** which call sites (core, built-in plugins, external plugins)
- **Read by:** which consumers
- **Persistence:** in-memory-only | persisted to `.obsidian/<file>` (with schema) | synthesised from scan on startup
- **Lifecycle:** when entries are added, when removed, what happens on vault switch / plugin unload

**Scope rule:** *public plugin-facing registries* (anything a plugin populates through a `register*` / `add*` API) go here AND get a row in Section 10. *Internal-only maps* that happen to be registry-shaped (e.g. `FileManager.fileParentCreatorByType`) go here only. If in doubt: does a third-party plugin touch it? Yes → Section 10 too.

---

## 8. Invariants

Enumerated bullets. "X is always Y when Z." Corbomite must uphold these for compatibility.

Examples in template style:
- `Vault.adapter.read(path)` is only valid for paths returned by `Vault.getFiles()` or `Vault.getAbstractFileByPath`; paths must use `/` even on Windows.
- `MetadataCache.resolvedLinks[src][dst]` count is always ≥ 1 when present; missing keys mean zero links, not "unresolved".
- `Component.unload` is idempotent and safe-after-destroy.

---

## 9. Observable user features

Bullet list of user-visible behaviours this domain powers. Phrased as "the user can …" or "when the user does X, Y happens". These feed directly into `FEATURE-MATRIX.md` in Pass 3.

---

## 10. Extension surfaces exposed

What plugins hook into from this domain. Cite the registration verb and the consumer call site.

| Surface | Registration verb | Consumer call site | What plugins supply |
|---|---|---|---|
| Markdown post-processor | `Plugin.registerMarkdownPostProcessor(fn, order)` | `rendering/MarkdownRenderer.ts:<line>` | `(el, ctx) => void` |

If none: `No extension surfaces here.`

---

## 11. Corbomite mapping

Table per concept. **Be precise about Corbomite file paths** — this is the handoff to planning.

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `Vault.adapter` (DataAdapter interface) | `libs/storage/FileSystemAdapter` | Partial | Lacks `exists()`, `rmdir()`, atomic rename |
| `TFile` | `libs/core/NoteDocument` (note-only) | Partial | Corbomite lacks a general `TFile` for non-notes |
| … | … | Have / Partial / Missing | … |

---

## 12. Markoff gap confirmations / discoveries

Only for domains that touch the editor or rendering path (`editor`, `editor/markdown`, `rendering`, possibly `ui/popups` for hover previews). Confirm or refute each Pass-1 signal that applies, and add new ones you find. Format matches `01-markoff-gaps.md`.

For other domains, write: `N/A — no editor/rendering surface in this domain.`

---

## 13. Open questions

Numbered list. Anything you could not resolve from source alone. Each item is a specific question a human can answer, not a vague doubt.

1. Does `Workspace.on('file-menu', …)` payload include the source file for embed menus, or only the opened file? Need to cross-reference a built-in plugin emitter call.
2. …

---

## 14. Recommended Pass 3 synthesis input

1–3 bullets summarising the *biggest* items Pass 3 should promote into `FEATURE-MATRIX.md` / `VAULT-FORMAT.md` / `GAP-ANALYSIS.md` from this doc. Think of these as "if Pass 3 only reads the first paragraph of my doc, it must read *this*".

---

## 15. Cross-domain references

List every *other* Pass 2 domain this doc touches and why. Pass 3 uses this to build its cross-reference map mechanically.

| Other domain | Reference type | Brief description |
|---|---|---|
| `workspace` | consumer | `Vault` events are listened to by `Workspace` to update tab chrome on rename |
| `metadata` | consumer | `MetadataCache` rebuilds its index from `Vault.read` and `Vault` events |
| `parsing` | dependency | `FileManager.processFrontMatter` calls `parseYaml` |
| `core` | sibling | `App` holds the `Vault` instance |

Also list any **short symbols from other domains that this doc references by name** (e.g. "uses `PC` defaults table from `core`"). Pass 3 builds a shared symbol table from these.

| Short symbol | Defined in | Used here for |
|---|---|---|
| `PC` | `core` (likely) | appearance/app config defaults merged by `reloadConfig` |
| `ru` | `utils` (likely) | ignored-path predicate used by Vault event filtering |
```

---

## Notes to agents on tooling

- Use `rg` / `Grep` via the Grep tool (not Bash `grep`). Multi-line patterns with `multiline: true` when needed.
- Use `Read` for targeted slices of files; avoid re-reading an entire large file to find one thing.
- Do not write tests or change code. You are producing markdown docs only.
- If you find a Markoff gap or extension surface, also *append* to `/home/clinton/dev/Corbomite/docs/obsidian-audit/01-markoff-gaps.md` or `02-extension-surfaces.md` as appropriate, so the running lists stay authoritative. Use a `## Pass 2 additions — <domain>` subheading.

## Notes to agents on Corbomite

- The Corbomite repo is at `/home/clinton/dev/Corbomite/`. Glance as needed for the "Corbomite mapping" section. Do **not** modify Corbomite code.
- Markoff libraries: `libs/markoff/` (widget), `libs/markoff-parser/` (parser, tree-sitter-based), `libs/mmdr/` (Rust Mermaid bridge).
- Corbomite is C++20/Qt6/KDE Frameworks 6. Keep your `Corbomite mapping` recommendations Qt/KDE-idiomatic (e.g. "Qt signal" not "EventEmitter").
