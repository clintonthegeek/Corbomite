# Cluster Q.0 — What actually happened

> Sibling to the structured retro at `cluster-q0.md`. Higher-altitude
> narrative answering "what the heck did we even do?" along five axes.
> Written 2026-04-17 in the closing session.

---

Honestly? We did a **domain-model refoundation** — the kind of move that usually takes a team a quarter and is most often attempted only once in a codebase's life. Here's what it means along each axis:

**Relative to Obsidian.** Obsidian exposes a single `App.vault` object to plugins: `vault.read(file)`, `vault.modify(file, body)`, `vault.on("modify", ...)` — everything funnels through one aggregate, and files move around the system as `TFile` / `TFolder` handle objects that carry their own metadata. Pre-Q.0 we had no object of that shape. A plugin written against Obsidian's API couldn't have been ported to Corbomite without a rewrite. Now it can. That's the point of the whole refactor: Obsidian-shape isn't cosmetic, it's ABI.

**Relative to our docs.** The 94k-word reverse-engineering audit at `docs/obsidian-audit/` already said what Vault should look like. `GAP-ANALYSIS.md`, `PLUGIN-API-SKETCH.md`, and `VAULT-FORMAT.md` were aspirational — they described a shape the code didn't have. Q.0 made the code match the audit. Spec followed reality up until now; now reality has caught up to spec.

**Relative to our original code.** Three major classes died (`VaultModel`, `VaultService`, `NoteService`) plus five helpers (`FrontMatterWriter`, `VaultProcess`, `VaultTrash`, `FileWatchReactor`, the Task-7 `Vault` stub). These weren't duplicates — each had a real job — but the jobs overlapped in ways that muddied responsibility. `VaultModel` was both "QAbstractItemModel for tree views" and "vault data owner," which are two jobs. We unbundled: tree-model duties stayed in `NotesTreeModel`, data-owner duties moved to canonical `Vault`. The vault layer went from 5+ classes in 3+ libraries to 1 library with clear homes for everything.

**Relative to Qt norms.** We explicitly rejected several Qt idioms. (1) The canonical `Vault` isn't a `QAbstractItemModel` — it's a domain object; the Qt model is downstream. (2) We use `std::unordered_map<QString, std::unique_ptr<T>>` instead of `QHash` because QHash's rehash requires copy-constructible values — the `unique_ptr` ownership discipline beats the Qt-native choice. (3) The filesystem watcher folded *into* the aggregate (`detail::Watcher`) instead of staying a separate QObject — Qt idiom exposes watchers, but our watcher's only legitimate consumer was the Vault itself. (4) Self-write echo-suppression via a `(path, mtime)` ledger with 1-second expiry — a systems-programming pattern, not a Qt idiom; Qt's usual `blockSignals` doesn't work across async filesystem events.

**Relative to typical software development.** This is the part that's actually rare:

- **11 phases, ~50 commits, zero new user features.** Most teams can't justify that kind of pure-architecture sprint politically.
- **Test count went *up*.** 181 tests at the end — no net test loss. Refactors usually lose tests because the tests were married to the old shape.
- **Phase 10 landed before Phase 9.** Numbered-order-is-sacred would have produced worse work. We reordered because the cleanup phase produced a cleaner substrate for the new-surface phase.
- **All on master, serialized.** No long-lived feature branch, no giant squash-merge. Each phase independently green, independently bisectable. Most teams would have branched.
- **Plan deviations were documented, not hidden.** Seven of them in the retro. The plan was treated as editable by reality, not a contract.
- **A "close-out phase" was a real phase**, not an afterthought — T11.1 (retarget Cluster Q plan) + T11.3 (retro + PROJECT-STATE + memory) — because documentation decay across sessions is the default failure mode and we treat it as preventable.

**The one-sentence version:** we replaced a domain model that had grown accidentally over six months with one that was designed deliberately from an audit, moved ~1500 lines of code between four libraries, deleted eight classes, shipped a permissioned plugin facade over the result, and wrote down why — all without breaking a test or stopping shipping on master.

That's what happened.
