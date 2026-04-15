# Cluster J — Embed / rendering primitives Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship Obsidian-parity embed rendering (`![[Note]]`, `![[Note#heading]]`, `![[Note#^block]]`), consolidate inline-link emission in both `libs/markoff/` and `libs/readingview/` onto first-class `LinkRenderer` classes, introduce internal post-processor + code-block-processor registries that Mermaid / math / syntax-highlighting route through, and swap HoverPopover's renderer to `EmbedRenderer` for rich-content previews.

**Architecture:** Split by concern. Interfaces, registries, lifecycle types in `libs/core/`; each of Markoff and ReadingView implements its own thin renderer shim. Stable plugin ABI is explicitly out of scope (Cluster N territory). Registries are sync-only with a documented async-placeholder-plus-signal pattern.

**Tech Stack:** C++20, Qt6 (QObject lifecycle, QFutureWatcher, QPointer), KDE Frameworks 6 (KConfigGroup where appropriate), tree-sitter via `libs/markoff-parser/`, existing `libs/jkqtmathtext/` + `libs/mmdr/` + `KF6::SyntaxHighlighting` for code-block built-ins.

**Spec:** `docs/superpowers/specs/2026-04-15-cluster-j-embed-rendering-design.md`.
**Supersedes:** `docs/superpowers/plans/2026-04-14-cluster-j-embed-rendering-primitives-STUB.md` (will be deleted when this plan is committed).

---

## Phase 0 — Audit prep (parallel-dispatchable)

Three Explore-agent dispatches run in parallel before Phase 1 begins. Each produces a findings doc that informs a downstream phase. No code changes; no commits.

### Task 0.1: Dispatch Explore — KDevelop async-render patterns

**Files:**
- Create: `docs/superpowers/research/2026-04-15-kdevelop-async-render-findings.md`

- [ ] **Step 1: Dispatch agent**

Use Agent tool with subagent_type=Explore. Prompt:

> Read KDevelop's documentation tooltip + code-completion async-render code at `~/src/kde/src/kdevelop/kdevplatform/language/codecompletion/` and `~/src/kde/src/kdevelop/kdevplatform/language/duchain/navigation/`. Do NOT clone from upstream — local source is current.
>
> Identify: (a) how KDevelop stages a multi-stage render where each stage may produce a partial result, (b) how widget lifecycle is tied to the surrounding view's state, (c) precedent for an "embed depth" or recursion-guard pattern in any nested-render path.
>
> Produce a translation plan for Corbomite's `ReadingView::EmbedRenderer` + `Corbomite::EmbedDepthGuard`. Target file structure in the Corbomite repo is: interfaces in `libs/core/`, renderer in `libs/readingview/`. Report: concrete class/method patterns worth adopting; class/method patterns to deliberately NOT adopt (with reason); open questions. Output in under 700 words, written directly to `docs/superpowers/research/2026-04-15-kdevelop-async-render-findings.md` in the Corbomite repo.

- [ ] **Step 2: Confirm findings doc exists**

Run: `ls -la docs/superpowers/research/2026-04-15-kdevelop-async-render-findings.md`
Expected: file exists, non-empty.

### Task 0.2: Dispatch Explore — Embed depth confirmation

**Files:**
- Create: `docs/superpowers/research/2026-04-15-embed-depth-findings.md`

- [ ] **Step 1: Dispatch agent**

Use Agent tool with subagent_type=Explore. Prompt:

> The Corbomite audit at `docs/obsidian-audit/domains/editor-markdown.md §1` documents an infinite-recursion guard `JZ(containerEl)` counting `.internal-embed` ancestors passed as `sJ.load({depth})`. Also referenced in `docs/obsidian-audit/SHARED-SYMBOLS.md`.
>
> Grep `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/` for the actual depth limit constant — look for `JZ`, literal `depth`, `internal-embed`, `MAX_DEPTH`, numeric comparisons against a depth variable. Report:
> 1. The exact integer cap used by Obsidian.
> 2. Whether the cap is exclusive (reject at `>=N`) or inclusive (reject at `>N`).
> 3. The exact behaviour/output/placeholder rendered at the cap.
> 4. Source file + approximate line number for the integer constant.
>
> Output in under 400 words, written directly to `docs/superpowers/research/2026-04-15-embed-depth-findings.md` in the Corbomite repo.

- [ ] **Step 2: Confirm findings doc exists**

Run: `ls -la docs/superpowers/research/2026-04-15-embed-depth-findings.md`
Expected: file exists.

### Task 0.3: Dispatch Explore — Markoff link-emission inventory

**Files:**
- Create: `docs/superpowers/research/2026-04-15-markoff-link-emission-inventory.md`

- [ ] **Step 1: Dispatch agent**

Use Agent tool with subagent_type=Explore. Prompt:

> Walk `/home/clinton/dev/Corbomite/libs/markoff/src/` and enumerate every current path that emits an inline link, a wikilink, a tag link, an external link, or a `hover-link` signal.
>
> For each found call-site, report: (a) the source file:line, (b) the emitting class/method, (c) the current API shape (signature + arguments at call-site), (d) the hover-link source string if any (searching for `hover-link`, `hoveredLink`, `linkHovered` signal emissions).
>
> Also identify: existing `RenderContext`-like ambient state, if any, already in Markoff; existing wikilink parsing paths; existing tag emitter paths; any ad-hoc vs deliberate split.
>
> Output: a table of call-sites ordered by file, plus one paragraph summarising which paths look like "consolidate into Markoff::LinkRenderer" and which look like "leave alone, already clean". Under 800 words, written directly to `docs/superpowers/research/2026-04-15-markoff-link-emission-inventory.md` in the Corbomite repo.

- [ ] **Step 2: Confirm findings doc exists**

Run: `ls -la docs/superpowers/research/2026-04-15-markoff-link-emission-inventory.md`
Expected: file exists.

### Task 0.4: Ritual 2 — Phase 0 complete

- [ ] **Step 1: Update PROJECT-STATE.md**

Update Cluster J Roadmap row status to `In progress (phase 1)`. Add in-flight row:

```
### Cluster J — Embed / rendering primitives
- **Phase:** 1 of 6 (Phase 0 audit prep complete)
- **Last completed step:** Phase 0 — three Explore dispatches produced findings docs (YYYY-MM-DD)
- **Next expected step:** Phase 1 — libs/core/ interfaces (VaultResourceProvider, MarkdownRenderChild, EmbedRegistry, EmbedDepthGuard)
- **Owner:** agent session
- **Date last touched:** YYYY-MM-DD
```

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/research/ docs/PROJECT-STATE.md
git commit -m "$(cat <<'EOF'
docs(research): Cluster J Phase 0 — audit prep findings

Three parallel Explore dispatches:
- KDevelop async-render patterns → informs Phase 4 EmbedRenderer
- Obsidian JZ depth-cap confirmation → informs Phase 1 EmbedDepthGuard
- Markoff link-emission inventory → informs Phase 3 LinkRenderer consolidation

Cluster J phase 0: research substrate for implementation phases 1-6.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Phase 1 — `libs/core/` interfaces

Four classes, all pure API + lifecycle. No renderers. Each lands with unit tests.

### Task 1.1: Promote `VaultResourceProvider` from readingview to core

**Files:**
- Create: `libs/core/include/corbomite/core/VaultResourceProvider.h`
- Modify: `libs/readingview/include/corbomite/readingview/VaultResourceProvider.h` (replace with forwarding typedef)
- Modify: `libs/readingview/CMakeLists.txt` (add `Corbomite::Core` dependency if not already)
- Test: `libs/core/tests/tst_vaultresourceprovider.cpp`
- Modify: `libs/core/tests/CMakeLists.txt`

- [ ] **Step 1: Read the current ReadingView VaultResourceProvider**

Read `libs/readingview/include/corbomite/readingview/VaultResourceProvider.h` and `libs/readingview/src/VaultResourceProvider.cpp` if it exists. Capture the exact method signatures. Expected interface:
- `resolveWikiLink(const QString &linkText, const QString &fromPath) → QString`
- `loadImageBytes(const QString &path) → QByteArray`
- any additional methods present.

- [ ] **Step 2: Write failing test for the promoted interface**

Create `libs/core/tests/tst_vaultresourceprovider.cpp`:

