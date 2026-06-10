# Markoff link-emission inventory — findings for Cluster J

**Date:** 2026-04-15
**For:** Cluster J Phase 3 — `Markoff::LinkRenderer` consolidation
**Plan reference:** `docs/superpowers/plans/2026-04-15-cluster-j-embed-rendering.md` Task 0.3 + Task 3.4

## TL;DR — scope surprise

The consolidation plan in Phase 3 assumed there were many scattered ad-hoc link-emission paths in `libs/markoff/` to collapse onto a single `LinkRenderer`. Reality is more nuanced:

- **There are only 6 emission points**, and they're actually architecturally clean already.
- **No `"bases"` hardcode exists anywhere in `libs/markoff/`.** Phase 3's "honesty regression test" is a *prevention* gate, not a *cleanup* gate.
- **Wikilinks aren't clickable today.** `MarkdownHighlighter` doesn't set `QTextCharFormat::anchorHref` for `[[target]]` spans, so `TextControl` can't recognise them as clickable anchors.
- **`Editor::linkClicked` is declared but never emitted.** A real dormant bug: the signal exists on the public API; no call-site fires it. `MainWindow.cpp:137` connects to it with a `qDebug` stub that never runs.
- **No `TextControl → Editor` bridge exists.** `TextControl::linkActivated` fires for standard markdown links; nothing connects that to `Editor::linkClicked`.

**Consequence for Phase 3:** scope shifts from "consolidate" to "build". The work becomes:
1. Add a `Markoff::LinkRenderer` class as a typed emission surface.
2. Bridge `TextControl::linkActivated` → `LinkRenderer::emitExternalLink` / `emitFileLink`.
3. Make wikilinks clickable by setting `anchorHref` in `MarkdownHighlighter`.
4. Make `Editor::linkClicked` actually fire, routed through `LinkRenderer`.
5. Add honest per-caller source strings to every emission.
6. Keep `linksChanged` / `tagsChanged` / `wikiLinkTrigger` / `tagTrigger` unchanged — they're orthogonal metadata + completion signals.

This is still within Phase 3's scope umbrella, but the phase is *feature completion* rather than *refactor-consolidation*. The test in Task 3.4 Step 2 (no `"bases"` hardcode) becomes trivially true from day zero; a different integration test is needed to validate the wikilink-clickability feature.

## Call-site table

| # | File:line | Class::method | Signal / API shape | Source string | Notes |
|---|---|---|---|---|---|
| 1 | `libs/markoff/src/TextControl.cpp:2311` | `TextControlPrivate::activateLink()` | `emit q->linkActivated(href)` → `TextControl::linkActivated(const QString &)` | none (TextControl is generic) | Qt-internal QWidgetTextControl path; fires for `anchorHref`-tagged spans. Today only standard markdown/HTML links produce these. |
| 2 | `libs/markoff/src/TextControl.cpp:2319` | `TextControlPrivate::updateHighlightedAnchor()` | `emit q->linkHovered(anchor)` → `TextControl::linkHovered(const QString &)` | none | Fires on mousemove over an `anchorHref` span. |
| 3 | `libs/markoff/src/TextControl.cpp:2327` | `TextControlPrivate::resetHighlightedAnchor()` | `emit q->linkHovered(QString())` | none | Fires when the cursor leaves an anchor span; empty-string signals "left". |
| 4 | `libs/markoff/src/Editor.cpp:618` | `Editor::onDocumentReparsed()` | `Q_EMIT linksChanged(m_document->links())` → `Editor::linksChanged(const QList<Markoff::LinkInfo> &)` | n/a | Metadata broadcast, not per-click/hover. Correct as-is. |
| 5 | `libs/markoff/src/Editor.cpp:619` | `Editor::onDocumentReparsed()` | `Q_EMIT tagsChanged(m_document->tags())` → `Editor::tagsChanged(const QList<Markoff::TagInfo> &)` | n/a | Metadata broadcast. Correct as-is. |
| 6 | `libs/markoff/src/Editor.cpp:949` | `Editor::onTextInserted()` | `Q_EMIT wikiLinkTrigger(cursor.position())` | n/a | Fires when user types the second `[`. Completion trigger, not a link emission. Orthogonal. |
| 7 | `libs/markoff/src/Editor.cpp:952` | `Editor::onTextInserted()` | `Q_EMIT tagTrigger(cursor.position())` | n/a | Fires when user types `#`. Completion trigger. Orthogonal. |
| 8 | `src/MainWindow.cpp:137` (consumer-side) | `MainWindow::connectEditor()` | `connect(m_editor, &Markoff::Editor::linkClicked, ...)` with `qDebug()` lambda | n/a | **Dead connection.** `Editor::linkClicked` is declared in the public API header but never emitted anywhere. |

