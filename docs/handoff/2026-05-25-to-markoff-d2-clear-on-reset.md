# Steer → Markoff: D2 block state not cleared on content reset/reload (content doubling)

**From:** Corbomite (downstream consumer)
**Date:** 2026-05-25
**Markoff pin:** `v0.7.0-freeze` (`1e0f332`)
**Severity (our view):** P0/P1 — silent **content corruption on save** in content-replacement flows.

---

## TL;DR

`MarkoffDocument::resetContent()` and `loadFromMarkdown()` both rebuild D2 block
state via `buildD2FromBytes()`, which **appends** parsed blocks on top of any
pre-existing D2 state **without clearing it first**. On a *fresh* document this
is correct; on an *already-populated* document it doubles the content —
`serializeForSave()` then writes **new content + stale old content** to disk.

You already flagged this exact gap in-code (`MarkoffDocument.cpp:745-746`):

> *"the right behavior is a full D2 wipe before rebuild; that requires IdList
> clear semantics the CRDT doesn't yet expose. Tracked as a follow-up."*

This is the steer asking you to close that follow-up. We believe it's the same
root cause as port degradation #1 ("editing causes content to repeat at end of
doc") from the 2026-05-20 recap.

## Reproduction (Corbomite headless integration tests)

`NoteDocument::setMarkdown(x)` → `markoff->resetContent(x.toUtf8(), Origin::TestFixture)`.
The flow is: open a note (D2 built via `loadFromMarkdown`), then replace its
content, then save.

```
open "test.md" (= "original content")     // loadFromMarkdown → D2 = [original]
setMarkdown("modified content")            // resetContent → buildD2FromBytes appends → D2 = [original, modified] (or [modified, original])
saveDocument → serializeForSave()
```

Actual bytes written to disk (three independent tests, same shape):

| input on disk | setMarkdown to | bytes saved (BUG) | expected |
|---|---|---|---|
| `original content` | `modified content` | `modified content\n\noriginal content\n` | `modified content\n` |
| `# Note 1` | `# Modified Note 1` | `# Modified Note 1\n\n# Note 1\n` | `# Modified Note 1\n` |
| `日本語 café 🎉 résumé` | `…\n\nMore text` | `…\n\nMore text\n\n日本語 café 🎉 résumé\n` | `…\n\nMore text\n` |

The stale original block survives the reset. (The CRDT *buffer* is correctly
replaced — `visibleLength()`/`flatView()` would show only the new content — but
the D2 block model that `serializeForSave()` iterates is not, so buffer and D2
diverge.)

## Root cause (your code)

- `resetContent()` (`MarkoffDocument.cpp:692`): rebuilds `d->buffer` correctly, then calls `buildD2FromBytes(newContent)` (`:744`) — the comment at `:738-746` documents that this populates D2 "on top of any pre-existing D2 state without clearing first."
- `buildD2FromBytes()` (`:1822`) → `materializeBlocksFromParsedDoc()` (`:1732`) — additive; no pre-clear of existing block IdList / block maps.
- `loadFromMarkdown()` (`:1851`) calls the same `buildD2FromBytes()`, so it shares the gap. It's safe today **only** because Corbomite calls it once on a freshly-constructed document (`Vault::openDocument`). Any second load/reset on the same document instance doubles.

## What we need

A way to **fully replace** a document's D2 block state from new bytes — i.e.
`buildD2FromBytes()` (or its callers) wipes the prior D2 block model before
materializing, for the wholesale-replace origins and for re-load. Concretely,
the contract we depend on:

> After `resetContent(B, origin)` or `loadFromMarkdown(B)` on a document whose
> D2 currently holds content A, `iterateBlocks()` / `serializeForSave()` reflect
> **only** B — no residual A blocks — regardless of whether the document was
> fresh.

This is the "IdList clear semantics" you noted the CRDT doesn't yet expose;
whether the fix is a new CRDT/IdList clear primitive, or a D2-layer teardown +
rebuild, is your call. We only depend on the observable contract above.

**Origins affected:** `ExternalReloadClean`, `ExternalReloadResolved`,
`UserRevertToSaved` (the wholesale-replace origins your comment names), plus any
`FirstOpen`/`TestFixture` reset on a non-fresh instance. The interactive
`UserEdit` path is **not** implicated (incremental D2 edits don't reset), which
is why ordinary typing doesn't double — but external-reload, revert-to-saved,
and programmatic content replacement (daily-note/template/`setMarkdown`) do.

## Acceptance test (falsifiable)

A non-fresh-reset round-trip, e.g. extending `tests/d2/tst_d2_reset_content.cpp`:

```cpp
MarkoffDocument doc;
doc.loadFromMarkdown("original content");          // D2 populated
doc.resetContent("modified content", Origin::TestFixture);
QCOMPARE(doc.serializeForSave(), QByteArray("modified content\n"));  // only B, no A
// and the same for an Origin::ExternalReloadClean reset, and for a second loadFromMarkdown().
```

## Not in scope (accepted, no change requested)

The single trailing `\n` from `finalDocumentTerminator()` (B1 convention) is
**fine** — Corbomite has accepted CommonMark-canonical terminal newlines. The
`\n` in the "expected" column above reflects that. This steer is *only* about
the stale-block doubling.

## What Corbomite does once this lands

Re-pin the submodule to the fixed Markoff tip and, in the same change, update
three integration tests for the accepted canonical-`\n` behaviour and the
now-correct (un-doubled) round-trip: `tst_editor_save`, `tst_vault_lifecycle`,
`tst_dailynoteservice`. Those test edits are specified in this steer + the
triage notes; we reverted them for now (the tree stays clean) rather than
commit tests that pass over the corruption. Until the fix lands,
`setMarkdown`-based replace and external-reload save paths are corruption-prone
on our side.

---

*Evidence-driven micro-spec per the port-first workflow. Happy to pair on the
acceptance test or the D2-teardown approach.*