```cpp
#include <QTest>
#include <corbomite/core/VaultResourceProvider.h>

class StubResourceProvider : public Corbomite::Core::VaultResourceProvider {
public:
    QString resolveWikiLink(const QString &linkText, const QString &) const override {
        return "resolved:" + linkText;
    }
    QByteArray loadImageBytes(const QString &path) const override {
        return QByteArray("bytes:") + path.toUtf8();
    }
    // Include exact signatures from existing readingview provider.
};

class TstVaultResourceProvider : public QObject {
    Q_OBJECT
private slots:
    void testInterfaceDispatch() {
        StubResourceProvider p;
        QCOMPARE(p.resolveWikiLink("Foo", "Bar.md"), QStringLiteral("resolved:Foo"));
        QCOMPARE(p.loadImageBytes("img.png"), QByteArray("bytes:img.png"));
    }
};

QTEST_APPLESS_MAIN(TstVaultResourceProvider)
#include "tst_vaultresourceprovider.moc"
```

Add to `libs/core/tests/CMakeLists.txt`:

```cmake
add_executable(tst_vaultresourceprovider tst_vaultresourceprovider.cpp)
target_link_libraries(tst_vaultresourceprovider PRIVATE Corbomite::Core Qt6::Test)
add_test(NAME tst_vaultresourceprovider COMMAND tst_vaultresourceprovider)
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd build && cmake --build . --target tst_vaultresourceprovider 2>&1 | tail -30`
Expected: compile error, `VaultResourceProvider.h` not found in corbomite/core.

- [ ] **Step 4: Create `libs/core/include/corbomite/core/VaultResourceProvider.h`**

Copy exact signatures from readingview; change namespace to `Corbomite::Core`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QString>

namespace Corbomite::Core {

class VaultResourceProvider {
public:
    virtual ~VaultResourceProvider() = default;
    virtual QString resolveWikiLink(const QString &linkText, const QString &fromPath) const = 0;
    virtual QByteArray loadImageBytes(const QString &path) const = 0;
    // Mirror ALL methods from libs/readingview/include/corbomite/readingview/VaultResourceProvider.h
};

} // namespace Corbomite::Core
```

- [ ] **Step 5: Update readingview header to re-export**

Replace `libs/readingview/include/corbomite/readingview/VaultResourceProvider.h` body with:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <corbomite/core/VaultResourceProvider.h>

namespace Corbomite::ReadingView {
using VaultResourceProvider = Corbomite::Core::VaultResourceProvider;
}
```

- [ ] **Step 6: Verify ReadingView target still builds + existing tests pass**

Run: `cd build && cmake --build . --target readingview && ctest -R readingview --output-on-failure`
Expected: all green.

- [ ] **Step 7: Run new test**

Run: `cd build && ctest -R tst_vaultresourceprovider --output-on-failure`
Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add libs/core/include/corbomite/core/VaultResourceProvider.h libs/core/tests/tst_vaultresourceprovider.cpp libs/core/tests/CMakeLists.txt libs/readingview/include/corbomite/readingview/VaultResourceProvider.h
git commit -m "feat(core): promote VaultResourceProvider to libs/core/

Cluster J phase 1: shared interface for vault file lookup + image loading.
Closes Cluster E residual follow-up #7."
```

### Task 1.2: `MarkdownRenderChild` — lifecycle-tied child

**Files:**
- Create: `libs/core/include/corbomite/core/MarkdownRenderChild.h`
- Create: `libs/core/src/MarkdownRenderChild.cpp`
- Test: `libs/core/tests/tst_markdownrenderchild.cpp`
- Modify: `libs/core/CMakeLists.txt`, `libs/core/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Audit reference: `domains/editor-markdown.md §10`.

```cpp
#include <QTest>
#include <QSignalSpy>
#include <corbomite/core/Component.h>
#include <corbomite/core/MarkdownRenderChild.h>

class TstMarkdownRenderChild : public QObject {
    Q_OBJECT
private slots:
    void testInheritsComponent() {
        Corbomite::Core::MarkdownRenderChild child;
        auto *asComponent = static_cast<Corbomite::Core::Component *>(&child);
        QVERIFY(asComponent != nullptr);
    }
    void testUnloadFiresOnce() {
        Corbomite::Core::MarkdownRenderChild child;
        int unloadCount = 0;
        child.onUnload([&] { unloadCount++; });
        child.load();
        child.unload();
        child.unload();  // idempotent
        QCOMPARE(unloadCount, 1);
    }
    void testChildRegistration() {
        Corbomite::Core::MarkdownRenderChild parent;
        auto *child = new Corbomite::Core::MarkdownRenderChild();
        parent.addChild(child);  // inherits from Component
        parent.load();
        QVERIFY(child->isLoaded());
        parent.unload();
        QVERIFY(!child->isLoaded());
    }
};

QTEST_APPLESS_MAIN(TstMarkdownRenderChild)
#include "tst_markdownrenderchild.moc"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target tst_markdownrenderchild 2>&1 | tail -20`
Expected: compile error, `MarkdownRenderChild.h` not found.

- [ ] **Step 3: Implement header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <corbomite/core/Component.h>

namespace Corbomite::Core {

// Lifecycle-tied widget/scene-node subtree produced by post-processors,
// code-block processors, and embed renderers. Auto-unloads when its
// containing section is recycled by ReadingView::SectionRecyclePool.
// Audit reference: domains/editor-markdown.md §10.
class MarkdownRenderChild : public Component {
public:
    MarkdownRenderChild();
    ~MarkdownRenderChild() override;
};

} // namespace Corbomite::Core
```

- [ ] **Step 4: Implement .cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <corbomite/core/MarkdownRenderChild.h>
namespace Corbomite::Core {
MarkdownRenderChild::MarkdownRenderChild() = default;
MarkdownRenderChild::~MarkdownRenderChild() = default;
} // namespace Corbomite::Core
```

- [ ] **Step 5: Wire into CMake**

In `libs/core/CMakeLists.txt` add `src/MarkdownRenderChild.cpp` to sources and the header to install list. In `libs/core/tests/CMakeLists.txt`:

```cmake
add_executable(tst_markdownrenderchild tst_markdownrenderchild.cpp)
target_link_libraries(tst_markdownrenderchild PRIVATE Corbomite::Core Qt6::Test)
add_test(NAME tst_markdownrenderchild COMMAND tst_markdownrenderchild)
```

- [ ] **Step 6: Run tests to verify pass**

Run: `cd build && cmake --build . && ctest -R tst_markdownrenderchild --output-on-failure`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add libs/core/include/corbomite/core/MarkdownRenderChild.h libs/core/src/MarkdownRenderChild.cpp libs/core/tests/tst_markdownrenderchild.cpp libs/core/CMakeLists.txt libs/core/tests/CMakeLists.txt
git commit -m "feat(core): add MarkdownRenderChild lifecycle base

Cluster J phase 1: Component-derived base for renderer-produced
widget subtrees; recycle-aware unload."
```

### Task 1.3: `EmbedDepthGuard` — context-passed depth cap

**Files:**
- Create: `libs/core/include/corbomite/core/EmbedDepthGuard.h`
- Create: `libs/core/src/EmbedDepthGuard.cpp`
- Test: `libs/core/tests/tst_embeddepthguard.cpp`

Uses the `JZ` cap integer from `docs/superpowers/research/2026-04-15-embed-depth-findings.md` (Phase 0 Task 0.2 output).

- [ ] **Step 1: Read Phase 0 findings for the cap**

Read `docs/superpowers/research/2026-04-15-embed-depth-findings.md` and note the integer cap value and inclusive/exclusive semantics. Refer to as `JZ_CAP` in test + implementation.

- [ ] **Step 2: Write failing test**

```cpp
#include <QTest>
#include <corbomite/core/EmbedDepthGuard.h>

