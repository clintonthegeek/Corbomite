# Cluster D.1 — Bases backend correctness: vault-bound functions, tags, sort

**Date:** 2026-05-25
**Cluster:** D (Bases UI completion) — sub-project **D.1**, the backend/value-layer foundation.
**Status:** Shipped 2026-05-25.
**Substrate:** Independent of the Markoff foundation rewrite (Bases is `QTableView`-based).

## Context

Cluster K (legacy) shipped the Bases runtime as MVP: a full Pratt lexer/parser/AST, a 19-type `Value` hierarchy, evaluator, `FunctionRegistry`/builtins, and the query/filter engine (~6,300 LOC in `libs/bases`). Several **correctness** gaps remain in the evaluator/value layer — formulas and filters silently misbehave because vault-bound functions are stubbed and hierarchical tag matching is wrong. D.1 closes those gaps so that the read-side rendering (D.2) and editing UI (D.3) sub-projects sit on a correct evaluation core.

Two audit-listed gaps have already drained since the 2026-04-26 audit and are **not** in scope: YAML key-order (→ Cluster A) and the `this`-binding-never-updates bug (→ P5 Bases Phase 2, `setCurrentFile` now wired to `activeLeafChanged`).

## Goal

The Bases evaluator resolves vault-bound functions against the vault and matches hierarchical tags per Obsidian semantics, with the behaviour pinned by unit tests. Specifically: `file("Notes/X.md").mtime` returns a real value; `LinkValue.asFile`/`linksTo` resolve link targets; `file.tags.contains("#parent")` matches a stored `#parent/child` (and not the reverse); list sort and loose-equals semantics are verified and pinned.

## Scope

**In scope (evaluator/value layer only):**
1. Vault-binding seam: a `VaultResolver` interface surfaced via `EvalContext`.
2. `file()` global, `LinkValue::asFile`, `LinkValue::linksTo` — rewritten to use the seam.
3. Hierarchical tag matching: directionality fix + `ListValue::includes`/`contains` special-casing for `TagValue` elements.
4. `ListValue::sort()` null-ordering — verify and pin.
5. `looseEquals` — verify against the DSL addendum and pin.

