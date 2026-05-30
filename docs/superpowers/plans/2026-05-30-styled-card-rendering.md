# Styled Card Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Feed Corbomite's canvas-card rendering from Markoff's headless `Markoff::Styled::DocumentRenderer` so file/text cards render real styled markdown instead of nothing.

**Architecture:** Add a `Corbomite::StyledRenderEngine` (a `MarkdownRenderEngine`) in `libs/core` that wraps `Markoff::Styled::DocumentRenderer::renderInto` to produce a `RenderedDocument` (T1). Wire it into the canvas via the existing `setRenderEngine` chain, which currently has no production caller. The `MarkdownRenderEngine`/`RenderedDocument` abstraction already exists — this fills the empty slot.

**Tech Stack:** C++20, Qt6 Widgets, CMake presets (`dev` → `build-dev/`), CTest.

**Prerequisite (handled separately):** submodule pinned to `8db7c5a` (contains `Markoff::Styled::DocumentRenderer`). Done in the convergence re-pin task; this plan assumes `libs/markoff-family/libs/markoff-styled/include/markoff/styled/DocumentRenderer.h` exists.

**Design:** [`docs/superpowers/specs/2026-05-29-styled-headless-rendering-convergence-design.md`](../specs/2026-05-29-styled-headless-rendering-convergence-design.md). **HoverPopover is NOT in this plan** — it needs the never-ported hover-trigger signal, a separate effort (see "Deferred").

---

## File Structure

| File | Responsibility | Action |
|---|---|---|
| `libs/core/include/corbomite/core/SubpathExtract.h` | Shared `extractMarkdownSubpath()` (factored out of `MarkoffRenderEngine.cpp`) | Create |
| `libs/core/include/corbomite/core/StyledRenderEngine.h` | `StyledRenderEngine : MarkdownRenderEngine` | Create |
| `libs/core/src/StyledRenderEngine.cpp` | Impl wrapping `Markoff::Styled::DocumentRenderer` | Create |
| `libs/core/src/MarkoffRenderEngine.cpp` | Use the shared subpath helper (de-dup) | Modify |
| `libs/core/CMakeLists.txt` | Add `StyledRenderEngine.cpp` to sources; link `markoff_styled` | Modify |
| `tests/core/tst_styled_render_engine.cpp` | Unit test for the engine | Create |
| `tests/core/CMakeLists.txt` | Register the test | Modify |
| `libs/canvas/src/CanvasScene.cpp` | Re-render existing cards on `setRenderEngine` | Modify |
| `libs/canvas/tests/tst_canvasscene.cpp` | Test re-render-on-set | Modify |
| `src/app/MainWindow.{h,cpp}` | Own a `StyledRenderEngine`; hand it to each `CanvasFileView` | Modify |

---

## Phase B — StyledRenderEngine in libs/core

### Task B.1: Factor the shared subpath helper

**Files:**
- Create: `libs/core/include/corbomite/core/SubpathExtract.h`
- Modify: `libs/core/src/MarkoffRenderEngine.cpp`

- [ ] **Step 1: Create the shared helper header**

`MarkoffRenderEngine.cpp` has a file-local `extractSubpath(markdown, subpath)`. Move its logic to a shared free function so `StyledRenderEngine` reuses it (DRY). Create `libs/core/include/corbomite/core/SubpathExtract.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>

namespace Corbomite {
/// Extract the section of `markdown` addressed by an Obsidian-style subpath
/// ("#heading" or "#^block-id"). Empty subpath → returns `markdown` unchanged.
QString extractMarkdownSubpath(const QString &markdown, const QString &subpath);
}
```

- [ ] **Step 2: Move the implementation**

In `libs/core/src/MarkoffRenderEngine.cpp`: remove the file-local `extractSubpath` definition, add `#include "corbomite/core/SubpathExtract.h"`, and put the moved body in a new `libs/core/src/SubpathExtract.cpp` as `Corbomite::extractMarkdownSubpath`. Update `MarkoffRenderEngine::render` to call `extractMarkdownSubpath(markdown, options.subpath)`. (If the original helper has a different name/signature, preserve its exact behavior — this is a pure move + rename, no logic change.)

- [ ] **Step 3: Add SubpathExtract.cpp to CMake**

In `libs/core/CMakeLists.txt`, add `src/SubpathExtract.cpp` to the `corbomite-core` sources list (alongside `src/MarkoffRenderEngine.cpp`).