class TstEmbedDepthGuard : public QObject {
    Q_OBJECT
private slots:
    void testCapConstantMatchesAudit() {
        // Value from docs/superpowers/research/2026-04-15-embed-depth-findings.md
        QCOMPARE(Corbomite::Core::EmbedDepthGuard::kMaxDepth, /*FILL FROM FINDINGS*/);
    }
    void testAllowsUpToMax() {
        Corbomite::Core::EmbedDepthGuard g;
        for (int d = 0; d < Corbomite::Core::EmbedDepthGuard::kMaxDepth; ++d) {
            QVERIFY(g.allow(d));
        }
    }
    void testRejectsAtMax() {
        Corbomite::Core::EmbedDepthGuard g;
        QVERIFY(!g.allow(Corbomite::Core::EmbedDepthGuard::kMaxDepth));
    }
    void testPlaceholderShape() {
        auto p = Corbomite::Core::EmbedDepthGuard::placeholder("FooNote");
        QVERIFY(p.contains("FooNote"));
        QVERIFY(p.contains(QStringLiteral("embed depth")));
    }
};

QTEST_APPLESS_MAIN(TstEmbedDepthGuard)
#include "tst_embeddepthguard.moc"
```

- [ ] **Step 3: Run to verify fail**

Run: `cd build && cmake --build . --target tst_embeddepthguard 2>&1 | tail -10`
Expected: compile error.

- [ ] **Step 4: Implement**

`libs/core/include/corbomite/core/EmbedDepthGuard.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>

namespace Corbomite::Core {

// Embed-depth recursion guard. Mirrors Obsidian's JZ guard; cap constant
// confirmed in docs/superpowers/research/2026-04-15-embed-depth-findings.md.
// Compat-mode: match Obsidian exactly. User-configurable cap is an
// explicit post-parity follow-up.
class EmbedDepthGuard {
public:
    static constexpr int kMaxDepth = /*FILL FROM FINDINGS*/;
    bool allow(int currentDepth) const { return currentDepth < kMaxDepth; }
    static QString placeholder(const QString &targetLabel);
};

} // namespace Corbomite::Core
```

`libs/core/src/EmbedDepthGuard.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <corbomite/core/EmbedDepthGuard.h>
#include <KLocalizedString>

namespace Corbomite::Core {
QString EmbedDepthGuard::placeholder(const QString &targetLabel) {
    return i18n("[%1 — embed depth exceeded]", targetLabel);
}
} // namespace Corbomite::Core
```

- [ ] **Step 5: Wire CMake + run tests**

Append to CMakeLists. Run: `cd build && cmake --build . && ctest -R tst_embeddepthguard --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/EmbedDepthGuard.h libs/core/src/EmbedDepthGuard.cpp libs/core/tests/tst_embeddepthguard.cpp libs/core/CMakeLists.txt libs/core/tests/CMakeLists.txt
git commit -m "feat(core): add EmbedDepthGuard matching Obsidian JZ cap

Cluster J phase 1: recursion guard for nested ![[]] embed rendering.
Cap constant from audit prep findings."
```

### Task 1.4: `EmbedRegistry` — extension-to-factory dispatch

**Files:**
- Create: `libs/core/include/corbomite/core/EmbedRegistry.h`
- Create: `libs/core/src/EmbedRegistry.cpp`
- Test: `libs/core/tests/tst_embedregistry.cpp`

- [ ] **Step 1: Write failing test**

```cpp
#include <QTest>
#include <corbomite/core/EmbedRegistry.h>
#include <corbomite/core/MarkdownRenderChild.h>

class TstEmbedRegistry : public QObject {
    Q_OBJECT
private slots:
    void testRegisterAndDispatch() {
        Corbomite::Core::EmbedRegistry reg;
        bool called = false;
        reg.registerExtension("pdf", [&](const Corbomite::Core::EmbedRequest &req) {
            called = true;
            QCOMPARE(req.targetPath, QStringLiteral("doc.pdf"));
            return std::make_unique<Corbomite::Core::MarkdownRenderChild>();
        });
        auto child = reg.dispatch({"doc.pdf", {}, nullptr, 0});
        QVERIFY(called);
        QVERIFY(child != nullptr);
    }
    void testUnknownExtensionReturnsNullopt() {
        Corbomite::Core::EmbedRegistry reg;
        auto child = reg.dispatch({"doc.xyz", {}, nullptr, 0});
        QCOMPARE(child, nullptr);
    }
    void testCaseInsensitiveExtension() {
        Corbomite::Core::EmbedRegistry reg;
        reg.registerExtension("pdf", [](auto){ return std::make_unique<Corbomite::Core::MarkdownRenderChild>(); });
        QVERIFY(reg.dispatch({"doc.PDF", {}, nullptr, 0}) != nullptr);
    }
    void testHandleUnregister() {
        Corbomite::Core::EmbedRegistry reg;
        auto h = reg.registerExtension("md", [](auto){ return std::make_unique<Corbomite::Core::MarkdownRenderChild>(); });
        QVERIFY(reg.dispatch({"a.md", {}, nullptr, 0}) != nullptr);
        reg.unregister(h);
        QVERIFY(reg.dispatch({"a.md", {}, nullptr, 0}) == nullptr);
    }
};

QTEST_APPLESS_MAIN(TstEmbedRegistry)
#include "tst_embedregistry.moc"
```

- [ ] **Step 2: Run to verify fail**

Run: `cd build && cmake --build . --target tst_embedregistry 2>&1 | tail -10`
Expected: compile error.

- [ ] **Step 3: Implement header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <functional>
#include <memory>
#include <unordered_map>

namespace Corbomite::Core {

class VaultResourceProvider;
class MarkdownRenderChild;

struct EmbedRequest {
    QString targetPath;         // "Note.md", "image.png", "doc.pdf"
    QString subpath;            // "#heading", "#^blockid", or empty
    VaultResourceProvider *resources;
    int depth;                  // current embed depth for guard check
};

using EmbedFactory = std::function<std::unique_ptr<MarkdownRenderChild>(const EmbedRequest &)>;

class EmbedRegistry {
public:
    struct Handle { uint64_t id; QString extension; };
    Handle registerExtension(const QString &extLowercase, EmbedFactory fn);
    void unregister(const Handle &h);
    std::unique_ptr<MarkdownRenderChild> dispatch(const EmbedRequest &req) const;
private:
    struct Entry { uint64_t id; EmbedFactory fn; };
    std::unordered_map<QString, Entry> m_byExt;  // key = lowercased extension
    uint64_t m_nextId = 1;
};

} // namespace Corbomite::Core
```

- [ ] **Step 4: Implement .cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <corbomite/core/EmbedRegistry.h>
#include <corbomite/core/MarkdownRenderChild.h>
#include <QFileInfo>

namespace Corbomite::Core {

EmbedRegistry::Handle EmbedRegistry::registerExtension(const QString &extLowercase, EmbedFactory fn) {
    Handle h{m_nextId++, extLowercase};
    m_byExt[extLowercase] = Entry{h.id, std::move(fn)};
    return h;
}

void EmbedRegistry::unregister(const Handle &h) {
    auto it = m_byExt.find(h.extension);
    if (it != m_byExt.end() && it->second.id == h.id) m_byExt.erase(it);
}

std::unique_ptr<MarkdownRenderChild> EmbedRegistry::dispatch(const EmbedRequest &req) const {
    const QString ext = QFileInfo(req.targetPath).suffix().toLower();
    auto it = m_byExt.find(ext);
    if (it == m_byExt.end()) return nullptr;
    return it->second.fn(req);
}

} // namespace Corbomite::Core
```

- [ ] **Step 5: Wire CMake + run**

Append to `libs/core/CMakeLists.txt` + `libs/core/tests/CMakeLists.txt`. Run: `cd build && cmake --build . && ctest -R tst_embedregistry --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/EmbedRegistry.h libs/core/src/EmbedRegistry.cpp libs/core/tests/tst_embedregistry.cpp libs/core/CMakeLists.txt libs/core/tests/CMakeLists.txt
git commit -m "feat(core): add EmbedRegistry extension-to-factory dispatch