**Out of scope:**
- `unrecognizedData` non-scalar preservation — a *serialization* round-trip fidelity fix (sibling of the key-order work that went to Cluster A). Tracked as a standalone punch-list item, not D.1.
- All UI (group rendering, formula editor, filter builder, etc.) — D.2/D.3.
- Plugin-API surfaces (`registerGlobalFunc`/`registerInstanceFunc`/`registerView`) — D.5.
- Per-row incremental re-evaluation perf (audit structural #7) — separate performance concern.

## Design

### 1. The `VaultResolver` seam

The evaluator passes `const EvalContext&` to every builtin. Today `EvalContext` exposes only `getByIdentifier()` + `keys()`; `BasesEntry` (the concrete row context) holds `Vault*` + `MetadataCache*` but does not surface them, so the vault-bound builtins cannot reach the vault and return null/string fallbacks.

Introduce a narrow abstract interface in `libs/bases` and one accessor on `EvalContext`:

```cpp
// VaultResolver.h (libs/bases) — no dependency on Qt Vault/MetadataCache types
class VaultResolver {
public:
    virtual ~VaultResolver() = default;

    /// Resolve a vault-relative path (or bare name) to a FileValue; NullValue if absent.
    virtual ValuePtr fileAt(const QString& pathOrName) const = 0;

    /// Resolve a wiki/markdown link's data to a canonical vault path; "" if unresolved.
    virtual QString resolveLinkTarget(const QString& linkData) const = 0;
};

// EvalContext gains:
virtual const VaultResolver* vault() const { return nullptr; }   // default: unbound
```

`BasesEntry` implements `VaultResolver` (directly or via a small adapter), backing `fileAt`/`resolveLinkTarget` with its `MetadataCache` resolved-link data and `Vault` path lookup, and returns it from `vault()`. Contexts without a vault (tests, `LambdaContext`, summary/shadowing contexts) inherit the `nullptr` default.

**Rationale (decided during brainstorming):** keeps the `EvalContext` interface — which the whole evaluator depends on — small and stable; confines the `Vault`/`MetadataCache` coupling behind a single seam `BasesEntry` already has the data to implement; and gives future vault capabilities (e.g. `tagsFor`) a home without touching `EvalContext`. The seam is also the test lever (§5).

### 2. Builtins rewritten to use the seam

Each builtin uses the resolver when present and keeps today's fallback when unbound:

- **`file(path)` global** — `if (auto* v = ctx.vault()) return v->fileAt(toStr(args[0])); return NullValue::instance();`
- **`LinkValue::asFile`** — resolve the link to a path then to a file: `fileAt(resolveLinkTarget(lv->data()))`; `NullValue` if either step fails or unbound.
- **`LinkValue::linksTo(other)`** — compare `resolveLinkTarget(lv->data())` against the resolved/normalized `other` by canonical path. When unbound, fall back to today's string equality (`lv->data() == other`).

### 3. Hierarchical tags (Option 1: special-case `includes`)

- **Directionality fix.** `TagValue::tagMatches(other)` currently matches bidirectionally. Correct it to one-directional Obsidian semantics: `this` matches `other` iff `this == other` **or** `this.startsWith(other + "/")` (the stored tag is `other` or a subtag of it). Drop the reverse (`other.startsWith(this + "/")`) branch.
- **`ListValue::includes`/`contains`.** When a list element is a `TagValue`, compare via `element.tagMatches(needleString)` instead of generic value-equality; otherwise unchanged. `file.tags` already yields a `ListValue` of `TagValue`s, so no producer plumbing changes.

Result: `file.tags.contains("#parent")` matches a stored `#parent/child`; `file.tags.contains("#parent/child")` does **not** match a stored `#parent`.

### 4. `sort()` and `looseEquals` — verify and pin

- `ListValue::sort()` reads as already sinking nulls **last** (consistent with `BasesQueryResult::compareValues`). D.1 confirms via a `sortNullsLast` test and changes the comparator only if the test proves otherwise. Do not assume the audit's "nulls first" claim without evidence.
- `looseEquals` appears honoured via the symmetric `staticLooseEquals` (`Value.cpp`). D.1 audits the per-type behaviour against the DSL addendum and pins it with tests; no pre-emptive rewrite.

### 5. Testing

The `VaultResolver` seam is the lever: tests inject a **fake resolver** (an in-memory map of path→FileValue and linkData→path) without constructing a Qt `Vault`/`MetadataCache`. New cases:

- `fileAtResolvesPath` / `fileUnboundReturnsNull` — `file()` with and without a resolver.
- `asFileResolvesLinkTarget` — `LinkValue.asFile` round-trips link→file.
- `linksToCanonical` — `linksTo` matches on resolved path, not raw string; plus the unbound string-equality fallback.
- `tagsContainsHierarchical` — `contains("#parent")` matches `#parent/child`; `contains("#parent/child")` does not match `#parent`.
- `tagMatchesOneDirectional` — direct `TagValue::tagMatches` unit assertions in both directions.
- `sortNullsLast` — pins null ordering.
- `looseEquals*` — per-type loose-equality assertions per the addendum.

All existing `libs/bases` tests must stay green.

## Definition of done

- `file()`, `LinkValue::asFile`, `LinkValue::linksTo` resolve against a (fake or real) vault; unbound contexts keep safe fallbacks.
- `file.tags.contains("#parent")` is hierarchical and one-directional.
- `ListValue::sort()` null ordering and `looseEquals` semantics are pinned by tests (fixed only if proven wrong).
- New unit tests pass; full `libs/bases` suite green.
- No UI changes; no dependency on the Markoff foundation work.

## References

- [audit-2026-04-26/bases.md](../../audit-2026-04-26/bases.md) — §"Missing (prioritized)" (structural/correctness items), §"Notable concerns / suspected bugs".
- [obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md](../../obsidian-audit/addenda/2026-04-17-bases-formula-dsl.md) — formula DSL reference (loose-equals, tag, link, file semantics).
- Cluster D stub: [plans/2026-04-26-cluster-d-bases-ui-completion.md](../plans/2026-04-26-cluster-d-bases-ui-completion.md) — D.1 is the first of ~5 sub-projects (D.2 read-side rendering, D.3 editing UI, D.4 interactivity/export, D.5 plugin API).

## Current-state anchors (verified 2026-05-25)

- `EvalContext` interface: `libs/bases/include/corbomite/bases/EvalContext.h`.
- `BasesEntry` (holds `Vault*` + `MetadataCache*`): `libs/bases/include/corbomite/bases/BasesEntry.h`.
- Stubs: `file()` global `libs/bases/src/Builtins.cpp:166`; `asFile`/`linksTo` `Builtins.cpp:564,571`.
- Tags: `TagValue::tagMatches` `libs/bases/src/StringSubclasses.cpp:11`; `ListValue::includes` `libs/bases/src/ListValue.cpp:49`; `sort()` `ListValue.cpp:103`.
- `FileValue::looseEquals` `libs/bases/src/FileValue.cpp:34`.
