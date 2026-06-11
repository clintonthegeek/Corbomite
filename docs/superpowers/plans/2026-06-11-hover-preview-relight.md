# Hover Preview Re-light Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make hovering a `[[wikilink]]` in the running app show a live styled preview, end-to-end, in Live and Reading modes.

**Architecture:** `HoverPopover`'s state machine is untouched. We (1) repoint its content path from the retired `Markoff::Reading` stack onto the already-shipped `StyledRenderEngine` + a `VaultResourceProvider` resolver, rendered into a `QTextBrowser`; (2) wire the existing shared `Markoff::LinkService::linkHovered`/`linkHoverLeft` signals (both Live and Reading leaves already emit them) to `scheduleShow`/`linkHoverEnded` in `NoteEditorWidget`; (3) feed the popover its engine + per-vault resources from `MainWindow`.

**Tech Stack:** C++20, Qt6 Widgets, Corbomite::Core (`StyledRenderEngine`, `RenderedDocument`, `VaultResourceProvider`), markoff-core `LinkService`.

**Spec:** [`docs/superpowers/specs/2026-06-11-hover-preview-relight-design.md`](../specs/2026-06-11-hover-preview-relight-design.md)

**Build/test reminders:**
- Configure (if needed): `cmake --preset dev`
- Build: `cmake --build --preset dev -j 10`
- Test: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j 10`
- Baseline to preserve: **267/267** (excl. label `benchmark`).
- Per the Corbomite repo rule, **never `git add -A`** — stage explicit paths.

---

## File Structure

- `src/editor/HoverPopover.h` — **modify**: swap retired API (`setEmbedRenderer`/`setVault`/`readingViewForTest`/`m_view`/`m_embedRenderer`) for `setRenderEngine`/`setResources`/`previewPlainText` + `QTextBrowser *m_display`.
- `src/editor/HoverPopover.cpp` — **modify**: build the `QTextBrowser`; implement `renderTarget` via resolver + engine; drop retired includes.
- `src/editor/NoteEditorWidget.cpp` — **modify**: two `connect`s from the shared `m_linkService` hover signals to the popover.
- `src/app/MainWindow.cpp` — **modify**: feed the popover its engine (once) + per-vault resources (on open/close).
- `tests/editor/tst_hover_popover_render.cpp` — **rewrite** against the new API.
- `tests/editor/tst_hover_popover_pinning.cpp` — **re-enable** (no source change expected; CMake only).
- `tests/editor/tst_note_editor_widget_hover.cpp` — **create**: end-to-end trigger test.
- `tests/editor/CMakeLists.txt` — **modify**: un-gate the two `if(FALSE)` tests, fix link libs, register the new test.

---

## Task 1: Re-light HoverPopover content path

**Files:**
- Modify: `src/editor/HoverPopover.h`
- Modify: `src/editor/HoverPopover.cpp`
- Rewrite: `tests/editor/tst_hover_popover_render.cpp`
- Modify: `tests/editor/CMakeLists.txt`

- [ ] **Step 1: Rewrite the render test against the new API**

Replace the entire contents of `tests/editor/tst_hover_popover_render.cpp` with:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// Hover preview re-light (2026-06-11) — HoverPopover renders a resolvable
// link target through StyledRenderEngine into its QTextBrowser. Resolution
// is supplied by a VaultResourceProvider; rendering by the headless engine.

#include <QApplication>
#include <QHash>
#include <QPoint>
#include <QString>
#include <QTest>

#include <optional>

#include "corbomite/core/StyledRenderEngine.h"
#include "corbomite/core/VaultResourceProvider.h"
#include "editor/HoverPopover.h"

using namespace Corbomite;

namespace {
class InMemoryResources : public Corbomite::Core::VaultResourceProvider
{
public:
    void addNote(const QString &name, const QString &content)
    {
        m_notes.insert(name, content);
    }
    QUrl resolveImage(const QString &name) const override
    {
        return QUrl(QStringLiteral("file:///fake/") + name);
    }
    QByteArray loadImageBytes(const QString &) const override { return {}; }
    std::optional<QString> resolveEmbed(const QString &name) const override
    {
        const auto it = m_notes.constFind(name);
        if (it == m_notes.constEnd()) return std::nullopt;
        return it.value();
    }
    QUrl resolveWikiLink(const QString &target) const override
    {
        return QUrl(QStringLiteral("vault:///") + target);
    }
    bool wikiLinkExists(const QString &target) const override
    {
        return m_notes.contains(target);
    }

private:
    QHash<QString, QString> m_notes;
};
} // namespace

class TstHoverPopoverRender : public QObject
{
    Q_OBJECT
private slots:
    void rendersResolvableTarget();
    void rendersPlaceholderForUnresolved();
    void emptyTargetDoesNotShow();
};

void TstHoverPopoverRender::rendersResolvableTarget()
{
    InMemoryResources resources;
    resources.addNote(QStringLiteral("Note.md"),
                      QStringLiteral("# Title\n\nSome body text.\n"));
    StyledRenderEngine engine;

    HoverPopover popover;
    popover.setRenderEngine(&engine);
    popover.setResources(&resources);

    popover.scheduleShow(QStringLiteral("Note.md"), QPoint(10, 10));
    QTRY_VERIFY_WITH_TIMEOUT(popover.isVisible(), 1000);

    const QString shown = popover.previewPlainText();
    QVERIFY2(shown.contains(QStringLiteral("Some body text")),
             qPrintable(shown));
}

void TstHoverPopoverRender::rendersPlaceholderForUnresolved()
{
    InMemoryResources resources; // empty — nothing resolves
    StyledRenderEngine engine;

    HoverPopover popover;
    popover.setRenderEngine(&engine);
    popover.setResources(&resources);

    popover.scheduleShow(QStringLiteral("Missing.md"), QPoint(10, 10));
    QTRY_VERIFY_WITH_TIMEOUT(popover.isVisible(), 1000);

    const QString shown = popover.previewPlainText();
    QVERIFY2(shown.contains(QStringLiteral("unresolved")), qPrintable(shown));
}

void TstHoverPopoverRender::emptyTargetDoesNotShow()
{
    StyledRenderEngine engine;
    HoverPopover popover;
    popover.setRenderEngine(&engine);
    popover.scheduleShow(QString(), QPoint(0, 0));
    QTest::qWait(350);
    QVERIFY(!popover.isVisible());
}

QTEST_MAIN(TstHoverPopoverRender)
#include "tst_hover_popover_render.moc"
```