- [ ] **Step 4: Build to confirm the move compiles**

Run: `cmake --build --preset dev -j 10 --target corbomite-core`
Expected: builds clean (pure refactor).

- [ ] **Step 5: Commit**
```bash
git add libs/core/include/corbomite/core/SubpathExtract.h libs/core/src/SubpathExtract.cpp libs/core/src/MarkoffRenderEngine.cpp libs/core/CMakeLists.txt
git commit -m "refactor(core): extract extractMarkdownSubpath into a shared helper

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task B.2: Failing test for StyledRenderEngine

**Files:**
- Create: `tests/core/tst_styled_render_engine.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/core/tst_styled_render_engine.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// StyledRenderEngine wraps Markoff::Styled::DocumentRenderer into the
// MarkdownRenderEngine/RenderedDocument abstraction. Offscreen.

#include "corbomite/core/StyledRenderEngine.h"
#include "corbomite/core/RenderedDocument.h"

#include <QTextDocument>
#include <QTest>

using Corbomite::StyledRenderEngine;

class StyledRenderEngineTest : public QObject {
    Q_OBJECT
private Q_SLOTS:

    void render_producesNonEmptyDocumentWithContent()
    {
        StyledRenderEngine engine;
        auto rendered = engine.render(
            QStringLiteral("# Title\n\nbody text here"));
        QVERIFY(rendered != nullptr);
        QTextDocument *doc = rendered->toQTextDocument();
        QVERIFY(doc != nullptr);
        // Styled keeps delimiters visible; plain text contains the content.
        QVERIFY(doc->toPlainText().contains(QStringLiteral("body text here")));
        QVERIFY(doc->toPlainText().contains(QStringLiteral("Title")));
    }

    void render_emptyMarkdown_isSafe()
    {
        StyledRenderEngine engine;
        auto rendered = engine.render(QString());
        QVERIFY(rendered != nullptr);
        QVERIFY(rendered->toQTextDocument() != nullptr);
    }

    void render_subpath_extractsSection()
    {
        StyledRenderEngine engine;
        Corbomite::RenderOptions opts;
        opts.subpath = QStringLiteral("#Second");
        auto rendered = engine.render(
            QStringLiteral("# First\n\nalpha\n\n# Second\n\nbravo"), opts);
        const QString text = rendered->toQTextDocument()->toPlainText();
        QVERIFY(text.contains(QStringLiteral("bravo")));
        QVERIFY(!text.contains(QStringLiteral("alpha")));
    }
};

QTEST_MAIN(StyledRenderEngineTest)
#include "tst_styled_render_engine.moc"
```

- [ ] **Step 2: Register the test**

Append to `tests/core/CMakeLists.txt` (mirror an existing offscreen test target that links `Corbomite::Core`):
```cmake
add_executable(tst_styled_render_engine tst_styled_render_engine.cpp)
target_link_libraries(tst_styled_render_engine PRIVATE
    Qt6::Test Qt6::Widgets Corbomite::Core markoff_styled)
