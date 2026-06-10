# Properties panel — full editing surface (delete / reorder / rename / add-with-type)

**Date:** 2026-05-27
**Punch-list item:** P2 (shakedown) — "PropertiesView is read-mostly" (`docs/punch-list.md`, surfaced T-14 + T-17)
**Status:** Shipped 2026-05-27

## Background

The punch-list framing ("read-mostly … typed text is lost on save") is **stale**.
Since the 2026-04-17 InternalPlugin migration (`759e5809`), `src/plugins/properties/PropertiesView.cpp`
already:

- renders every frontmatter key as a typed `PropertyEditorWidget`
  (Text / Number / Checkbox / Date / DateTime / List),
- commits edits via a 500 ms-debounced `FileManagerProxy::processFrontMatter`,
- adds a new key via a `+ Add property` button (name prompt; always typed Text),
- suppresses external-edit refresh while a user edit is pending.

What is actually missing: **delete a key, reorder keys, rename a key, and choose a
type when adding.** This spec covers those four, plus a correctness fix for a
pre-existing silent-corruption bug uncovered during design.

## Pre-existing bug this work fixes

`inferPropertyType` maps `Kind::Map` → `PropertyType::Text`. `TextPropertyEditor::setValue`
flattens a Map to `""` (its fallback `setText(value.asString())`), and `flushWrite`
rewrites **every** row on any edit. Net effect on `master`: **editing any property in a
note whose frontmatter contains a nested-map value silently overwrites that map with an
empty string.** Lists-of-non-scalars are similarly unsafe. The lossy-value handling below
(§5) closes this.

## Goal

Make the Properties panel a complete frontmatter editor: add (with type), edit, delete,
rename, and reorder properties, persisting key order to YAML and never corrupting value
shapes the editors can't represent.

## Non-goals (YAGNI)