- [ ] **Step 2: Update `HoverPopover.h` to the new API**

In `src/editor/HoverPopover.h`:

Replace the forward-decl block

```cpp
namespace Markoff::Reading {
class EmbedRenderer;
class ReadingView;
} // namespace Markoff::Reading

namespace Corbomite {

class Vault;
```

with

```cpp
class QTextBrowser;

namespace Corbomite {

class MarkdownRenderEngine;
namespace Core { class VaultResourceProvider; }
```

Replace the three setter declarations (`setVault`, `setEmbedRenderer`, and the `readingViewForTest` block) — i.e. remove `setVault`, `setEmbedRenderer`, and `readingViewForTest`, and add:

```cpp
    // Headless render engine (non-owning) used to turn resolved markdown
    // bytes into a styled QTextDocument. Caller retains ownership.
    void setRenderEngine(MarkdownRenderEngine *engine);

    // Per-vault resource provider (non-owning) used to resolve a hover
    // target (note name) to its markdown bytes via resolveEmbed(). Pass
    // nullptr on vault close.
    void setResources(Core::VaultResourceProvider *resources);

    // Test hook — current plain-text content of the preview widget.
    QString previewPlainText() const;
```

Replace the member block

```cpp
    Markoff::Reading::ReadingView *m_view = nullptr;
    Vault *m_vault = nullptr;
    Markoff::Reading::EmbedRenderer *m_embedRenderer = nullptr;
```