add_test(NAME tst_styled_render_engine COMMAND tst_styled_render_engine)
set_tests_properties(tst_styled_render_engine PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Build to verify it FAILS**

Run: `cmake --preset dev && cmake --build --preset dev -j 10 --target tst_styled_render_engine`
Expected: FAILS to compile — `StyledRenderEngine.h` does not exist yet.

### Task B.3: Implement StyledRenderEngine

**Files:**
- Create: `libs/core/include/corbomite/core/StyledRenderEngine.h`
- Create: `libs/core/src/StyledRenderEngine.cpp`
- Modify: `libs/core/CMakeLists.txt`

- [ ] **Step 1: Header**

Create `libs/core/include/corbomite/core/StyledRenderEngine.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "corbomite/core/MarkdownRenderEngine.h"
#include <markoff/styled/DocumentRenderer.h>

namespace Markoff { class Theme; }

namespace Corbomite {

/// MarkdownRenderEngine backed by Markoff::Styled::DocumentRenderer (headless,
/// read-only). Renders markdown bytes into a RenderedDocument's QTextDocument.
class StyledRenderEngine : public MarkdownRenderEngine {
public:
    StyledRenderEngine();
    /// Optional theme (non-owning, may be null → renderer's default palette).
    void setTheme(const Markoff::Theme *theme);

    std::unique_ptr<RenderedDocument> render(
        const QString &markdown,
        const RenderOptions &options = {}) const override;

private:
    Markoff::Styled::DocumentRenderer m_renderer;
};

}  // namespace Corbomite
```

- [ ] **Step 2: Implementation**

Create `libs/core/src/StyledRenderEngine.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/StyledRenderEngine.h"

#include "corbomite/core/RenderedDocument.h"
#include "corbomite/core/RenderOptions.h"
#include "corbomite/core/SubpathExtract.h"

#include <QTextDocument>

namespace Corbomite {

StyledRenderEngine::StyledRenderEngine() = default;

void StyledRenderEngine::setTheme(const Markoff::Theme *theme)
{
    m_renderer.setTheme(theme);
}

std::unique_ptr<RenderedDocument> StyledRenderEngine::render(
    const QString &markdown, const RenderOptions &options) const
{
    const QString md = options.subpath.isEmpty()
        ? markdown
        : extractMarkdownSubpath(markdown, options.subpath);

    auto doc = std::make_unique<QTextDocument>();
    m_renderer.renderInto(doc.get(), md.toUtf8());
    return RenderedDocument::fromQTextDocument(std::move(doc));
}

}  // namespace Corbomite
```
Note: `DocumentRenderer::renderInto(QTextDocument*, const QByteArray&) const` is `const`, so calling it from `render() const` on the `m_renderer` member is valid. `setTheme` is non-const → `StyledRenderEngine::setTheme` is non-const (correct).

- [ ] **Step 3: CMake — add source + link markoff_styled**

In `libs/core/CMakeLists.txt`: add `src/StyledRenderEngine.cpp` to the `corbomite-core` sources. Add the styled lib to the Markoff `BUILD_INTERFACE` block (next to `$<BUILD_INTERFACE:markoff_core>`):
```cmake
        $<BUILD_INTERFACE:markoff_styled>
```

- [ ] **Step 4: Build + run to verify PASS**

Run:
```bash
cmake --preset dev && cmake --build --preset dev -j 10 --target tst_styled_render_engine
cd build-dev && ctest -R tst_styled_render_engine --output-on-failure
```
Expected: PASS (3/3). If the `#Second` subpath test fails, confirm `extractMarkdownSubpath`'s heading-match semantics; adjust the test's expectation to match the helper's actual contract (the helper's behavior is authoritative — do not change the helper).

- [ ] **Step 5: Commit**
```bash
git add libs/core/include/corbomite/core/StyledRenderEngine.h libs/core/src/StyledRenderEngine.cpp libs/core/CMakeLists.txt tests/core/tst_styled_render_engine.cpp tests/core/CMakeLists.txt
git commit -m "feat(core): StyledRenderEngine wraps Markoff::Styled::DocumentRenderer

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Phase C — Wire the engine into the canvas

### Task C.1: Re-render existing cards on setRenderEngine

**Files:**
- Modify: `libs/canvas/src/CanvasScene.cpp`
- Modify: `libs/canvas/tests/tst_canvasscene.cpp`

Currently `CanvasScene::setRenderEngine` only stores the pointer; if a document was already loaded (cards built) before the engine is set, those cards never render. Make it re-render.

- [ ] **Step 1: Write the failing test**

In `libs/canvas/tests/tst_canvasscene.cpp`, add a test that sets the document/cards FIRST, then sets the engine, and asserts a card has a rendered document. Mirror the existing `testFileCard` harness (construct `CanvasDocument` + `CanvasScene`, `setFileResolver`, `addNode`), but call `scene.setRenderEngine(&engine)` AFTER `doc.addNode(...)`, then assert the file card's rendered content is non-empty. Use `Corbomite::StyledRenderEngine` (or the existing `RegexRenderEngine` if simpler for the test) as the engine. Add the assertion against the card item's rendered document (extend `FileCardItem`/`TextCardItem` with a test accessor if none exists, or assert via the scene's public surface).

- [ ] **Step 2: Build + run → FAIL**

Run: `cmake --build --preset dev -j 10 --target tst_canvasscene && cd build-dev && ctest -R tst_canvasscene --output-on-failure`
Expected: the new case FAILS (engine set after cards exist → card not rendered).

- [ ] **Step 3: Implement re-render**

In `libs/canvas/src/CanvasScene.cpp`, change `setRenderEngine` to, after storing `m_renderEngine`, re-render every existing card if a document is loaded — iterate the scene's current card items and re-run the same render path used at creation (`m_renderEngine->render(node.text/markdown, opts)` → `card->setRenderedDocument(...)`). Factor the per-card render into a helper if it isn't already, and call it from both creation and here. Guard for `m_renderEngine == nullptr` (clear/no-op).

- [ ] **Step 4: Build + run → PASS**

Run: `cmake --build --preset dev -j 10 --target tst_canvasscene && cd build-dev && ctest -R tst_canvasscene --output-on-failure`
Expected: PASS (all cases, including the new one).

- [ ] **Step 5: Commit**
```bash
git add libs/canvas/src/CanvasScene.cpp libs/canvas/tests/tst_canvasscene.cpp
git commit -m "fix(canvas): re-render existing cards when the render engine is set

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

