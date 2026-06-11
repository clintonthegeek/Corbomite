# Hover Preview Re-light — Design

**Date:** 2026-06-11
**Status:** Approved (brainstorming) — ready for plan
**Track:** road-to-dogfood Phase 2
**Supersedes the hover-preview slice of:** [`2026-05-29-styled-headless-rendering-convergence-design.md`](2026-05-29-styled-headless-rendering-convergence-design.md) §3 (the third, unimplemented surface)

## Problem

`HoverPopover` has a complete, tested state machine (300 ms delay → Pending →
Visible, 500 ms grace timer, Ctrl-to-pin, Esc / outside-click dismiss) but is
**dark on both ends**:

1. **Content is gone.** It was built on `Markoff::Reading::ReadingView` +
   `EmbedRenderer`, both retired in the foundation port. Today `m_view == nullptr`
   and `renderTarget()` is a no-op. The two render/pinning tests are
   `if(FALSE)`-disabled in `tests/editor/CMakeLists.txt`.
2. **No trigger.** `MainWindow` constructs the popover and pushes it into every
   `MarkdownView`, but nothing ever calls `scheduleShow()` — so it never fires
   (PARITY-MATRIX: "scheduleShow never fires").

Goal: hovering a `[[wikilink]]` in the running app shows a live styled preview,
end-to-end, in Live and Reading modes.

## Key discovery — the seams already exist

- `Markoff::LinkService` (markoff-core) already exposes the exact signals we need:
  - `linkHovered(const LinkActivation&, const QPoint& globalPos)`
  - `linkHoverLeft(const QString& linkText)`
- **Both** editor leaves already pump those signals into the *shared*
  `m_linkService`:
  - Live: `LiveListModelBinding::hoverLinkAt` → `notifyHover`/`notifyHoverLeft`.
  - Reading (Styled): `markoff-styled`'s `LinkInteraction` (viewport has
    `setMouseTracking(true)`) → `notifyHover`/`notifyHoverLeft`.
  - In `NoteEditorWidget`, the Live binding and the Styled reading leaf are both
    wired to the same `m_linkService` instance.
- `MainWindow` already owns a stateless `StyledRenderEngine` (`m_cardRenderEngine`,
  used for canvas cards) and a `VaultResourceProvider` (`VaultScopedResources`)
  whose `resolveEmbed(name) → std::optional<QString>` returns a note's markdown
  bytes.

**Consequence: no upstream Markoff work and no submodule re-pin.** Trigger is pure
Corbomite signal wiring; content reuses already-shipped Corbomite components.

## Design

### 1. Trigger wiring — `NoteEditorWidget`

Alongside the existing `m_linkService::linkActivated → onLinkActivated`
connection, add two connections in the constructor:

- `linkHovered(act, globalPos)` → `m_hoverPopover->scheduleShow(<target>, globalPos)`
- `linkHoverLeft(text)`         → `m_hoverPopover->linkHoverEnded()`

The hover target string mirrors `onLinkActivated`'s rule: prefer
`activation.page` (resolved wikilink note name), else `activation.rawText`;
skip `LinkKind::External` (no preview for `http`/`mailto`). Because both leaves
share `m_linkService`, this single pair of connections lights up **Live and
Reading** at once. Source (Qutepart) does not participate — matches Obsidian
(no preview in raw source mode). The state machine is untouched.

**UX:** always-on hover (any internal link previews after 300 ms). No modifier
gate in v1 (tracked as a follow-up if it proves noisy while editing).

### 2. Content path — `HoverPopover`

Replace the retired render path:

- **API change:**
  - Remove `setEmbedRenderer(Markoff::Reading::EmbedRenderer*)`.
  - Add `setRenderEngine(MarkdownRenderEngine*)` (non-owning) — the headless
    `StyledRenderEngine`.
  - Replace `setVault(Vault*)` with
    `setResources(Core::VaultResourceProvider*)` (non-owning) — supplies
    `resolveEmbed(name) → std::optional<QString>` for target → markdown bytes.
- **Display widget:** a read-only `QTextEdit` (`m_display`) added to the existing
  layout, replacing the null `m_view`. `setReadOnly(true)`, no frame, the
  popover's `QGraphicsDropShadowEffect` stays.