with

```cpp
    QTextBrowser *m_display = nullptr;
    MarkdownRenderEngine *m_renderEngine = nullptr;
    Core::VaultResourceProvider *m_resources = nullptr;
```

Keep `isPinned()`, `stateForTest()`, the timers, `m_pendingTarget`, `m_currentTarget`, `m_pendingAnchor`, `m_state`, `m_appFilterInstalled`, and all protected/private method decls unchanged (`renderTarget` stays).

- [ ] **Step 3: Update `HoverPopover.cpp`**

In `src/editor/HoverPopover.cpp`, replace the top include block

```cpp
#include <markoff/core/EmbedRegistry.h>
#include "corbomite/core/MarkdownRenderChild.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/vault/Vault.h"
// TODO(port): Reading::EmbedRenderer retired
// TODO(port): Reading::ReadingView retired
```

with

```cpp
#include "corbomite/core/MarkdownRenderEngine.h"
#include "corbomite/core/RenderedDocument.h"
#include "corbomite/core/VaultResourceProvider.h"

#include <QTextBrowser>
#include <QTextDocument>

#include <optional>
```

Replace the constructor body that nulls `m_view` — the block

```cpp
    // TODO(port-foundation-exploration): preview was a Markoff::Reading::
    // ReadingView; Reading retired. Hover preview is dark until the
    // markoff-reading-lite restoration OR Live-with-editing-disabled lands.
    m_view = nullptr;
    (void)layout;
```

with

```cpp
    m_display = new QTextBrowser(this);
    m_display->setOpenLinks(false);
    m_display->setOpenExternalLinks(false);
    m_display->setReadOnly(true);
    m_display->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_display);
```

Replace `setVault` and `setEmbedRenderer` definitions

```cpp
void HoverPopover::setVault(Vault *vault)
{
    m_vault = vault;
}

void HoverPopover::setEmbedRenderer(Markoff::Reading::EmbedRenderer *renderer)
{
    m_embedRenderer = renderer;
}

Markoff::Reading::ReadingView *HoverPopover::readingViewForTest() const
{
    return m_view;
}
```

with

```cpp
void HoverPopover::setRenderEngine(MarkdownRenderEngine *engine)
{
    m_renderEngine = engine;
}

void HoverPopover::setResources(Core::VaultResourceProvider *resources)
{
    m_resources = resources;
}

QString HoverPopover::previewPlainText() const
{
    return m_display ? m_display->toPlainText() : QString();
}
```

Replace the no-op `renderTarget`

```cpp
void HoverPopover::renderTarget(const QString &target)
{
    // TODO(port-foundation-exploration): renderTarget pumped resolved
    // markdown into a Markoff::Reading::ReadingView (m_view) — retired.
    // No-op until HoverPopover is rewired against Live-with-editing-disabled
    // or markoff-reading-lite is restored.
    (void)target;
}
```

with

```cpp
void HoverPopover::renderTarget(const QString &target)
{
    if (!m_display) return;

    QString path;
    QString subpath;
    splitTarget(target, &path, &subpath);

    const std::optional<QString> md =
        m_resources ? m_resources->resolveEmbed(path) : std::nullopt;
    // Subpath (#heading / #^block) slicing is deferred — render whole note.
    const QString markdown =
        md ? *md
           : QStringLiteral("*(unresolved: %1)*").arg(target);

    if (m_renderEngine) {
        const auto rendered = m_renderEngine->render(markdown);
        if (rendered && rendered->toQTextDocument()) {
            // Copy the styled content into the browser's own document
            // (ownership-safe; mirrors RenderedDocument::createWidget()).
            m_display->setHtml(rendered->toQTextDocument()->toHtml());
            return;
        }
    }
    // Defensive fallback: no engine wired — show raw markdown.
    m_display->setPlainText(markdown);
}
```

