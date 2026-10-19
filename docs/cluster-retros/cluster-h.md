# Cluster H — Menus / hover / suggester UI (retrospective)

**Landed:** 2026-04-15, 5 commits across the 6 phases (Phases 4 and 5 bundled into one commit). Plus 1 state-update commit.

## What changed vs the original plan

Mostly faithful. Two scope adjustments:

1. **MenuSectionHelper migration scope.** The plan said "wire existing right-click paths through MenuEventEmitter" — I migrated FileExplorerPanel as the canonical exemplar but left the other 5 sites (EditorViewSpace tab bar, CanvasScene, Markoff Editor, TextControl, CorbomiteMDI Sidebar) unchanged. Pragmatic: each refactor is mechanical but touches deep widget internals; the substrate exists, and the remaining sites can migrate one-by-one as their owning code is otherwise touched. Tracked under follow-ups.

2. **RibbonSlot integration deferred.** Class is built and tested but not docked in MainWindow. CorbomiteMDI's existing layout doesn't have a natural left-vertical strip; adding one without disrupting the file/search/etc tool views is a UX call I'm not equipped to make solo. Tracked under follow-ups; build when first user/plugin needs it.

3. **SuggestPopup delegate stayed CompletionPopup.** The plan said "Delete or subsume CompletionPopup". I kept CompletionPopup as the rendering widget and made the EditorSuggest framework feed it — saves a major refactor. The proper SuggestPopup with per-suggester `renderSuggestion()` delegate is queued for when the first plugin (or built-in) wants rich-content suggestions.

## What surprised

- **The audit's "300ms not 500ms" Pass 1 correction was the right call.** The published Obsidian docs and several plugin authors get this wrong; following the audit's explicit correction in the HoverPopover constant kept us on-spec.
- **Compound operator names dictate tokenizer detail.** Came up in Cluster D (`match-case` etc.); didn't recur in Cluster H but reinforced the lesson — when porting Obsidian behavior, the *boundary cases* are what carry the compat invariants. The same shape appeared in `addRibbonIcon`'s "keys-on-title" silent-collision behaviour: a tiny rule that's load-bearing for plugin compat, easy to skip if you're not reading the audit carefully.
- **EditorSuggest-as-Component subclass works cleanly.** The Cluster C `Component` lifecycle slotted into `EditorSuggest` with no friction — onload/unload semantics carry over verbatim. Confidence-builder for the Component primitive.
- **Markoff already exposes `linkHovered`** (with empty-target-on-leave). Saved building a hover-detection layer in the editor; the framework just connects to the existing signal.

## Downstream effects

- **Cluster N (plugin-ready surfaces) is now strongly enabled.** All four registries (HoverLinkSourceRegistry, EditorSuggestManager, RibbonSlot, MenuEventEmitter) are populated with built-ins and ready to accept plugin-side registrations when the plugin layer arrives.
- **Cluster I (MetadataCache parity)** has a clearer downstream consumer now — TagSuggest and WikiLinkSuggest are early consumers of `vault.allTags()` / `vault.allNotes()`; once MetadataCache parity lands, both will get richer (frontmatter `tags:` merge, alias-aware wiki-link resolution).
- **Cluster J (embed/rendering)** has a concrete consumer for ReadingView: HoverPopover currently uses `QTextBrowser::setMarkdown()` for previews. The Markoff::ReadingView spec exists; the swap will be a small follow-up.

## Lessons for the next cluster

When the plan's scope spans both substrate (libs/core registries) and integration (MainWindow wiring), be explicit about the integration depth in each phase. Phase 4's "RibbonSlot" was ambiguous about whether docking-in-MainWindow was required; deferring it was the right call but I should have noted it as out-of-scope upfront rather than discovering it during implementation.

Build registry classes with their `registerBuiltins()` helper from the start — having the same code path that plugins will eventually use *also* register the shipped defaults gives free coverage of the plugin-facing API on every test run.

## Post-closeout, 2026-10-18 — the toast is gone; messages got a channel

Phase 5's `Notice` was a floating tooltip anchored to the **screen's**
bottom-right. It was never wrong in the middle of its own window, but it was
wrong in the two cases that matter: a message about a document could be drawn
outside the app's window (seen live, over a terminal, while the app was
minimised), and it left no trace once dismissed — if you looked away, the
message was simply gone.

Replaced by `MessageService` (app layer, owned by `MainWindow`), which is now
the only way the app tells the user something: the status bar's transient area
for everything, a persistent alert entry in the status bar for warning/error,
and a desktop notification for warning/error **only when the window is not in
front of the user** — active *and* not minimised, since a minimised window
still reports active under XWayland. `Notice` and its test are deleted; the
five `Notice::post` sites in `MainWindow` migrated (plugin load failure, merge
failure, view restore failure, metadata-cache open failure, document save
failure).

Two things worth carrying forward from doing it:

- **The layer was right; the sink was wrong.** Keeping `Notice::post`'s
  fire-and-forget shape was fine — what needed fixing was *where* it drew and
  *what else* it did. The service keeps the same call ergonomics
  (`postFrom(context, text, severity)`) and adds the sinks.
- **KNotification is the framework-shaped API and it does not work here.**
  `KNotificationPermission::checkPermission()` reported Granted and
  `sendEvent()` produced no `Notify` call on the session bus at all, while an
  identical `notify-send` call in the same window arrived and popped up. The
  dev build has no installed desktop file for the daemon to attribute the
  notification to (punch-list P6) and KNotification swallows that silently.
  The shipped transport is therefore a direct `org.freedesktop.Notifications`
  call over QtDBus, with the framework dependency dropped again. Before adding
  a framework "for correctness", check that the framework's writer actually
  writes under our install condition.