- **`renderTarget(target)`:**
  1. `splitTarget(target, &path, &subpath)` (helper already present).
  2. `auto md = m_resources ? m_resources->resolveEmbed(path) : std::nullopt;`
  3. If no engine or `!md`: show a typed placeholder
     (`"(unresolved: <target>)"`) — graceful, never blank-crash.
  4. Else: `m_rendered = m_renderEngine->render(*md);`
     `m_display->setDocument(m_rendered->document());`
  5. Keep `m_rendered` (`std::unique_ptr<RenderedDocument>`) alive as a member —
     it owns the `QTextDocument` the widget displays.
- **Subpath slicing (`#heading` / `#^block`) is deferred.** v1 renders the whole
  resolved note. Tracked as a follow-up; whole-note render is the graceful
  default.

### 3. Host wiring — `MainWindow`

- Replace the commented-out `setEmbedRenderer(...)` calls (construction + vault
  open + vault close) with `setRenderEngine(m_cardRenderEngine.get())` and
  `setResources(<vault resource provider>)`.
- Ensure the `VaultResourceProvider` instance fed to the popover outlives the
  hover interaction (own it on `MainWindow` per open vault; clear via
  `setResources(nullptr)` on vault close, mirroring the existing
  `setEmbedRenderer(nullptr)` teardown).

## Data flow

```
leaf hover ─► LinkService::notifyHover ─► linkHovered(act, globalPos)
   └─(NoteEditorWidget)─► HoverPopover::scheduleShow(target, anchor)
        └─(300ms)─► renderTarget(target)
             ├─ VaultResourceProvider::resolveEmbed(path) ─► markdown bytes
             └─ StyledRenderEngine::render(bytes) ─► RenderedDocument
                  └─► QTextEdit::setDocument(doc)  ─► visible preview
```

## Testing

Re-enable and rewrite the two `if(FALSE)` tests against the new API (drop
`Markoff::Reading`; link `Corbomite::Core` for `StyledRenderEngine`):

- **`tst_hover_popover_render`** — inject `InMemoryResources` (already a
  `VaultResourceProvider`) + a real `StyledRenderEngine`:
  - resolvable target → `scheduleShow` → popover visible AND `m_display`
    document is non-empty (contains body text);
  - unresolved target → visible with the `(unresolved: …)` placeholder, no crash;
  - empty target → cancels (no show).
- **`tst_hover_popover_pinning`** — state machine is unchanged; the rewrite only
  swaps the link surface (no `EmbedRenderer`/`ReadingView`) and the CMake link
  libs. All existing pin/grace/replacement slots stay green.
- **New `tst_note_editor_widget_hover`** (end-to-end trigger) — drive the shared
  `LinkService` hover signal (emit `linkHovered` / call `notifyHover`) and assert
  `NoteEditorWidget` forwards it to the popover (`scheduleShow` → visible after
  the delay); `linkHoverLeft` → `linkHoverEnded`. Links `CorbomiteApp` +
  `markoff_live` + `markoff_styled` + `Corbomite::Core` (same pattern as
  `tst_link_activation`).

Offscreen baseline must stay green: `QT_QPA_PLATFORM=offscreen ctest` (current
**267/267**, excl. `benchmark`), plus the 3 re-enabled/new tests.

## Error handling / graceful degradation

- **Null engine or null resources:** placeholder text, never a crash.
- **Unresolved / missing target:** typed `(unresolved: …)` placeholder.
- **Unsupported blocks (tables/math/images/embeds):** inherited from
  `StyledRenderEngine`'s graceful degradation (source text / placeholder).
- **External links:** filtered at the `NoteEditorWidget` seam (no preview).

## Out of scope / deferred (tracked, not in this work)

- Subpath (`#heading` / `#^block`) slicing in the preview.
- Modifier-gated hover (Obsidian's edit-mode default).
- Nested-embed expansion in previews (was an `EmbedRenderer` feature; not in
  `StyledRenderEngine`'s headless path).
- Hover preview over Source (Qutepart) mode.
- Child-popover chains from links *inside* a pinned preview.
```