- [ ] **Step 4: Un-gate and re-wire the test targets in CMake**

In `tests/editor/CMakeLists.txt`, find the comment + `if(FALSE)` that precedes `add_executable(tst_hover_popover_render ...)` and the matching `endif()  # FALSE — HoverPopover tests disabled pending port` after the `tst_hover_popover_pinning` block. Delete those two guard lines (the `if(FALSE)` and its comment, and the `endif()`), leaving both `add_executable` blocks active.

Then change **both** test targets' `target_link_libraries` from

```cmake
    Qt6::Test
    Qt6::Widgets
    Corbomite::Core
    Corbomite::Models
    Markoff::Reading
```

to

```cmake
    Qt6::Test
    Qt6::Widgets
    Corbomite::Core
    markoff_styled
    markoff_core
```

(Both targets still compile `tst_*.cpp` + `${CMAKE_SOURCE_DIR}/src/editor/HoverPopover.cpp` and include `${CMAKE_SOURCE_DIR}/src` — leave those lines unchanged.)

- [ ] **Step 5: Configure + build**

Run: `cmake --preset dev && cmake --build --preset dev -j 10`
Expected: clean build (both hover test executables link).

- [ ] **Step 6: Run the two hover tests**

Run: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R "tst_hover_popover" --output-on-failure`
Expected: `tst_hover_popover_render` (3 slots) and `tst_hover_popover_pinning` (all existing slots) PASS.

- [ ] **Step 7: Commit**

```bash
git add src/editor/HoverPopover.h src/editor/HoverPopover.cpp \
        tests/editor/tst_hover_popover_render.cpp tests/editor/CMakeLists.txt
git commit -m "feat(hover): re-light HoverPopover content on StyledRenderEngine

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: Wire the hover trigger in NoteEditorWidget

**Files:**
- Create: `tests/editor/tst_note_editor_widget_hover.cpp`
- Modify: `src/editor/NoteEditorWidget.cpp:72-74` (after the existing `linkActivated` connect)
- Modify: `tests/editor/CMakeLists.txt`

- [ ] **Step 1: Write the failing end-to-end trigger test**