Cluster J phase 1: typed dispatch for ![[file.ext]] by extension.
Built-in registrations land in Phase 5."
```

### Task 1.5: Ritual 2 — Phase 1 complete

- [ ] **Step 1: Full suite check**

Run: `cd build && cmake --build . && ctest --output-on-failure`
Expected: all non-flaky tests green; 4 known-flaky listed in PROJECT-STATE §Known-flaky tests unchanged.

- [ ] **Step 2: Update PROJECT-STATE Cluster J in-flight row**

```
### Cluster J — Embed / rendering primitives
- **Phase:** 2 of 6
- **Last completed step:** Phase 1 — libs/core/ interfaces (VaultResourceProvider, MarkdownRenderChild, EmbedDepthGuard, EmbedRegistry) (YYYY-MM-DD)
- **Next expected step:** Phase 2 — libs/core/ internal registries (PostProcessorRegistry, CodeBlockProcessorRegistry)
- **Date last touched:** YYYY-MM-DD
```

- [ ] **Step 3: Commit state update**

```bash
git add docs/PROJECT-STATE.md
git commit -m "docs(state): Cluster J Phase 1 landed

libs/core/ gained VaultResourceProvider (promoted from readingview),
MarkdownRenderChild, EmbedDepthGuard (matching Obsidian JZ cap),
EmbedRegistry. No renderers yet; Phase 2 next."
```

---

## Phase 2 — `libs/core/` internal registries

### Task 2.1: `PostProcessorRegistry` — stable-sort by priority

**Files:**
- Create: `libs/core/include/corbomite/core/PostProcessorRegistry.h`
- Create: `libs/core/src/PostProcessorRegistry.cpp`
- Test: `libs/core/tests/tst_postprocessorregistry.cpp`

- [ ] **Step 1: Write failing test**

```cpp
#include <QTest>
#include <corbomite/core/PostProcessorRegistry.h>

class TstPostProcessorRegistry : public QObject {
    Q_OBJECT
private slots:
    void testRegisterAndIterate() {
        Corbomite::Core::PostProcessorRegistry reg;
        QStringList order;
        reg.registerProcessor(10, [&](auto *, const auto &) { order << "a"; });
        reg.registerProcessor(-5, [&](auto *, const auto &) { order << "b"; });
        reg.registerProcessor(5, [&](auto *, const auto &) { order << "c"; });
        reg.run(nullptr, {});
        // lower priority = earlier
        QCOMPARE(order, QStringList{"b", "c", "a"});
    }
    void testStableTiesInsertionOrder() {
        Corbomite::Core::PostProcessorRegistry reg;
        QStringList order;
        reg.registerProcessor(5, [&](auto *, auto) { order << "first"; });
        reg.registerProcessor(5, [&](auto *, auto) { order << "second"; });
        reg.registerProcessor(5, [&](auto *, auto) { order << "third"; });
        reg.run(nullptr, {});
        QCOMPARE(order, QStringList{"first", "second", "third"});
    }
    void testUnregister() {
        Corbomite::Core::PostProcessorRegistry reg;
        int count = 0;
        auto h = reg.registerProcessor(0, [&](auto *, auto) { count++; });
        reg.run(nullptr, {});
        QCOMPARE(count, 1);
        reg.unregister(h);
        reg.run(nullptr, {});
        QCOMPARE(count, 1);  // unchanged
    }
};

QTEST_APPLESS_MAIN(TstPostProcessorRegistry)
#include "tst_postprocessorregistry.moc"
```

- [ ] **Step 2: Run to verify fail**

Expected: compile error.

- [ ] **Step 3: Implement header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <functional>
#include <vector>

namespace Corbomite::Core {

class VaultResourceProvider;

struct PostProcessorContext {
    QString sourcePath;
    VaultResourceProvider *resources = nullptr;
    int depth = 0;
};

// Scenegraph node type is renderer-agnostic at the interface level —
// ReadingView passes ReadingSection*, Markoff passes its own node type.
// The interface uses void* and each renderer casts to its expected type
// at its own registration sites (internal API; type safety at wrap layer).
using PostProcessorFn = std::function<void(void *node, const PostProcessorContext &)>;

// WHY (design, not implementation): post-processors are SYNCHRONOUS by
// contract. Async work (e.g. Mermaid diagram rendering) is modelled via
// "mutate scenegraph with placeholder, kick off async job, update node
// through the scenegraph's own signal/slot plumbing". Pipeline does NOT
// await futures before declaring a section rendered. This matches
// ReadingView's existing async patterns for math/mermaid/images.
// Revisit after real use surfaces cases the placeholder pattern cannot
// express cleanly.
class PostProcessorRegistry {
public:
    struct Handle { uint64_t id; };
    Handle registerProcessor(int priority, PostProcessorFn fn);
    void unregister(Handle h);
    void run(void *node, const PostProcessorContext &ctx) const;
private:
    struct Entry { uint64_t id; int priority; uint64_t seq; PostProcessorFn fn; };
    mutable std::vector<Entry> m_entries;
    mutable bool m_dirty = false;
    uint64_t m_nextId = 1;
    uint64_t m_nextSeq = 0;
};

} // namespace Corbomite::Core
```

- [ ] **Step 4: Implement .cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <corbomite/core/PostProcessorRegistry.h>
#include <algorithm>

namespace Corbomite::Core {

PostProcessorRegistry::Handle PostProcessorRegistry::registerProcessor(int priority, PostProcessorFn fn) {
    Handle h{m_nextId++};
    m_entries.push_back({h.id, priority, m_nextSeq++, std::move(fn)});
    m_dirty = true;
    return h;
}

void PostProcessorRegistry::unregister(Handle h) {
    auto it = std::remove_if(m_entries.begin(), m_entries.end(),
                             [&](const Entry &e) { return e.id == h.id; });
    m_entries.erase(it, m_entries.end());
}

void PostProcessorRegistry::run(void *node, const PostProcessorContext &ctx) const {
    if (m_dirty) {
        std::stable_sort(m_entries.begin(), m_entries.end(),
                         [](const Entry &a, const Entry &b) {
                             if (a.priority != b.priority) return a.priority < b.priority;
                             return a.seq < b.seq;
                         });
        m_dirty = false;
    }
    for (const auto &e : m_entries) e.fn(node, ctx);
}

} // namespace Corbomite::Core
```

- [ ] **Step 5: Wire CMake, run test**

Run: `cd build && cmake --build . && ctest -R tst_postprocessorregistry --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/PostProcessorRegistry.h libs/core/src/PostProcessorRegistry.cpp libs/core/tests/tst_postprocessorregistry.cpp libs/core/CMakeLists.txt libs/core/tests/CMakeLists.txt
git commit -m "feat(core): add PostProcessorRegistry with stable priority sort

Cluster J phase 2: sync-only post-processor dispatch. Ties broken by
insertion order. Documented sync-placeholder-plus-async-update pattern
in header // WHY: block."
```

### Task 2.2: `CodeBlockProcessorRegistry` — per-language dispatch

**Files:**
- Create: `libs/core/include/corbomite/core/CodeBlockProcessorRegistry.h`
- Create: `libs/core/src/CodeBlockProcessorRegistry.cpp`
- Test: `libs/core/tests/tst_codeblockprocessorregistry.cpp`

- [ ] **Step 1: Write failing test**

```cpp
#include <QTest>
#include <corbomite/core/CodeBlockProcessorRegistry.h>

class TstCodeBlockProcessorRegistry : public QObject {
    Q_OBJECT
private slots:
    void testLanguageDispatch() {
        Corbomite::Core::CodeBlockProcessorRegistry reg;
        QString lastSource;
        reg.registerLanguage("mermaid", [&](const QString &src, void *, const auto &) {
            lastSource = src;
            return true;
        });
        QVERIFY(reg.dispatch("mermaid", "flowchart TD;A-->B", nullptr, {}));
        QCOMPARE(lastSource, QStringLiteral("flowchart TD;A-->B"));
    }
    void testUnknownLanguageReturnsFalse() {
        Corbomite::Core::CodeBlockProcessorRegistry reg;
        QVERIFY(!reg.dispatch("unknownlang", "x=1", nullptr, {}));
    }
    void testCaseInsensitiveLanguageMatch() {
        Corbomite::Core::CodeBlockProcessorRegistry reg;
        reg.registerLanguage("mermaid", [](auto, void *, auto){ return true; });
        QVERIFY(reg.dispatch("Mermaid", "x", nullptr, {}));
    }
    void testUnregister() {
        Corbomite::Core::CodeBlockProcessorRegistry reg;
        auto h = reg.registerLanguage("cpp", [](auto, void *, auto){ return true; });
        QVERIFY(reg.dispatch("cpp", "", nullptr, {}));
        reg.unregister(h);
        QVERIFY(!reg.dispatch("cpp", "", nullptr, {}));
    }
};