- **Changing the type of an existing property** (Obsidian's type-icon dropdown). Type is
  fixed at add time. Deferred — punch-list follow-up if wanted.
- **Editing nested/complex values in place.** Map values and lists-of-non-scalars are
  shown read-only and preserved verbatim, not made editable.
- **Multi-level list editing.** The existing List editor handles flat string lists; that
  behavior is unchanged.

## Architecture

### 0. Markoff prerequisite — `YamlValue::setChildFrom`

Order-authoritative rebuild requires building a *fresh* map in insertion order
(`findOrCreateKey` keeps existing keys in place; new keys append at end — so an
existing tree can't be reordered in place). `YamlValue` currently has no primitive
to copy an arbitrary node verbatim into that fresh map. Add one to the Markoff
submodule (`libs/markoff-family/libs/markoff-parser`), backed by ryml's
`Tree::duplicate`:

```cpp
// YamlValue.h — new mutation method
void setChildFrom(const QString &key, const YamlValue &src);
```

Semantics: deep-copies `src`'s subtree (any kind: scalar / seq / map, nested
arbitrarily) into this map under `key`, appended as the last child (so a fresh
build preserves call order). If `key` already exists it is removed first. Backed by
`d->tree->duplicate(src.d->tree.get(), src.d->nodeId, d->nodeId, last_child)` +
an explicit `set_key`. This is byte-faithful for every YAML shape, unlike the
scalar-only setters.

**Cross-repo ordering (CONTRIBUTING-OPS Ritual 5):** land + commit + push this in
Markoff first, then bump the Corbomite submodule pin, so Corbomite's pin always
resolves. Markoff is currently at `082b063`.

### 1. Vault API — ordered front-matter setter

Add to `FileManager` (and mirror on `FileManagerProxy`, gated on the `vault.write`
permission, matching the existing `processFrontMatter` proxy method):

```cpp
struct FrontMatterEntry {
    QString  key;
    QVariant value;                 // used when preserveFromDisk == false
    bool     preserveFromDisk = false;
};

bool setFrontMatter(TFile *f, const QList<FrontMatterEntry> &ordered);
```

Semantics:

- Re-parses the **real on-disk YAML** via `m_vault->process(...)` + `Markoff::Document::fromMarkdown`,
  exactly like `processFrontMatter` (so it never trusts a lossy in-memory view).
- Build a **fresh** `next = YamlValue::emptyMap()` and append entries in list order
  (insertion order is authoritative). For each entry: if `preserveFromDisk`, copy that
  key's node from the freshly parsed on-disk frontmatter via
  `next.setChildFrom(key, working.get(key))` (byte-faithful, §0); otherwise set the scalar/
  list via the `QVariant`-kind switch (`setString`/`setInt`/`setBool`/`setDouble`/`setSeq`).
- Builds `next` by appending entries **in list order** — this order is authoritative
  (no `originalKeys` reconstruction; that is `processFrontMatter`'s merge behavior, not this
  wholesale setter's).
- Keys present on disk but **absent** from `ordered` are dropped (delete).
- If `ordered` is empty, take the same strip-the-frontmatter-block branch
  `processFrontMatter` uses (emit a null `YamlValue` so the `---` fence is removed).
- Reuses `applyVariantMapToYaml` / the emit path; `processFrontMatter` is **untouched**, so
  existing callers (Bases `+New` seed, etc.) keep their read-modify-write merge semantics.

A `preserveFromDisk` entry whose key is **not** on disk is emitted as a YAML null (defensive;
shouldn't occur because such rows only originate from on-disk values).

### 2. Row architecture — `PropertyRow`

Replace `PropertiesView`'s `QFormLayout` with a `QVBoxLayout` of `PropertyRow` widgets.
`PropertiesView` owns the ordered `QVector<PropertyRow*>` and is the single source of truth
for order; it performs all writes.

Each `PropertyRow` (new files `src/sidebar/PropertyRow.{h,cpp}`, beside `PropertyEditorWidget`)
lays out horizontally:

```
[≡ grip] [key: QLabel ↔ QLineEdit] [PropertyEditorWidget] [🗑 delete]
```

- **grip** — drag handle; starts an in-list reorder drag.
- **key** — `QLabel` that swaps to an inline `QLineEdit` on click; commits on Enter/focus-out.
- **editor** — `makePropertyEditor(type, value)` reused unchanged. Absent for read-only rows
  (replaced by a greyed summary label — see §5).
- **delete** — `QToolButton` (`QIcon::fromTheme("edit-delete")`), removes the row.

Signals: `valueChanged()`, `keyRenamed(QString oldKey, QString newKey)`,
`deleteRequested()`. Reorder is handled by `PropertiesView` via drag/drop (see §4).

`PropertyRow` exposes: `key()`, `type()`, `isReadOnly()`, `currentValue()` (delegates to the
editor for editable rows), and for read-only rows carries an opaque
`preserveFromDisk` flag so the view knows to emit a preserve entry.

### 3. Interactions

- **Add** — `+ Add property` opens an "Add property" dialog: name `QLineEdit` + type
  `QComboBox` (Text / Number / Checkbox / Date / Date-time / List). On accept, create a
  `PropertyRow` with an empty value of that type appended at the end; trigger a write.
  Duplicate name (case-insensitive) is rejected in the dialog (matches vault collision rules).
- **Edit** — editor `valueChanged` → `scheduleWrite()` (unchanged 500 ms debounce).
- **Rename** — inline key edit. Empty or case-insensitive-duplicate name reverts to the old
  key with no write. A valid rename keeps the row's position and value, then writes.
  **Disabled on read-only rows.**
- **Delete** — removes the `PropertyRow` from the list and writes immediately.
- **Reorder** — drag the grip; `PropertiesView` moves the row in its vector and writes.

### 4. Write / reconcile flow

All five interactions funnel through one `flushWrite()`:

1. Walk `m_rows` in current visual order. For each row build a `FrontMatterEntry`:
   - read-only row → `{key, {}, preserveFromDisk=true}`
   - editable row → `{key, <QVariant from editor->currentValue()>, false}`,
     using the existing `YamlValue`-kind → `QVariant` switch already in `flushWrite`.
2. Call `m_fmProxy->setFrontMatter(tf, orderedEntries)`.

Because the panel renders **every** frontmatter key, this wholesale write inherently
handles delete (absent rows drop), reorder (list order), and rename (changed key) with no
special-casing. The debounce and the `cacheChanged`-suppression-while-pending logic are
unchanged. After an external `cacheChanged` (not self-induced), `refresh()` rebuilds rows.

### 5. Lossy-value handling

During `refresh()`, classify each key's value:

- **Editable** — `Kind` is Bool / Int / Double / String, or a `Seq` whose elements are all
  scalars. Rendered with the matching editor as today.
- **Read-only / preserve** — `Kind::Map`, or a `Seq` containing any non-scalar element.
  Rendered as a greyed, non-editable summary label (e.g. `{…} (not editable here)` for a
  map, `[…] (not editable here)` for a complex list). The row still **drags** and
  **deletes**; it does **not** rename or edit. On write it emits `preserveFromDisk=true`,
  so `setFrontMatter` copies the value verbatim from disk.

This guarantees complex frontmatter round-trips byte-faithfully through any add / edit /
delete / reorder of *other* keys — fixing the pre-existing corruption described above.

The scalar-vs-non-scalar list check is a small helper on the view (or reused from
`inferPropertyType`'s caller); `inferPropertyType` itself is unchanged (it still returns
`List` for a `Seq` — the view decides editable-vs-readonly based on element shapes).

## Components & files

| File | Change |
|---|---|
| `libs/markoff-family/libs/markoff-parser/include/markoff/parser/YamlValue.h` | `setChildFrom` decl (Markoff submodule) |
| `libs/markoff-family/libs/markoff-parser/src/YamlValue.cpp` | `setChildFrom` impl via ryml `duplicate` (Markoff submodule) |
| `libs/vault/include/corbomite/vault/FileManager.h` | `FrontMatterEntry` struct + `setFrontMatter` decl |
| `libs/vault/src/FileManager.cpp` | `setFrontMatter` impl (reuses `applyVariantMapToYaml` + strip-empty branch) |
| `libs/vault/include/corbomite/vault/proxies/FileManagerProxy.h` | proxy decl |
| `libs/vault/src/proxies/FileManagerProxy.cpp` | proxy fwd, `vault.write`-gated |
| `src/sidebar/PropertyRow.{h,cpp}` | **new** row widget |
| `src/plugins/properties/PropertiesView.{h,cpp}` | `QFormLayout` → `QVBoxLayout` of `PropertyRow`; add dialog; reorder; lossy classification; `flushWrite` via `setFrontMatter` |
| `src/sidebar/CMakeLists.txt` / properties plugin CMake | add `PropertyRow` sources |

## Testing

Tests define expected behavior; fix code, not tests.

**Markoff layer — extend the `YamlValue` test suite** (in the submodule): `setChildFrom`
copies a scalar, a flat string-list, a nested map of scalars, and a list-of-maps verbatim
(stringify of the copied child equals stringify of the source); copying under an existing
key replaces it; copying appends as the last child (order check via `keys()`).

**Vault layer — `tst_setfrontmatter` (new, `libs/vault/tests/`)** — pure, no widgets:

- writes entries in given order → on-disk YAML key order matches the list
- omitting an on-disk key deletes it
- empty list strips the `---` block
- `preserveFromDisk` copies a nested-map value verbatim while sibling scalars are rewritten
  (the corruption-fix assertion: edit a scalar key, assert the nested map survives byte-equal)
- rename-shaped change (drop `old`, add `new` at same index) round-trips
- non-`.md` file returns false; null/missing args return false

**Plugin layer — extend `tst_properties_plugin.cpp`** (offscreen widgets):

- add-with-type creates a typed row and writes the key
- delete removes a row and erases the key on disk
- reorder changes persisted key order
- rename changes the key, preserving value + position
- a note with a nested-map frontmatter value: row is read-only; editing a *sibling* scalar
  leaves the map intact on disk (regression guard for the pre-existing bug)
- duplicate add / duplicate rename are rejected (no write)

Interactive drag and dialog flows that offscreen Qt can't fully drive are exercised at the
model/signal level (call the reorder/rename/delete slots directly) and flagged for user
eyeball, consistent with the D.2–D.4b verification backlog.

## Definition of done

- `setFrontMatter` + proxy shipped with `tst_setfrontmatter` green.
- PropertiesView supports add-with-type, edit, delete, rename, drag-reorder.
- Nested-map / complex-list values are read-only and preserved verbatim across edits of
  other keys (corruption fix verified by test).
- Full `ctest` green; clean build; offscreen launch smoke clean.
- Punch-list P2 item marked resolved; closeout in `decisions-archive.md`.
- Interactive paths (drag, add dialog, inline rename) flagged pending user eyeball.

## Blocks / enables

- Unblocks user-driven verification of P0 #1 (key-order preservation) and P0 #11
  (empty-frontmatter eviction) via a real editing surface — both cited in the punch-list item.
- Independent of the Markoff read-only-Live work (bucket ④) and of Cluster D.