Create `tests/editor/tst_note_editor_widget_hover.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// Hover preview re-light (2026-06-11) — NoteEditorWidget forwards the shared
// LinkService hover signals to the HoverPopover. Both Live and Reading leaves
// share m_linkService, so driving notifyHover() here proves the wiring.

#include "NoteEditorWidget.h"
#include "HoverPopover.h"

#include <markoff/core/DefaultLinkService.h>
#include <markoff/core/LinkActivation.h>
#include <markoff/core/LinkKind.h>
#include <markoff/live/EditorWidget.h>
#include <markoff/live/LiveListModelBinding.h>

#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/StyledRenderEngine.h"
#include "corbomite/core/VaultResourceProvider.h"

#include <QHash>
#include <QPoint>
#include <QTest>

#include <optional>

using Corbomite::HoverPopover;
using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;
using Corbomite::StyledRenderEngine;

namespace {
class InMemoryResources : public Corbomite::Core::VaultResourceProvider
{
public:
    void addNote(const QString &name, const QString &content)
    {
        m_notes.insert(name, content);
    }
    QUrl resolveImage(const QString &name) const override
    {
        return QUrl(QStringLiteral("file:///fake/") + name);
    }
    QByteArray loadImageBytes(const QString &) const override { return {}; }
    std::optional<QString> resolveEmbed(const QString &name) const override
    {
        const auto it = m_notes.constFind(name);
        if (it == m_notes.constEnd()) return std::nullopt;
        return it.value();
    }
    QUrl resolveWikiLink(const QString &target) const override
    {
        return QUrl(QStringLiteral("vault:///") + target);
    }
    bool wikiLinkExists(const QString &target) const override
    {
        return m_notes.contains(target);
    }

private:
    QHash<QString, QString> m_notes;
};
} // namespace

class HoverTriggerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void linkHover_forwardsToPopover_andRenders()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[[Target]]"));
        widget.setNoteDocument(&doc);

        InMemoryResources resources;
        resources.addNote(QStringLiteral("Target"),
                          QStringLiteral("# T\n\nhovered body.\n"));
        StyledRenderEngine engine;

        HoverPopover popover;
        popover.setRenderEngine(&engine);
        popover.setResources(&resources);
        widget.setHoverPopover(&popover);

        auto *svc = widget.editor()->binding()->linkService();
        QVERIFY(svc);

        Markoff::LinkActivation act;
        act.kind    = Markoff::LinkKind::WikiLink;
        act.page    = QStringLiteral("Target");
        act.rawText = QStringLiteral("[[Target]]");

        svc->notifyHover(act, QPoint(50, 50));
        // scheduleShow() entered the Pending state synchronously.
        QCOMPARE(popover.stateForTest(), HoverPopover::State::Pending);

        // After the 300ms delay the popover shows the rendered target.
        QTRY_VERIFY_WITH_TIMEOUT(popover.isVisible(), 1000);
        QVERIFY2(popover.previewPlainText().contains(QStringLiteral("hovered body")),
                 qPrintable(popover.previewPlainText()));
    }

    void linkHoverLeft_whilePending_cancels()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[[Target]]"));
        widget.setNoteDocument(&doc);

        HoverPopover popover;
        widget.setHoverPopover(&popover);

        auto *svc = widget.editor()->binding()->linkService();
        Markoff::LinkActivation act;
        act.kind    = Markoff::LinkKind::WikiLink;
        act.page    = QStringLiteral("Target");
        act.rawText = QStringLiteral("[[Target]]");

        svc->notifyHover(act, QPoint(50, 50));
        QCOMPARE(popover.stateForTest(), HoverPopover::State::Pending);

        svc->notifyHoverLeft(QStringLiteral("[[Target]]"));
        QCOMPARE(popover.stateForTest(), HoverPopover::State::Hidden);
    }
};

QTEST_MAIN(HoverTriggerTest)
#include "tst_note_editor_widget_hover.moc"
```

- [ ] **Step 2: Register the new test in CMake**

Append to `tests/editor/CMakeLists.txt`:

```cmake
# Hover preview re-light (2026-06-11) — NoteEditorWidget forwards the shared
# LinkService hover signals to the HoverPopover (Live + Reading via one service).
add_executable(tst_note_editor_widget_hover tst_note_editor_widget_hover.cpp)
target_include_directories(tst_note_editor_widget_hover PRIVATE ${CMAKE_SOURCE_DIR}/src/editor)
target_link_libraries(tst_note_editor_widget_hover PRIVATE
    Qt6::Test Qt6::Widgets CorbomiteApp markoff_live markoff_styled Corbomite::Core)
add_test(NAME tst_note_editor_widget_hover COMMAND tst_note_editor_widget_hover)
set_tests_properties(tst_note_editor_widget_hover PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Build and confirm the test fails**

Run: `cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_note_editor_widget_hover --output-on-failure`
Expected: FAIL — popover stays `Hidden` (no forwarding wired yet); `stateForTest()` is `Hidden`, not `Pending`.

- [ ] **Step 4: Add the forwarding connections**

In `src/editor/NoteEditorWidget.cpp`, immediately after

```cpp
    connect(m_linkService, &Markoff::LinkService::linkActivated,
            this, &NoteEditorWidget::onLinkActivated);