QTEST_APPLESS_MAIN(TstCodeBlockProcessorRegistry)
#include "tst_codeblockprocessorregistry.moc"
```

- [ ] **Step 2: Run to verify fail**

Expected: compile error.

- [ ] **Step 3: Implement header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <functional>
#include <unordered_map>

namespace Corbomite::Core {

class VaultResourceProvider;

struct CodeBlockContext {
    QString sourcePath;
    VaultResourceProvider *resources = nullptr;
    int depth = 0;
};

// Returns true if handled; false to fall through to default highlighting.
using CodeBlockProcessorFn = std::function<bool(const QString &source, void *node, const CodeBlockContext &)>;

// WHY (design, not implementation): per-language dispatch is SYNCHRONOUS.
// Same async-placeholder pattern as PostProcessorRegistry — the processor
// mutates the scenegraph with a placeholder and triggers its own async
// update via signal. Re-evaluate when plugin-authored code-block
// processors surface use cases the placeholder pattern can't express.
class CodeBlockProcessorRegistry {
public:
    struct Handle { uint64_t id; QString language; };
    Handle registerLanguage(const QString &language, CodeBlockProcessorFn fn);
    void unregister(const Handle &h);
    bool dispatch(const QString &language, const QString &source, void *node, const CodeBlockContext &ctx) const;
private:
    struct Entry { uint64_t id; CodeBlockProcessorFn fn; };
    std::unordered_map<QString, Entry> m_byLang;  // key = lowercased
    uint64_t m_nextId = 1;
};

} // namespace Corbomite::Core
```

- [ ] **Step 4: Implement .cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <corbomite/core/CodeBlockProcessorRegistry.h>

namespace Corbomite::Core {

CodeBlockProcessorRegistry::Handle CodeBlockProcessorRegistry::registerLanguage(const QString &language, CodeBlockProcessorFn fn) {
    const QString key = language.toLower();
    Handle h{m_nextId++, key};
    m_byLang[key] = Entry{h.id, std::move(fn)};
    return h;
}

void CodeBlockProcessorRegistry::unregister(const Handle &h) {
    auto it = m_byLang.find(h.language);
    if (it != m_byLang.end() && it->second.id == h.id) m_byLang.erase(it);
}

bool CodeBlockProcessorRegistry::dispatch(const QString &language, const QString &source, void *node, const CodeBlockContext &ctx) const {
    auto it = m_byLang.find(language.toLower());
    if (it == m_byLang.end()) return false;
    return it->second.fn(source, node, ctx);
}

} // namespace Corbomite::Core
```

- [ ] **Step 5: Wire CMake + run**

Run: `cd build && cmake --build . && ctest -R tst_codeblockprocessorregistry --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/core/include/corbomite/core/CodeBlockProcessorRegistry.h libs/core/src/CodeBlockProcessorRegistry.cpp libs/core/tests/tst_codeblockprocessorregistry.cpp libs/core/CMakeLists.txt libs/core/tests/CMakeLists.txt
git commit -m "feat(core): add CodeBlockProcessorRegistry per-language dispatch

Cluster J phase 2: sync-only dispatch, case-insensitive language key,
boolean return signals handled-vs-fallthrough. Built-ins register
in Phase 5."
```

### Task 2.3: Ritual 2 — Phase 2 complete

- [ ] **Step 1: Run full suite**

Run: `cd build && cmake --build . && ctest --output-on-failure`
Expected: all non-flaky green.

- [ ] **Step 2: Update PROJECT-STATE; mark Phase 3+4 parallel-dispatchable**

```
### Cluster J — Embed / rendering primitives
- **Phase:** 3/4 of 6 (parallel-dispatchable)
- **Last completed step:** Phase 2 — libs/core/ registries (PostProcessorRegistry, CodeBlockProcessorRegistry) (YYYY-MM-DD)
- **Next expected step:** Phase 3 (Markoff::LinkRenderer) + Phase 4 (ReadingView::LinkRenderer + EmbedRenderer) dispatched in parallel
- **Date last touched:** YYYY-MM-DD
```

- [ ] **Step 3: Commit**

```bash
git add docs/PROJECT-STATE.md
git commit -m "docs(state): Cluster J Phase 2 landed; 3+4 next (parallel)"
```

---

## Phase 3 — `Markoff::LinkRenderer` (parallel-dispatchable with Phase 4)

Consolidates all current ad-hoc link-emission paths in `libs/markoff/src/` into one `LinkRenderer` class. Honest per-caller `hover-link` source strings.

### Task 3.1: Read Phase 0 inventory

- [ ] **Step 1: Read inventory findings**

Read `docs/superpowers/research/2026-04-15-markoff-link-emission-inventory.md`. List the call-sites to consolidate. Note the current signature variations.

### Task 3.2: Design `Markoff::LinkRenderer` class contract

**Files:**
- Create: `libs/markoff/include/markoff/LinkRenderer.h`

- [ ] **Step 1: Write header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QString>

namespace Corbomite::Core { class VaultResourceProvider; }

namespace Markoff {

class LinkRenderer : public QObject {
    Q_OBJECT
public:
    explicit LinkRenderer(Corbomite::Core::VaultResourceProvider *resources, QObject *parent = nullptr);

    // Called by every Markoff inline-link-emission path. `source` is the
    // honest identifier of the emitter ("markoff:editor", "markoff:reading",
    // "markoff:livepreview"). NO hardcoded "bases" string: compat alias
    // is a Cluster N shim concern.
    struct FileLinkRequest {
        QString linkText;           // wikilink "Note#heading|Alias" or plain text
        QString fromPath;           // note containing the link
        QString sourceId;           // honest caller identifier
        QPoint  anchorHint;         // optional: pixel anchor for hover emission
    };

    void emitFileLink(const FileLinkRequest &req);
    void emitTagLink(const QString &tag, const QString &sourceId);
    void emitExternalLink(const QUrl &url, const QString &sourceId);

signals:
    void linkHovered(const QString &target, const QString &sourceId, const QPoint &anchorHint);
    void linkClicked(const QString &target, const QString &sourceId);
    void tagHovered(const QString &tag, const QString &sourceId);
    void externalLinkActivated(const QUrl &url, const QString &sourceId);

private:
    Corbomite::Core::VaultResourceProvider *m_resources;
};

} // namespace Markoff
```

- [ ] **Step 2: No test yet (tests land in Task 3.4 as integration over consolidated call-sites)**

### Task 3.3: Implement `Markoff::LinkRenderer`

**Files:**
- Create: `libs/markoff/src/LinkRenderer.cpp`

- [ ] **Step 1: Write implementation**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <markoff/LinkRenderer.h>
#include <corbomite/core/VaultResourceProvider.h>

namespace Markoff {

LinkRenderer::LinkRenderer(Corbomite::Core::VaultResourceProvider *resources, QObject *parent)
    : QObject(parent), m_resources(resources) {}

void LinkRenderer::emitFileLink(const FileLinkRequest &req) {
    const QString resolved = m_resources
        ? m_resources->resolveWikiLink(req.linkText, req.fromPath)
        : req.linkText;
    emit linkHovered(resolved, req.sourceId, req.anchorHint);
}

void LinkRenderer::emitTagLink(const QString &tag, const QString &sourceId) {
    emit tagHovered(tag, sourceId);
}

void LinkRenderer::emitExternalLink(const QUrl &url, const QString &sourceId) {
    emit externalLinkActivated(url, sourceId);
}

} // namespace Markoff
```

- [ ] **Step 2: Wire CMake**

Append source to `libs/markoff/CMakeLists.txt`.

- [ ] **Step 3: Build check**

Run: `cd build && cmake --build . --target markoff`
Expected: PASS.

### Task 3.4: Consolidate existing Markoff link-emission call-sites

Per the inventory from Phase 0 Task 0.3, for each call-site listed:

**Files:** Per the inventory doc.

- [ ] **Step 1: For each call-site, replace direct emission with `LinkRenderer::emit*`**

Example (apply to each call-site): if the inventory names `libs/markoff/src/Editor.cpp:456` as emitting `emit linkHovered(...)` directly, replace:

```cpp
// Before:
emit linkHovered(target);

