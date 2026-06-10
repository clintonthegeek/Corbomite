# KDevelop async-render patterns — findings for Cluster J

**Date:** 2026-04-15
**For:** Cluster J Phase 4 — ReadingView::EmbedRenderer + Core::EmbedDepthGuard
**Plan reference:** docs/superpowers/plans/2026-04-15-cluster-j-embed-rendering.md Task 4.1

## Observations

### (a) Multi-stage render with partial results

KDevelop stages rendering through a **worker-thread + signal pattern**:

1. **Foreground emits completion request** — `CodeCompletionModel::completionInvoked()` clears state, calls `completionInvokedInternal()`.
2. **Model emits to background** — `completionsNeeded(DUContextPointer, Cursor, View)` signals `CodeCompletionWorker` on a dedicated `QThread`.
3. **Worker computes incrementally** — `CodeCompletionWorker::computeCompletions()` calls subclass override; at any point calls `foundDeclarations(QList<CompletionTreeElement>, CodeCompletionContext)`.
4. **Each emission is partial** — `foundDeclarations()` emits `foundDeclarationsReal` back to foreground, which updates the QAbstractModel without clearing prior items. The model calls `rowCount()`, `data()` lazily.
5. **Lazy detail expansion** — Items provide `createExpandingWidget()` that is only called when the user selects a row; `data(role=Qt::DecorationRole)` is fetched synchronously on-demand.

**Precedent:** The pattern does NOT wait for full completion. Each `foundDeclarations()` call updates the visible list immediately; UI shows "gathering…" and refines as items arrive.

### (b) Widget lifecycle tied to view state

`AbstractNavigationWidget` demonstrates tight coupling:

1. **Lifecycle ownership:** Widget is owned by context (via `NavigationContext::widget()` returning a QWidget*). Parent ownership is explicitly avoided in comments.
2. **Context lifetime:** Widget is shown/hidden with `update()` which calls `context->html()` under `DUChainReadLocker`. If context changes, prior widget is explicitly removed from layout and reparented to null: `layout()->removeWidget(d->m_currentWidget); d->m_currentWidget->setParent(nullptr)`.
3. **Transient state:** `m_browser` (QTextBrowser) is only created on first `setContext()` call (lazy init). Size hints are cached (`m_idealTextSize`) and cleared on context change.
4. **Signal bridging:** If new widget has `navigateDeclaration(IndexedDeclaration)` signal, it's wired to the navigation widget via `QMetaObject::normalizedSignature()` + `indexOfSignal()`.
5. **Parent-aware deletion:** `~AbstractNavigationWidget` explicitly removes widgets from layout to avoid crashes on parent deletion.

**Pattern:** Avoid QObject parent-child ownership for transient widgets. Use explicit layout management + `QPointer` for safety.

### (c) Recursion-guard / embed-depth pattern

KDevelop's **UsesCollector** (`duchain/navigation/usescollector.h`) implements visited-set recursion guards:

1. **QSet<IndexedTopDUContext> m_checked** — Populated in `updateReady()` before recursing into imports/importers.
2. **Comment:** "To prevent endless recursion in updateReady()".
3. **Pattern:** Uses `Algorithm::insert(visited, current).inserted` to test-and-add in one operation; if already visited, skips.

`AbstractIncludeNavigationContext` notes header-guard logic: "This prevents picking a context that is empty due to header-guards" — checking contexts for non-empty status before rendering.

**No depth counter found.** KDevelop uses set-membership, not numeric depth limits. The pattern is: collect unique contexts in a QSet, skip if already seen.

## Adopt

1. **Worker-thread + signal pattern for async stages** — Emit partial results via signals; each emission updates UI immediately without waiting for completion. Fits Corbomite::EmbedRenderer well.

2. **Lazy detail expansion** — Don't compute full HTML/layout until user requests it. Use `createExpandingWidget()` model for "Show more" expansion.

3. **Explicit widget reparenting on context switch** — Don't rely on parent ownership. Manage layout insertion/removal directly + set parent to nullptr before discarding.

4. **QPointer<> for transient widgets** — Safe sentinel against double-delete if parent is deleted unexpectedly.

5. **QMetaObject-based signal wiring** — Check for signal existence at runtime via `indexOfSignal()` instead of assuming. Allows plugins to opt-in to navigation signaling.

6. **Visited-set recursion guard** — Use `QSet<IndexedSomething>` + `Algorithm::insert().inserted` pattern for cycle detection in nested traversals.

## Do not adopt

1. **Dedicated QThread per model** — KDevelop creates a `CompletionWorkerThread : QThread` for each `CodeCompletionModel`. Overkill for Corbomite; use `QThreadPool` + `QFutureWatcher` or async function callbacks instead.

2. **Raw DUChain locking** — `DUChainReadLocker` is KDevelop-specific. Corbomite has its own sync strategy; don't copy the locking pattern wholesale.

3. **Foreground grouping in model** — KDevelop groups items twice (background + foreground). Corbomite's simpler embed hierarchy doesn't need this complexity.

4. **NavigationContext tree stacking** — The `previousContext()` navigation stack for drill-down is useful but ties too tightly to KDevelop's declaration-centric model. Corbomite embeds are note-centric (simpler); consider a flatter structure.

## Open questions

1. **Where does EmbedDepthGuard live?** — Cluster J plan says Core, but is it a standalone class or a member of EmbedRegistry? The plan mentions it in Phase 1, Task 1.4.

2. **Partial rendering: incremental tree vs. deferred content?** — Should embeds emit partial tree structure (e.g., title + stub, then body later), or defer full HTML generation until expanded? Plan Phase 4 will clarify.

3. **Threading model for EmbedRenderer** — Plan targets Qt6 + QFutureWatcher. Should each embed launch its own future, or batch them in a global pool? Worker thread vs. thread pool?

4. **Signal emission scope** — KDevelop's `foundDeclarations()` is a public API; Corbomite's EmbedRenderer should clarify whether partial-result signals are public (plugins hook) or internal (EmbedRegistry uses them).

5. **Lifecycle: who owns the rendered widget?** — Should EmbedRenderer return a QWidget* that the embedding context owns, or a QSharedPointer? KDevelop avoids parent ownership; Corbomite should document this explicitly in Phase 1.