```

add:

```cpp
    // Hover preview (2026-06-11) — forward the shared LinkService hover
    // stream to the host-owned popover. Both Live and Reading leaves emit
    // through this one service, so this covers both. m_hoverPopover is set
    // later by the host (setHoverPopover), so read it lazily at signal time.
    connect(m_linkService, &Markoff::LinkService::linkHovered, this,
            [this](const Markoff::LinkActivation &act, const QPoint &globalPos) {
                if (!m_hoverPopover) return;
                if (act.kind == Markoff::LinkKind::External) return;
                const QString target =
                    !act.page.isEmpty() ? act.page : act.rawText;
                if (target.isEmpty()) return;
                m_hoverPopover->scheduleShow(target, globalPos);
            });
    connect(m_linkService, &Markoff::LinkService::linkHoverLeft, this,
            [this](const QString &) {
                if (m_hoverPopover) m_hoverPopover->linkHoverEnded();
            });
```

- [ ] **Step 5: Build and confirm the test passes**

Run: `cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_note_editor_widget_hover --output-on-failure`
Expected: PASS (both slots).

- [ ] **Step 6: Commit**

```bash
git add src/editor/NoteEditorWidget.cpp \
        tests/editor/tst_note_editor_widget_hover.cpp tests/editor/CMakeLists.txt
git commit -m "feat(hover): forward LinkService hover signals to popover (Live + Reading)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: Feed the popover from MainWindow + full-suite verification

**Files:**
- Modify: `src/app/MainWindow.cpp` (construction ~line 363; `onVaultOpened` ~line 2222-2232; vault close ~line 2543; destructor ~line 429)

- [ ] **Step 1: Wire the render engine once, at popover construction**

In `src/app/MainWindow.cpp`, the constructor currently has (around line 363-376):

```cpp
    m_hoverPopover = new HoverPopover(this);
    // Vault binding deferred to onVaultOpened — no vault exists yet.

    m_embedRegistry = std::make_unique<Markoff::EmbedRegistry>();
    m_mermaidRenderer = std::make_unique<Corbomite::Core::MermaidRenderer>();
    m_cardRenderEngine = std::make_unique<Corbomite::StyledRenderEngine>();
```

Insert the `setRenderEngine` call right after `m_cardRenderEngine` is created (the engine is stateless and lives for the app lifetime), so this block becomes:

```cpp
    m_hoverPopover = new HoverPopover(this);
    // Vault binding deferred to onVaultOpened — no vault exists yet.

    m_embedRegistry = std::make_unique<Markoff::EmbedRegistry>();
    m_mermaidRenderer = std::make_unique<Corbomite::Core::MermaidRenderer>();
    m_cardRenderEngine = std::make_unique<Corbomite::StyledRenderEngine>();
    // Hover preview (2026-06-11) — reuse the canvas-card render engine; it is
    // stateless and read-only. Per-vault resources are set in onVaultOpened.
    m_hoverPopover->setRenderEngine(m_cardRenderEngine.get());
```

Then delete the three dead `EmbedRenderer` comment lines that follow (the `// TODO(port-foundation-exploration): Markoff::Reading::EmbedRenderer ...` block down to `// m_hoverPopover->setEmbedRenderer(m_embedRenderer.get());`).

- [ ] **Step 2: Set per-vault resources on vault open**

Around line 2222-2232, replace the `setVault` call. The current code is:

```cpp
    m_popoverResources = std::make_unique<VaultScopedResources>(m_vaultObj);
```
…(a few lines later)…
```cpp
    m_hoverPopover->setVault(m_vaultObj);
```

Keep the `m_popoverResources = std::make_unique<VaultScopedResources>(...)` line. Replace `m_hoverPopover->setVault(m_vaultObj);` with:

```cpp
    m_hoverPopover->setResources(m_popoverResources.get());
```

- [ ] **Step 3: Clear resources on vault close**

Around line 2543-2548, the close path has:

```cpp
    if (m_hoverPopover) m_hoverPopover->setVault(nullptr);
```
…
```cpp
    m_popoverResources.reset();
```

Replace `if (m_hoverPopover) m_hoverPopover->setVault(nullptr);` with:

```cpp
    if (m_hoverPopover) m_hoverPopover->setResources(nullptr);
```