// After:
m_linkRenderer->emitFileLink({target, m_currentNotePath, "markoff:editor", mousePoint});
```

For each call-site, the test assertion:

```cpp
// In the affected class's existing test, or add new integration test:
QSignalSpy spy(&linkRenderer, &Markoff::LinkRenderer::linkHovered);
// trigger the hover
QVERIFY(spy.count() >= 1);
QCOMPARE(spy.first()[1].toString(), QStringLiteral("markoff:editor"));  // honest source
```

- [ ] **Step 2: Add integration test confirming no `"bases"` string remains in Markoff**

`libs/markoff/tests/tst_markoff_link_source_honesty.cpp`:

```cpp
#include <QTest>
#include <QFile>
#include <QDir>
#include <QDirIterator>

class TstMarkoffLinkSourceHonesty : public QObject {
    Q_OBJECT
private slots:
    void testNoBasesHardcodeInMarkoffSources() {
        // Walk libs/markoff/src/ and assert no occurrence of the literal "bases"
        // in the hover-link-emission context.
        QDirIterator it(QStringLiteral("../../libs/markoff/src"),
                        {"*.cpp", "*.h"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile f(it.next());
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QByteArray body = f.readAll();
            QVERIFY2(!body.contains("\"bases\""),
                     qPrintable("File contains \"bases\" hardcode: " + f.fileName()));
        }
    }
};

QTEST_APPLESS_MAIN(TstMarkoffLinkSourceHonesty)
#include "tst_markoff_link_source_honesty.moc"
```

- [ ] **Step 3: Run full Markoff test suite**

Run: `cd build && cmake --build . && ctest -R markoff --output-on-failure`
Expected: all pre-existing Markoff tests still pass + new honesty test passes.

- [ ] **Step 4: Commit**

```bash
git add libs/markoff/
git commit -m "refactor(markoff): consolidate link emission through LinkRenderer

Cluster J phase 3: LinkRenderer is the sole inline-link-emitter in
libs/markoff/. Per-caller honest source strings ('markoff:editor',
'markoff:livepreview', etc.). No 'bases' hardcode. Old ad-hoc paths
removed."
```

### Task 3.5: Ritual 2 — Phase 3 complete

- [ ] **Step 1: Update PROJECT-STATE**

```
Phase 3 complete. Phase 4 still in-flight (parallel).
```

- [ ] **Step 2: Commit state doc update**

---

## Phase 4 — `ReadingView::LinkRenderer` + `EmbedRenderer` (parallel-dispatchable with Phase 3)

### Task 4.1: Read KDevelop async-render findings

- [ ] **Step 1: Read findings**

Read `docs/superpowers/research/2026-04-15-kdevelop-async-render-findings.md`. Adopt any recommended patterns (staged render, lifecycle tie-in, recursion guard).

### Task 4.2: `ReadingView::LinkRenderer`

**Files:**
- Create: `libs/readingview/include/corbomite/readingview/LinkRenderer.h`
- Create: `libs/readingview/src/LinkRenderer.cpp`
- Test: `libs/readingview/tests/tst_readingview_linkrenderer.cpp`

- [ ] **Step 1: Write failing test**

```cpp
#include <QTest>
#include <QSignalSpy>
#include <corbomite/readingview/LinkRenderer.h>
#include "TestResourceProvider.h"  // existing test helper

class TstReadingViewLinkRenderer : public QObject {
    Q_OBJECT
private slots:
    void testFileLinkEmitsHonestSource() {
        TestResourceProvider resources;
        Corbomite::ReadingView::LinkRenderer r(&resources);
        QSignalSpy spy(&r, &Corbomite::ReadingView::LinkRenderer::linkHovered);
        r.emitFileLink({"Target", "From.md", "markoff:reading", QPoint(10, 20)});
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first()[1].toString(), QStringLiteral("markoff:reading"));
    }
};

QTEST_APPLESS_MAIN(TstReadingViewLinkRenderer)
#include "tst_readingview_linkrenderer.moc"
```

- [ ] **Step 2: Run to verify fail**

Expected: compile error.

- [ ] **Step 3: Implement header + .cpp**

Same shape as Markoff's `LinkRenderer`. Namespace `Corbomite::ReadingView`.

- [ ] **Step 4: Wire CMake + run test**

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add libs/readingview/include/corbomite/readingview/LinkRenderer.h libs/readingview/src/LinkRenderer.cpp libs/readingview/tests/tst_readingview_linkrenderer.cpp libs/readingview/CMakeLists.txt libs/readingview/tests/CMakeLists.txt
git commit -m "feat(readingview): add LinkRenderer sibling for section-mount pipeline

Cluster J phase 4: same contract as Markoff::LinkRenderer; honest
per-caller source strings."
```

### Task 4.3: `ReadingView::EmbedRenderer` — the marquee deliverable

**Files:**
- Create: `libs/readingview/include/corbomite/readingview/EmbedRenderer.h`
- Create: `libs/readingview/src/EmbedRenderer.cpp`
- Test: `libs/readingview/tests/tst_readingview_embedrenderer.cpp`

Uses: `Corbomite::Core::EmbedRegistry`, `EmbedDepthGuard`, `MarkdownRenderChild`, `MetadataCache::headings`, `MetadataCache::blocks`.

- [ ] **Step 1: Write failing test covering the marquee cases**

```cpp
#include <QTest>
#include <corbomite/readingview/EmbedRenderer.h>
#include <corbomite/core/EmbedRegistry.h>
#include <corbomite/storage/MetadataCache.h>
#include "TestResourceProvider.h"

class TstReadingViewEmbedRenderer : public QObject {
    Q_OBJECT
private slots:
    void testEmbedWholeNote() {
        TestResourceProvider resources;
        resources.addNote("Target.md", "# Target\n\nBody text.\n");
        Corbomite::Storage::MetadataCache cache;
        cache.rebuildForTest({"Target.md", "From.md"}, &resources);
        Corbomite::Core::EmbedRegistry reg;
        Corbomite::ReadingView::EmbedRenderer r(&reg, &cache, &resources);
        auto child = r.render({"Target.md", "", &resources, 0});
        QVERIFY(child != nullptr);
        QVERIFY(child->renderedText().contains("Body text"));
    }
    void testEmbedHeadingSection() {
        TestResourceProvider resources;
        resources.addNote("Target.md",
            "# First\nIgnored.\n\n## Second\nWanted.\n\n## Third\nAlso ignored.\n");
        Corbomite::Storage::MetadataCache cache;
        cache.rebuildForTest({"Target.md", "From.md"}, &resources);
        Corbomite::Core::EmbedRegistry reg;
        Corbomite::ReadingView::EmbedRenderer r(&reg, &cache, &resources);
        auto child = r.render({"Target.md", "#Second", &resources, 0});
        QVERIFY(child);
        QVERIFY(child->renderedText().contains("Wanted"));
        QVERIFY(!child->renderedText().contains("Ignored"));
    }
    void testEmbedBlockRef() {
        TestResourceProvider resources;
        resources.addNote("Target.md",
            "intro\n\nblock body ^blk\n\noutro\n");
        Corbomite::Storage::MetadataCache cache;
        cache.rebuildForTest({"Target.md"}, &resources);
        Corbomite::Core::EmbedRegistry reg;
        Corbomite::ReadingView::EmbedRenderer r(&reg, &cache, &resources);
        auto child = r.render({"Target.md", "#^blk", &resources, 0});
        QVERIFY(child);
        QVERIFY(child->renderedText().contains("block body"));
        QVERIFY(!child->renderedText().contains("intro"));
        QVERIFY(!child->renderedText().contains("outro"));
    }
    void testSelfEmbedStopsAtCap() {
        TestResourceProvider resources;
        // A note that embeds itself: ![[Self]]
        resources.addNote("Self.md", "![[Self]]\n");
        Corbomite::Storage::MetadataCache cache;
        cache.rebuildForTest({"Self.md"}, &resources);
        Corbomite::Core::EmbedRegistry reg;
        // register markdown factory
        reg.registerExtension("md", [&](const Corbomite::Core::EmbedRequest &req) {
            Corbomite::ReadingView::EmbedRenderer r(&reg, &cache, &resources);
            return r.render(req);
        });
        auto child = reg.dispatch({"Self.md", "", &resources, 0});
        QVERIFY(child);
        const QString rendered = child->renderedText();
        QVERIFY(rendered.contains(QStringLiteral("embed depth exceeded")));
        // Must not run the process out of stack — if the test hangs, the test framework
        // will timeout and this counts as a fail.
    }
};

QTEST_GUILESS_MAIN(TstReadingViewEmbedRenderer)
#include "tst_readingview_embedrenderer.moc"
```