### Task C.2: Hand a StyledRenderEngine to every CanvasFileView

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Own the engine**

In `src/app/MainWindow.h`, add a member:
```cpp
    std::unique_ptr<Corbomite::StyledRenderEngine> m_cardRenderEngine;
```
(forward-declare `namespace Corbomite { class StyledRenderEngine; }` near the other fwd-decls; `std::unique_ptr` to an incomplete type is fine with an out-of-line destructor — MainWindow already has one.)

- [ ] **Step 2: Construct it**

In `src/app/MainWindow.cpp`, add `#include "corbomite/core/StyledRenderEngine.h"`, and in the MainWindow constructor (near where other long-lived services are built) initialize:
```cpp
    m_cardRenderEngine = std::make_unique<Corbomite::StyledRenderEngine>();
```

- [ ] **Step 3: Hand it to each canvas view**

In `src/app/MainWindow.cpp`, in the `qobject_cast<CanvasFileView *>(view)` branch (the block that calls `setCanvasCommandDispatcher`, ~line 1110-1117), add — before the `return;`:
```cpp
        cv->setRenderEngine(m_cardRenderEngine.get());
```

- [ ] **Step 4: Build the whole app + full suite**

Run:
```bash
cmake --build --preset dev -j 10
cd build-dev && ctest --output-on-failure -j 10
```
Expected: clean build; suite green at baseline (245 + new tests passing; the 5 pre-existing failures + headless-only `tst_e2e_gui` unchanged).

- [ ] **Step 5: Commit**
```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat(canvas): render cards via StyledRenderEngine

MainWindow owns a StyledRenderEngine and hands it to each CanvasFileView on
creation, so file/text cards render real styled markdown (the setRenderEngine
chain previously had no production caller).

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Deferred (separate follow-up)

- **HoverPopover** rendering: it depends on a hover-trigger signal from the Live editor that was never ported (the popover is doubly-dead — `m_view` hardcoded null, `scheduleShow` has no callers). Reviving it is a distinct effort (port the hover source + rebuild the render path on `DocumentRenderer::renderInto`), not a renderer swap. Track separately.
- **T2 paint/idealHeight** path: this plan uses T1 (`renderInto` → `QTextDocument`), which `FileCardItem::paint` already consumes via `drawContents`. The `paint`/`idealHeight` one-shots are available for a future per-frame/auto-fit optimization but aren't needed for correctness now.
- **Theme**: `StyledRenderEngine::setTheme` exists but isn't wired (ThemeService is a stub returning a default Theme). Wire when theming lands.

---

## Self-Review

- **Spec coverage:** canvas-card rendering via the styled headless renderer → Phases B+C. The empty `MarkdownRenderEngine` slot + missing `setRenderEngine` call (both audit findings) → Task C.2. `CanvasScene` re-render gap (audit) → Task C.1. HoverPopover → explicitly deferred with rationale. ✔
- **Placeholder scan:** Phase B carries complete code. Phase C.1's re-render and the canvas test reference `CanvasScene` internals the implementer must locate (card storage) — described by intent + the exact render call signature, not left as "TBD". The subpath helper move (B.1) is a behavior-preserving relocation; exact body lives in the source the implementer edits. ✔
- **Type consistency:** `StyledRenderEngine`, `render(const QString&, const RenderOptions&) -> std::unique_ptr<RenderedDocument>`, `RenderedDocument::fromQTextDocument`, `DocumentRenderer::renderInto/setTheme`, `extractMarkdownSubpath`, `m_cardRenderEngine`, target `markoff_styled` — used consistently and matched to the verified signatures in the render-seam reference + DocumentRenderer.h. ✔
- **Const-correctness checked:** `render()` is `const`; `m_renderer.renderInto(...) const` is callable; `setTheme` non-const on both. ✔