## Existing ambient state

- **`Markoff::Document`** (from `libs/markoff-parser/`) already provides structured `LinkInfo` (type, target, displayText, sourceOffset) and `TagInfo`. Emission sites in the consolidation need not re-parse — they read from Document.
- **`MarkdownHighlighter`** decorates link spans visually but does not set `QTextCharFormat::anchorHref` on wikilinks. Standard markdown links (`[text](url)`) go through Qt's built-in markdown path and do get `anchorHref` set automatically.
- **`TextControl`** inherits QWidgetTextControl; emits `linkActivated` / `linkHovered` from Qt's internal machinery when an `anchorHref` span is clicked/hovered.
- **`MarkdownTextItem`** owns a `TextControl` but does not subscribe to its link signals (opportunity).
- **`SceneCoordinator`** coordinates text items; no link-awareness currently.
- **`Editor`** is the central hub; it subscribes to `SceneCoordinator::reparsed()` to emit metadata broadcasts but does not consume `TextControl::linkActivated` events from the items the scene owns.

## Consolidation plan for Phase 3

Three real pieces of work, each small but each new-feature-shaped:

**(a) Emission chokepoint in `Editor`.** Add `Markoff::LinkRenderer` as a member of `Editor`. For each `MarkdownTextItem` in `SceneCoordinator`, subscribe to its `TextControl::linkActivated` / `linkHovered` signals and route them through `LinkRenderer`. Honest source string: `"markoff:editor"` (or differentiate per-mode: `"markoff:source"`, `"markoff:livepreview"`).

**(b) Wikilink annotation in `MarkdownHighlighter`.** Extend the highlighter so wikilink character ranges (`[[target]]`) are tagged with a synthetic `anchorHref` such as `wikilink://target`. The `TextControl` emission path (item 1) then recognises them and fires `linkActivated` through the new chokepoint. `LinkRenderer` parses the `wikilink://` scheme and routes to `emitFileLink` (with `fromPath` populated from the Editor's current-note context); standard links route to `emitExternalLink`.

**(c) Fire `Editor::linkClicked`.** Inside the `LinkRenderer`-routed bridge, also emit the legacy `Editor::linkClicked(target)` signal for backward compatibility with any existing consumer (MainWindow.cpp:137). Deprecation of this signal can be a later follow-up.

## "Leave alone" items

- `Editor::linksChanged` and `tagsChanged` — broadcast metadata updates; not link-emission per se. Keep.
- `Editor::wikiLinkTrigger` and `tagTrigger` — completion triggers. Keep.
- `Document::links()` and `Document::tags()` — parser outputs. Out of scope for J.

## Open questions (flagged for plan Task 3.4 / Task 3.5)

- **Should tags be clickable?** Today they have completion-trigger signals but no click-to-search-tag path. Probably out of J's scope; flag as Cluster D follow-up (tag search UX) rather than J work.
- **Should wikilink activation differ from external-link activation?** Ctrl-click vs plain-click, new-pane-vs-same-pane — this is an app-layer UX question, not a Markoff-library question. J emits the honest signal; the app decides how to route.
- **Should the `wikilink://` scheme be exposed publicly or kept as an internal implementation detail?** Probably internal — the `anchorHref` is a mechanism, not an API. Plan's `LinkRenderer::emitFileLink` stays the public surface.

## Test recommendations (revised)

Phase 3 Task 3.4 Step 2's "no `"bases"` hardcode" assertion is trivially satisfied from day zero (it's a regression gate). Add a positive integration test that validates the new feature:

```cpp
void testWikilinkBecomesClickable() {
    Editor editor(...);
    editor.setPlainText("See [[Target]]");
    // Simulate hover on the [[Target]] span
    QSignalSpy spy(&editor.linkRenderer(), &Markoff::LinkRenderer::linkHovered);
    // ... trigger hover via test harness ...
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first()[0].toString(), QStringLiteral("Target"));
    QVERIFY(spy.first()[1].toString().startsWith("markoff:"));  // honest caller id
}
```
