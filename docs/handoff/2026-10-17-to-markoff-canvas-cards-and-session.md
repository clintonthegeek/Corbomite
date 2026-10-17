# Canvas cards, and the host session Corbomite actually opened

**Date:** 2026-10-17
**From:** Corbomite
**Markoff:** `dca64686` (readiness signal on sources `281e3bc1`)
**CollabText:** tag `collabtext-markoff-core-2026-10-13-alpha5`, commit `e8a8e7d1`, package `0.2.0-alpha.5`

This is a request, not a claim that canvas cards are collaborative. Stock
Obsidian editing is what we turned on. The session seam below is the part
we need you to leave alone.

## What Corbomite did

Notes no longer construct `MarkoffDocument(quint16{1})`. Each note asks a
process-wide `EditingSessionSource` for an `EditingBinding` and opens with
`MarkoffDocument::open`. The default source creates one ephemeral provider
per document, epoch 1, and uses a caller hint as the document key. Nothing
is written under the vault. Markdown files stay the Obsidian bytes:
`importInitialMarkdown` on first open, `replaceMarkdown` for a later
wholesale replacement, `snapshotForExport` plus `acknowledgeExport` on save.

`setEditingSessionSource` is the plugin hook. A collaboration plugin
replaces the source before notes or cards open and can hand back one shared
durable provider with its own scope per request. Corbomite does not call
`routeRemoteAction`, `pendingOutboundActions`, or the presence transport.
Those stay on `MarkoffDocument` for that plugin. Please do not add a
process-global replica or a single ambient document; both would make the
hook a lie.

`EditingRequest::keyHint` is a lookup string, not a UUID. Notes pass
`vaultRoot + '\n' + relativePath`. Canvas text cards will pass

```
canvas:<vault-relative .canvas path>#<node id>
```

A card is not a file. Its bytes live in the `.canvas` JSON. Please do not
require a document key to be a filesystem path.

## What a canvas card is today

`CanvasScene` paints a text card through `StyledRenderEngine`, which opens
a session document (`openForRender`) and calls
`DocumentRenderer::renderInto(QTextDocument*, const MarkoffDocument*)`.
A `ready` document is painted as-is and is not imported again. Editing the
card is still a `QTextEdit` proxy. On focus loss the plain text is stored
back into the node and the card is painted again. File cards pass the same
key hint as the note (`vaultRoot + '\n' + relativePath`) and still edit
through that `QTextEdit`, separately from the note's real editor. The byte
overload remains for one-shot callers and now treats
`committedHistoryPending` as success.

The pointer overload `renderInto(QTextDocument*, const MarkoffDocument*)`
is the one we will use once a card owns a document. Please keep it public,
widget-free, and cheap enough to call on each repaint of an already-open
document. The byte overload can stay for one-shot previews.

## What we need from you before cards become real documents

1. **An embeddable editor for a card-sized proxy.** We will put a
   `MarkdownView` in a `QGraphicsProxyWidget` over the card rectangle.
   `Canvas::EditorWidget` is the leaf Corbomite already uses for notes. If
   that widget assumes it is the central editor (minimum size, scroll
   ownership, accessibility tree for a whole window), tell us which leaf
   to embed instead, or add a constructor flag that makes it a bounded
   card: no toolbars, host-controlled size, read-only until the card is
   the one being edited. We will not fork a second editor.

2. **Many open scopes.** The ephemeral provider refuses a second session.
   A canvas has many cards, and a vault has many notes. Our default is one
   ephemeral provider per document, which matches that limit. Confirm that
   a durable `DocumentStorageProvider` may keep many scopes open at once
   (every open note, plus every card on the open canvases). If a durable
   provider is also one session, say so now. We will not share one
   ephemeral provider across cards.

3. **Paint without re-import.** Once a card holds a `MarkoffDocument`,
   repaint must use the pointer overload. Re-running
   `importInitialMarkdown` per paint would mint new block identities on
   every frame. No new API if the existing pointer overload stays.

4. **Optional, not blocking: an external adoption that is not an undo
   step.** `replaceMarkdown` keeps history, so undo after a clean reload
   restores the previous projection. We accepted that for notes and will
   do the same for a card whose `.canvas` file changed underneath us. If
   you would rather offer a publication that commits the bytes without an
   undo entry, add it as a separate call. Do not change `replaceMarkdown`
   to drop history; a plugin will want the action.

5. **Mid-block paste should splice, not trim.** `pasteMarkdownAt` trims
   the payload, so `" more"` becomes a new paragraph `"more"`. It also
   cuts the target at the caret and leaves the head as its own block:
   `"Hello"` plus `"X\n\nY"` at byte 3 becomes `"Hel\n\nX\n\nYlo"` rather
   than `"HelX\n\nYlo"`. Template insertion and a future card editor both
   need the flat splice (prefix + payload + suffix, then parse). We
   currently emulate it by replacing the whole block range with that
   splice, and we use `applyBlockEdit` for a payload that has no blank
   line. A paste that keeps leading spaces and joins the head onto the
   first parsed block would let us delete that emulation.

## What we are not asking

No network transport, no journal format, no presence drawing, no change to
the provider interface. Physical collection, pruning, and epoch transitions
stay off. The manual Orca pass stays yours until someone asks for it.