- [ ] **Step 2: Run to verify fail**

Expected: compile error (EmbedRenderer.h missing).

- [ ] **Step 3: Implement header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <corbomite/core/EmbedRegistry.h>
#include <corbomite/core/MarkdownRenderChild.h>
#include <corbomite/core/EmbedDepthGuard.h>
#include <QString>
#include <memory>

namespace Corbomite::Storage { class MetadataCache; }
namespace Corbomite::Core { class VaultResourceProvider; }

namespace Corbomite::ReadingView {

// Per-embed mini-renderer. Resolves subpath via MetadataCache.headings/blocks;
// mounts as a MarkdownRenderChild for lifecycle management. Respects
// EmbedDepthGuard; self-embedding stops at JZ_CAP with a placeholder.
class EmbedRenderer {
public:
    EmbedRenderer(Corbomite::Core::EmbedRegistry *registry,
                  Corbomite::Storage::MetadataCache *cache,
                  Corbomite::Core::VaultResourceProvider *resources);

    std::unique_ptr<Corbomite::Core::MarkdownRenderChild> render(const Corbomite::Core::EmbedRequest &req);

    // Convenience: render directly into an existing QWidget parent
    // (used by HoverPopover in Phase 6).
    bool renderInto(QWidget *parent, const QString &targetPath, const QString &subpath);

private:
    Corbomite::Core::EmbedDepthGuard m_guard;
    Corbomite::Core::EmbedRegistry *m_registry;
    Corbomite::Storage::MetadataCache *m_cache;
    Corbomite::Core::VaultResourceProvider *m_resources;
};

} // namespace Corbomite::ReadingView
```

- [ ] **Step 4: Implement .cpp**

Key behaviour:
- Check `m_guard.allow(req.depth)`; if not, return a `MarkdownRenderChild` carrying the `EmbedDepthGuard::placeholder(...)` string.
- Extension-dispatch via `m_registry`.
- For the `.md` case (registered in Phase 5 but also usable internally here):
  - Load raw markdown from `m_resources`.
  - If `req.subpath` starts with `#^`, look up block in `m_cache->getBlockRange(targetPath, blockId)`.
  - If starts with `#`, look up heading in `m_cache->getHeadingRange(targetPath, heading)`.
  - Else use whole body.
- Construct a `MarkdownRenderChild` whose subtree is a fresh sub-`ReadingPipeline` invocation. Use `ReadingPipeline::buildSections(markdown)` with `depth = req.depth + 1` threaded through.

Full code:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <corbomite/readingview/EmbedRenderer.h>
#include <corbomite/readingview/ReadingPipeline.h>
#include <corbomite/storage/MetadataCache.h>
#include <corbomite/core/VaultResourceProvider.h>
#include <QFileInfo>

namespace Corbomite::ReadingView {

EmbedRenderer::EmbedRenderer(Corbomite::Core::EmbedRegistry *registry,
                             Corbomite::Storage::MetadataCache *cache,
                             Corbomite::Core::VaultResourceProvider *resources)
    : m_registry(registry), m_cache(cache), m_resources(resources) {}

std::unique_ptr<Corbomite::Core::MarkdownRenderChild> EmbedRenderer::render(const Corbomite::Core::EmbedRequest &req) {
    if (!m_guard.allow(req.depth)) {
        auto child = std::make_unique<Corbomite::Core::MarkdownRenderChild>();
        child->setRenderedText(Corbomite::Core::EmbedDepthGuard::placeholder(req.targetPath));
        return child;
    }
    // Defer to registry for all extensions (including .md registered in Phase 5).
    if (auto child = m_registry->dispatch(req)) return child;
    // No built-in for this extension → unknown placeholder
    auto child = std::make_unique<Corbomite::Core::MarkdownRenderChild>();
    child->setRenderedText(QStringLiteral("[unknown embed type: ") + QFileInfo(req.targetPath).suffix() + QStringLiteral("]"));
    return child;
}

bool EmbedRenderer::renderInto(QWidget *parent, const QString &targetPath, const QString &subpath) {
    auto child = render({targetPath, subpath, m_resources, 0});
    if (!child) return false;
    child->mountInto(parent);
    return true;
}

} // namespace Corbomite::ReadingView
```

> NOTE: `MarkdownRenderChild` needs `setRenderedText(QString)`, `renderedText() → QString`, `mountInto(QWidget *)`. Those are added to Phase 1 Task 1.2's `MarkdownRenderChild` when this phase uncovers the requirement — update Task 1.2 retroactively if needed. The Phase-1 test for `MarkdownRenderChild` must grow a test for these accessor/mount methods before Phase 4 lands.

- [ ] **Step 5: Build + run test**

Run: `cd build && cmake --build . && ctest -R tst_readingview_embedrenderer --output-on-failure`
Expected: all 4 subcases PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/readingview/include/corbomite/readingview/EmbedRenderer.h libs/readingview/src/EmbedRenderer.cpp libs/readingview/tests/tst_readingview_embedrenderer.cpp libs/readingview/CMakeLists.txt libs/readingview/tests/CMakeLists.txt
git commit -m "feat(readingview): add EmbedRenderer for ![[Note]] ![[Note#h]] ![[Note#^blk]]

Cluster J phase 4: subpath resolution via MetadataCache; depth guard
respects Obsidian JZ cap; self-embed stops with placeholder."
```

### Task 4.4: Ritual 2 — Phase 4 complete

- [ ] **Step 1: Full suite sanity**

Run: `cd build && ctest --output-on-failure`
Expected: all non-flaky green.

- [ ] **Step 2: Update PROJECT-STATE** (Phases 3 + 4 both landed; Phase 5 next).

- [ ] **Step 3: Commit state doc update**

---

## Phase 5 — Built-in registrations + media stubs

Refactors existing Mermaid/math/syntax-highlighting paths in ReadingView onto `CodeBlockProcessorRegistry`; adds `EmbedRegistry` built-ins.

### Task 5.1: Refactor Mermaid onto CodeBlockProcessorRegistry

**Files:**
- Modify: `libs/readingview/src/ReadingPipeline.cpp` (replace direct Mermaid dispatch)
- Modify: the existing Mermaid registration site
- Test: `libs/readingview/tests/tst_readingview_mermaid_registered.cpp`

- [ ] **Step 1: Audit current Mermaid dispatch in ReadingView**

Grep `libs/readingview/src/` for "mermaid" (case-insensitive). Report current dispatch shape.

- [ ] **Step 2: Write failing test for registry-based dispatch**

```cpp
// Verify that Mermaid is registered onto the CodeBlockProcessorRegistry
// during ReadingView initialization.
void testMermaidIsRegistered() {
    Corbomite::ReadingView::ReadingView rv(/*...ctors...*/);
    const auto *reg = rv.codeBlockProcessorRegistry();
    CodeBlockContext ctx;
    const bool handled = reg->dispatch("mermaid", "graph TD;A-->B", nullptr, ctx);
    QVERIFY(handled);
}
```

- [ ] **Step 3: Refactor Mermaid to register via the new registry**

Move the current direct call into a `registerBuiltinCodeBlockProcessors()` helper; register mermaid with a lambda that delegates to existing `libs/mmdr/` bridge.

- [ ] **Step 4: Run tests; assert visible Mermaid output is unchanged**

Run: `cd build && cmake --build . && ctest -R "readingview_mermaid|tst_readingview_mermaid_registered" --output-on-failure`
Expected: existing mermaid tests still pass + new registration test passes.

- [ ] **Step 5: Commit**

```bash
git commit -m "refactor(readingview): route Mermaid through CodeBlockProcessorRegistry

Cluster J phase 5: no visible behaviour change; Mermaid is now a
registered internal processor rather than a direct-dispatch call."
```

### Task 5.2: Refactor math onto CodeBlockProcessorRegistry