Keep `m_popoverResources.reset();` where it is, but ensure it runs **after** `setResources(nullptr)` so the popover never holds a dangling resource pointer (it already follows in source order — verify the ordering when editing).

- [ ] **Step 4: Tidy the destructor comment**

Around line 429, delete the now-stale dead lines:

```cpp
    // TODO(port-foundation-exploration): EmbedRenderer disabled — see ctor.
    // if (m_embedRenderer) { m_embedRenderer->setMetadataCache(nullptr);
    //                       m_embedRenderer->setResources(nullptr); }
    // if (m_hoverPopover) m_hoverPopover->setEmbedRenderer(nullptr);
```

Leave `m_popoverResources.reset();` (the next line) intact.

- [ ] **Step 5: Build**

Run: `cmake --build --preset dev -j 10`
Expected: clean build. If the compiler flags any remaining `setVault`/`setEmbedRenderer` reference in `MainWindow.cpp`, remove that reference (those methods no longer exist).

- [ ] **Step 6: Run the full suite**

Run: `cd build-dev && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j 10 -E benchmark`
Expected: all green — the prior **267/267** plus `tst_hover_popover_render`, `tst_hover_popover_pinning`, and `tst_note_editor_widget_hover` (270 total, excl. benchmark). No regressions.

- [ ] **Step 7: Manual smoke (live eyeball — offscreen can't drive hover)**

Run: `./build-dev/bin/Corbomite`
Open a vault with a note containing a `[[wikilink]]` to another existing note. In **Live** mode, hover the link ~300 ms → a styled preview popover appears showing the target's content. Switch to **Reading** mode, hover a link → same. Move the cursor away → it dismisses after the grace period; press Ctrl while visible → it pins (accent border); Esc → dismiss. Note any visual issues for follow-up.

- [ ] **Step 8: Commit**

```bash
git add src/app/MainWindow.cpp
git commit -m "feat(hover): feed HoverPopover its engine + per-vault resources from MainWindow

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Closeout (after Task 3 passes + smoke confirmed)

- Update `docs/PARITY-MATRIX.md` line 79 ("Hover preview") from ⭕ to ✅ (or 🟡 pending the eyeball), noting StyledRenderEngine content + LinkService trigger, subpath slicing deferred.
- In `docs/PROJECT-STATE.md` §Current focus, replace the top entry with a ≤3-sentence hover-preview closeout; write the full paragraph into `docs/decisions-archive.md` under a new dated H2.
- Mark the **Hover preview** item done in `docs/superpowers/plans/2026-06-10-road-to-dogfood.md` Phase 2.
- Tracked follow-ups (out of scope here): subpath (`#heading`/`#^block`) slicing via `MarkdownRenderEngine::extractSubpath`; optional modifier-gated hover; nested-embed expansion in previews.

---

## Self-Review Notes

- **Spec coverage:** §1 trigger → Task 2; §2 content path (API swap, QTextBrowser, renderTarget, deferred subpath) → Task 1; §3 host wiring → Task 3; testing (3 tests) → Tasks 1+2; graceful degradation (null engine/resources, unresolved, external) → Task 1 renderTarget + Task 2 forwarding guard. All covered.
- **Type consistency:** `setRenderEngine(MarkdownRenderEngine*)`, `setResources(Core::VaultResourceProvider*)`, `previewPlainText()`, `RenderedDocument::toQTextDocument()`, `State::Pending/Hidden`, `LinkService::notifyHover/notifyHoverLeft`, `LinkActivation::{kind,page,rawText}`, `editor()->binding()->linkService()` — all match the real signatures verified in the tree.
- **Ownership:** preview content is copied into the `QTextBrowser` via `setHtml(toHtml())` (mirrors `RenderedDocument::createWidget`), so no `RenderedDocument` lifetime member is needed and there is no `QTextEdit`-ownership double-free hazard.
```