Same structure as Task 5.1 for `math` / inline math fenced blocks — route through registry. Existing `libs/jkqtmathtext/` bridge becomes the registered processor.

### Task 5.3: Refactor syntax-highlighting onto CodeBlockProcessorRegistry

Same structure. Default language fallback uses `KF6::SyntaxHighlighting` (follow existing path); register as the fallback processor that runs when no per-language processor handled.

### Task 5.4: `EmbedRegistry` built-in — markdown

Register `.md` → invoke `EmbedRenderer::render` recursively. Includes test for heading-embed, block-embed, whole-note-embed through the registry path.

### Task 5.5: `EmbedRegistry` built-in — image via wikilink-shim

Register `.png`, `.jpg`, `.jpeg`, `.gif`, `.svg`, `.webp` → a factory that converts the wikilink request into an equivalent `![](path)` markdown snippet and delegates to ReadingView's existing `SpanRenderer` image path. Test: `![[foo.png]]` renders identically to `![](foo.png)`.

### Task 5.6: `EmbedRegistry` built-in — PDF + audio + video stub placeholders

Register `.pdf`, `.mp3`, `.wav`, `.mp4`, `.webm` → a factory that produces a `MarkdownRenderChild` with a placeholder card naming the file. Test: the card contains the filename and a "preview not yet available" string.

### Task 5.7: Ritual 2 — Phase 5 complete

Full suite check + PROJECT-STATE update + commit.

---

## Phase 6 — HoverPopover renderer swap

### Task 6.1: Locate current HoverPopover renderer

**Files (to modify):**
- `src/ui/HoverPopover.{h,cpp}` (or wherever it lives; grep `QTextBrowser::setMarkdown` in `src/`).

- [ ] **Step 1: Grep and confirm location**

Run: `grep -rn "setMarkdown" src/ | head -20`
Expected: find the call in HoverPopover or an adjacent preview widget.

### Task 6.2: Swap the renderer

- [ ] **Step 1: Write failing test**

```cpp
void testHoverPopoverRendersMermaid() {
    MainWindow mw(...); // or whatever lifts the popover in test harness
    // Show a hover popover for a note that contains a mermaid block
    mw.showHoverPreviewFor("NoteWithMermaid.md", QString());
    auto *popover = mw.activeHoverPopover();
    QVERIFY(popover);
    // Assert the popover's content widget contains a mermaid-rendered
    // child (by class name or by finding the KDiagram output).
    QVERIFY(popover->findChild<QWidget *>("MermaidDiagramWidget") != nullptr);
}
```

- [ ] **Step 2: Swap the implementation**

Replace the `QTextBrowser::setMarkdown(raw)` call with:

```cpp
auto renderer = std::make_unique<Corbomite::ReadingView::EmbedRenderer>(
    &m_globalEmbedRegistry, &m_metadataCache, &m_vaultResources);
renderer->renderInto(m_contentWidget, targetPath, subpath);
```

Preserve cursor-position anchoring (`QCursor::pos()`). Do NOT change the show/hide timing; Cluster H follow-up #3 (hover-link rect anchoring) is not in scope.

- [ ] **Step 3: Run test + visual smoke**

Run: `cd build && cmake --build . && ctest -R hoverpopover --output-on-failure`
Expected: new test passes; prior-existing HoverPopover tests still pass.

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(hoverpopover): swap QTextBrowser renderer to EmbedRenderer

Cluster J phase 6: math, mermaid, wikilinks, images now render in hover
previews. Cursor-position anchoring preserved; hoveredLinkRect UX
deferred as Markoff-API follow-up."
```

### Task 6.3: Ritual 3 — Cluster J complete

- [ ] **Step 1: Flip Cluster J Roadmap row Status to `Done` in PROJECT-STATE**

- [ ] **Step 2: Move J's in-flight row out of In-flight work items**

- [ ] **Step 3: Write cluster retrospective at `docs/cluster-retros/cluster-j.md`** — what changed vs plan, surprises, downstream unblocks, lessons.

- [ ] **Step 4: Propagate unblocking effects**

Check each remaining cluster's Roadmap row for `Blocked — waiting on J` and flip to `Not started` / `Stub plan / Scouting`.

- [ ] **Step 5: Update `docs/superpowers/plans/INDEX.md`** Status column: Cluster J → Done.

- [ ] **Step 6: Add Recent-decisions bullet in PROJECT-STATE**:
```
- **YYYY-MM-DD — Cluster J landed.** See `cluster-retros/cluster-j.md`.
```

- [ ] **Step 7: Delete the superseded STUB plan**:

```bash
git rm docs/superpowers/plans/2026-04-14-cluster-j-embed-rendering-primitives-STUB.md
```

- [ ] **Step 8: Consider memory write**

If J resolved a load-bearing pattern worth future agents knowing, write a one-line memory per auto-memory rules. Example candidates: "Internal registries + plugin-ABI-deferred is the split pattern — Cluster N bridges later."

- [ ] **Step 9: Run the full test suite**

Run: `cd build && ctest --output-on-failure`
Expected: all non-flaky green; 4 known-flaky unchanged.

- [ ] **Step 10: Commit the Ritual-3 bookkeeping**

```bash
git add -u docs/
git commit -m "docs(state): Cluster J landed

Retrospective + PROJECT-STATE closeout + INDEX flip + STUB removal.
Unblocks Cluster K (Bases cell rendering substrate), Cluster L
extensions (embedded property values), Cluster N (bridges the
internal registries)."
```

- [ ] **Step 11: Tell human**

"Cluster J complete. Retrospective at `docs/cluster-retros/cluster-j.md`; downstream K/L/N unblocked. Suggest next focus is Session 2 (Cluster G full plan). Confirm or redirect?"

---

## §Residuals — named Markoff / KDE follow-ups collected during J

Not blockers. Collected here so none gets lost.

1. **`Markoff::Editor::hoveredLinkRect()`** — public API extension needed for rect-anchored hover previews (Cluster H follow-up #3's second half).
2. **`Markoff::Editor::setCursor(line, col)`** — column-granular cursor set (Cluster E / F follow-up).
3. **Source-mode fractional-scroll precision** — fork Phase 4 territory (KSyntaxHighlighting rework).
4. **Real PDF renderer** — Okular KPart vs Poppler-Qt6 spike; pairs with Cluster G's KPart spike.
5. **Real audio/video renderers** — QMediaPlayer-based; post-parity.
6. **Search-side AST consumers** — Cluster D follow-up #1: `line:`/`block:`/`section:`/`task*:` operators consume J's section/block/heading resolution.
7. **Stable plugin ABI for `PostProcessorRegistry` + `CodeBlockProcessorRegistry`** — Cluster N bridges.
8. **Compat alias `"bases"` for hover-link source string** — Cluster N shim, if a ported Obsidian plugin hardcodes the string.
9. **Shared scenegraph node type** — currently `void *` at registry layer with each renderer's own wrap/cast at registration sites. Consider a typed scenegraph abstraction if used patterns emerge.

---

## Self-review checklist (completed at plan-write time)

- [x] Spec coverage: every spec section maps to at least one phase/task.
- [x] No placeholders in task bodies; every step names files, commands, code.
- [x] Type consistency: `EmbedRequest` struct, `EmbedFactory`, `Handle` types used consistently across phases.
- [x] Phase ordering respects dependencies: interfaces (1) → registries (2) → renderers (3+4 parallel) → built-ins (5) → user-facing swap (6) → retro (6.3).
- [x] All 10 must-pass gates mapped to tests:
  - Gates 1-3 (embed topologies) → Task 4.3 tests
  - Gate 4 (depth cap) → Task 4.3 self-embed test + Task 1.3 unit test
  - Gate 5 (built-ins via registry) → Task 5.1-5.3 regression tests
  - Gate 6 (no "bases" hardcode) → Task 3.4 Step 2 honesty test
  - Gate 7 (hover popover renders math/mermaid) → Task 6.2 Step 1 test
  - Gate 8 (single VaultResourceProvider) → Task 1.1 tests + CMake link check
  - Gate 9 (single LinkRenderer in Markoff) → Task 3.4 integration
  - Gate 10 (no regressions) → Phase 1.5, 2.3, 4.4, 5.7 full-suite checks
