# Cluster D.1 — Bases Backend Correctness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Bases evaluator resolve vault-bound functions (`file()`, `LinkValue.asFile`, `LinkValue.linksTo`) against the vault and match hierarchical tags one-directionally, with behaviour pinned by unit tests.

**Architecture:** Introduce a narrow `VaultResolver` interface surfaced through one accessor on `EvalContext`. `QueryController` (the single site that builds `BasesEntry` rows) owns one concrete `BasesVaultResolver` per recompute and threads a `const VaultResolver*` into each entry. Builtins call `ctx.vault()` and fall back to today's behaviour when unbound. Tag matching is fixed in `TagValue::tagMatches` (directionality) and `ListValue::includes` (special-case for tag elements).

**Tech Stack:** C++20, Qt6, QtTest, CMake. Libraries: `libs/bases` (`corbomite-bases`), depends on `libs/storage` (`LinkResolver`, `MetadataCache`) and `libs/vault` (`Vault`, `TFile`).

---

## File structure

- **Create** `libs/bases/include/corbomite/bases/VaultResolver.h` — the abstract seam (`fileAt`, `resolveLinkTarget`).
- **Create** `libs/bases/include/corbomite/bases/BasesVaultResolver.h` + `libs/bases/src/BasesVaultResolver.cpp` — concrete resolver wrapping `Vault*` + `MetadataCache*` + an owned `LinkResolver`.
- **Modify** `libs/bases/include/corbomite/bases/EvalContext.h` — add `virtual const VaultResolver* vault() const`.
- **Modify** `libs/bases/include/corbomite/bases/BasesEntry.h` + `src/BasesEntry.cpp` — store + return a `const VaultResolver*`.
- **Modify** `libs/bases/src/QueryController.cpp` (+ `QueryController.h`) — own the resolver, pass it to entries.
- **Modify** `libs/bases/src/Builtins.cpp` — `file()`, `asFile`, `linksTo`.
- **Modify** `libs/bases/src/StringSubclasses.cpp` — `TagValue::tagMatches` directionality.
- **Modify** `libs/bases/src/ListValue.cpp` — `includes` tag special-case.
- **Modify** `libs/bases/CMakeLists.txt` — add `src/BasesVaultResolver.cpp`.
- **Test (existing files, new slots):** `libs/bases/tests/tst_builtins.cpp` (vault-bound functions), `tst_value_string_subclasses.cpp` (tagMatches), `tst_value_list.cpp` (includes + sort).

Build command (whole tree): `cmake --build --preset dev -j 10`
Single test: `cd build-dev && ctest -R <name> --output-on-failure`

---

### Task 1: `VaultResolver` interface + `EvalContext::vault()` accessor

**Files:**
- Create: `libs/bases/include/corbomite/bases/VaultResolver.h`
- Modify: `libs/bases/include/corbomite/bases/EvalContext.h`

- [ ] **Step 1: Create the interface header**

Create `libs/bases/include/corbomite/bases/VaultResolver.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ValuePtr.h"

#include <QString>

namespace Corbomite::Bases {

/// Narrow vault-access seam for vault-bound builtins (file(), asFile,
/// linksTo). Implemented by BasesVaultResolver; exposed via
/// EvalContext::vault(). Contexts without a vault return nullptr from
/// vault() and builtins fall back to non-vault behaviour.
class VaultResolver
{
public:
    virtual ~VaultResolver() = default;

    /// Resolve a vault-relative path (or bare/short name) to a FileValue;
    /// returns NullValue::instance() when nothing resolves.
    virtual ValuePtr fileAt(const QString &pathOrName) const = 0;

    /// Resolve a wiki/markdown link's target text to a canonical
    /// vault-relative path (Obsidian getLinkpathDest). Empty if unresolved.
    /// `sourcePath` is the link's origin note (for relative/short resolution).
    virtual QString resolveLinkTarget(const QString &linkData,
                                      const QString &sourcePath) const = 0;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Add the accessor to `EvalContext`**

In `libs/bases/include/corbomite/bases/EvalContext.h`, add a forward declaration and the accessor inside `class EvalContext` (after `keys()`):

```cpp
namespace Corbomite::Bases {

class VaultResolver;   // <-- add near the top, inside the namespace

class EvalContext
{
public:
    virtual ~EvalContext() = default;
    virtual ValuePtr getByIdentifier(const QString &name) const = 0;
    virtual QStringList keys() const { return {}; }

    /// Vault-access seam for vault-bound builtins. Default: unbound.
    virtual const VaultResolver *vault() const { return nullptr; }
};
```

- [ ] **Step 3: Build to confirm headers compile**

Run: `cmake --build --preset dev -j 10 --target corbomite-bases`
Expected: builds clean (no behaviour change).

- [ ] **Step 4: Commit**

```bash
git add libs/bases/include/corbomite/bases/VaultResolver.h libs/bases/include/corbomite/bases/EvalContext.h
git commit -m "feat(bases): VaultResolver seam + EvalContext::vault() accessor"
```

---

### Task 2: `BasesVaultResolver` concrete implementation

**Files:**
- Create: `libs/bases/include/corbomite/bases/BasesVaultResolver.h`, `libs/bases/src/BasesVaultResolver.cpp`
- Modify: `libs/bases/CMakeLists.txt:44` (add source)

- [ ] **Step 1: Create the header**

Create `libs/bases/include/corbomite/bases/BasesVaultResolver.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "VaultResolver.h"

#include <corbomite/storage/LinkResolver.h>

namespace Corbomite {
class Vault;
class MetadataCache;
}  // namespace Corbomite

namespace Corbomite::Bases {

/// VaultResolver backed by a live Vault + MetadataCache. Seeds an owned
/// LinkResolver from the vault's full path set at construction; rebuild
/// (reconstruct) when the vault path set changes — QueryController does
/// this once per recompute.
class BasesVaultResolver : public VaultResolver
{
public:
    BasesVaultResolver(Vault *vault, MetadataCache *cache);

    ValuePtr fileAt(const QString &pathOrName) const override;
    QString resolveLinkTarget(const QString &linkData,
                              const QString &sourcePath) const override;

private:
    Vault *m_vault;
    MetadataCache *m_cache;
    LinkResolver m_links;
};

}  // namespace Corbomite::Bases
```

- [ ] **Step 2: Create the implementation**

Create `libs/bases/src/BasesVaultResolver.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesVaultResolver.h"

#include "corbomite/bases/Values.h"

#include <corbomite/vault/Vault.h>
#include <corbomite/core/TFile.h>

namespace Corbomite::Bases {

BasesVaultResolver::BasesVaultResolver(Vault *vault, MetadataCache *cache)
    : m_vault(vault), m_cache(cache)
{
    QStringList paths;
    if (m_vault) {
        const QVector<TFile *> files = m_vault->getFiles();
        paths.reserve(files.size());
        for (TFile *f : files)
            if (f) paths << f->path;
    }
    m_links.setVaultPaths(paths);
}

ValuePtr BasesVaultResolver::fileAt(const QString &pathOrName) const
{
    if (!m_vault) return NullValue::instance();

    TFile *f = m_vault->getFileByPath(pathOrName);
    if (!f) {
        const ResolvedLink r = m_links.resolve(QString{}, pathOrName);
        if (r.resolved) f = m_vault->getFileByPath(r.path);
    }
    if (!f) return NullValue::instance();
    return std::make_shared<FileValue>(f, m_cache);
}

QString BasesVaultResolver::resolveLinkTarget(const QString &linkData,
                                              const QString &sourcePath) const
{
    const ResolvedLink r = m_links.resolve(sourcePath, linkData);
    return r.resolved ? r.path : QString{};
}

}  // namespace Corbomite::Bases
```

> Note: the exact include path for `TFile.h` is whatever `BasesEntry.cpp` already uses for `TFile` — match it (it forward-declares `Corbomite::TFile`; the `->path` member is used by `FileValue`/`BasesEntry`, so the include is already established in the bases sources). If `TFile` is header-only via `Vault.h`, drop the separate include.

- [ ] **Step 3: Add the source to CMake**

In `libs/bases/CMakeLists.txt`, add after `src/BasesEntry.cpp` (line 44):

```cmake
    src/BasesEntry.cpp
    src/BasesVaultResolver.cpp
```

- [ ] **Step 4: Build**

Run: `cmake --build --preset dev -j 10 --target corbomite-bases`
Expected: builds clean. (`BasesVaultResolver` is not yet referenced; this just confirms it compiles + links.)

- [ ] **Step 5: Commit**

```bash
git add libs/bases/include/corbomite/bases/BasesVaultResolver.h libs/bases/src/BasesVaultResolver.cpp libs/bases/CMakeLists.txt
git commit -m "feat(bases): BasesVaultResolver — Vault/MetadataCache-backed resolver"
```

---

### Task 3: Thread the resolver through `QueryController` → `BasesEntry`

**Files:**
- Modify: `libs/bases/include/corbomite/bases/BasesEntry.h`, `libs/bases/src/BasesEntry.cpp`
- Modify: `libs/bases/include/corbomite/bases/QueryController.h`, `libs/bases/src/QueryController.cpp:84-90`

- [ ] **Step 1: Add resolver param + override to `BasesEntry`**

In `BasesEntry.h`: forward-declare `class VaultResolver;` (inside `Corbomite::Bases`), add the last ctor param, the override, and the member:

```cpp
    BasesEntry(Vault *vault,
               MetadataCache *cache,
               TFile *file,
               TFile *localFile,
               const BasesQuery &query,
               FunctionRegistry *funcs = nullptr,
               const VaultResolver *resolver = nullptr);
    ~BasesEntry() override;

    // ... existing public methods ...
    const VaultResolver *vault() const override { return m_resolver; }
```

Add to the private members:

```cpp
    const VaultResolver *m_resolver = nullptr;
```

- [ ] **Step 2: Store the param in the ctor**

In `BasesEntry.cpp`, update the ctor signature + init list:

```cpp
BasesEntry::BasesEntry(Vault *vault, MetadataCache *cache,
                       TFile *file, TFile *localFile,
                       const BasesQuery &query, FunctionRegistry *funcs,
                       const VaultResolver *resolver)
    : m_vault(vault),
      m_cache(cache),
      m_file(file),
      m_local(localFile),
      m_query(query),
      m_funcs(funcs ? funcs : &FunctionRegistry::global()),
      m_resolver(resolver)
{
}
```

Add `#include "corbomite/bases/VaultResolver.h"` to `BasesEntry.cpp` if needed for the type to be complete (it is used only as a pointer, so the forward declaration in the header suffices; include only if the compiler complains).

- [ ] **Step 3: Own + build the resolver in `QueryController`**

In `QueryController.h`, add the member (with the other members):

```cpp
    std::unique_ptr<BasesVaultResolver> m_resolver;
```

and forward-declare / include as needed (`#include "corbomite/bases/BasesVaultResolver.h"` in `QueryController.cpp`).

In `QueryController.cpp`, inside `recomputeNow()`, immediately after the `const QVector<TFile *> files = m_vault->getMarkdownFiles();` line, build the resolver and pass it into each entry:

```cpp
    const QVector<TFile *> files = m_vault->getMarkdownFiles();

    // One resolver per recompute, seeded from the full vault path set.
    m_resolver = std::make_unique<BasesVaultResolver>(m_vault, m_cache);

    QVector<std::shared_ptr<BasesEntry>> entries;
    entries.reserve(files.size());
    for (TFile *f : files) {
        entries.push_back(std::make_shared<BasesEntry>(
            m_vault, m_cache, f, m_local ? m_local : f, *m_query, m_funcs,
            m_resolver.get()));
    }
```

- [ ] **Step 4: Build + run existing bases suite (no behaviour change yet)**

Run: `cmake --build --preset dev -j 10`
Then: `cd build-dev && ctest -R 'tst_(builtins|evaluator|value_|basesentry|yaml_schema)' --output-on-failure`
Expected: all existing bases tests still PASS.

- [ ] **Step 5: Commit**

```bash
git add libs/bases/include/corbomite/bases/BasesEntry.h libs/bases/src/BasesEntry.cpp libs/bases/include/corbomite/bases/QueryController.h libs/bases/src/QueryController.cpp
git commit -m "feat(bases): thread VaultResolver through QueryController into BasesEntry"
```

---

### Task 4: `file()` global resolves via the vault

**Files:**
- Test: `libs/bases/tests/tst_builtins.cpp`
- Modify: `libs/bases/src/Builtins.cpp:166-175`

- [ ] **Step 1: Add a fake resolver + context to the test file**

In `tst_builtins.cpp`, inside the existing anonymous `namespace { ... }`, add:

```cpp
#include "corbomite/bases/VaultResolver.h"

class FakeResolver : public VaultResolver
{
public:
    QSet<QString> files;                 // paths that exist
    QHash<QString, QString> linkMap;     // linkData -> canonical path

    ValuePtr fileAt(const QString &p) const override
    {
        return files.contains(p) ? std::static_pointer_cast<Value>(
                   std::make_shared<FileValue>(nullptr, nullptr))
                                 : NullValue::instance();
    }
    QString resolveLinkTarget(const QString &linkData, const QString &) const override
    {
        return linkMap.value(linkData);  // "" if absent
    }
};

class VaultCtx : public EvalContext
{
public:
    const FakeResolver *res = nullptr;
    QHash<QString, ValuePtr> ids;        // identifier -> value (for lnk.* tests)

    ValuePtr getByIdentifier(const QString &n) const override { return ids.value(n); }
    const VaultResolver *vault() const override { return res; }
};
```

(Add `#include <QSet>` / `#include <QHash>` at the top if not present.)

- [ ] **Step 2: Write the failing test**

Add these slots to `TestBuiltins`:

```cpp
    void testFileGlobalResolvesViaVault()
    {
        FakeResolver r; r.files.insert(QStringLiteral("Notes/X.md"));
        VaultCtx c; c.res = &r;
        auto v = run(QStringLiteral("file('Notes/X.md')"), c);
        QCOMPARE(v->type(), QStringLiteral("File"));
    }

    void testFileGlobalUnboundReturnsNull()
    {
        NullCtx c;  // no vault()
        auto v = run(QStringLiteral("file('Notes/X.md')"), c);
        QCOMPARE(v->type(), QStringLiteral("null"));
    }
```

(If `NullValue::type()` returns something other than `"null"`, match the actual string — check `NullValue.cpp`.)

- [ ] **Step 3: Run to verify it fails**

Run: `cmake --build --preset dev -j 10 --target tst_builtins && cd build-dev && ctest -R tst_builtins --output-on-failure`
Expected: `testFileGlobalResolvesViaVault` FAILS (current stub returns NullValue, so type is "null" not "File").

- [ ] **Step 4: Implement the `file()` global**

In `Builtins.cpp`, replace the `file` global body (around line 166):

```cpp
    r.addGlobal({
        QStringLiteral("file"),
        {requiredParam(QStringLiteral("path"))},
        [](const EvalContext &ctx, const QVector<ValuePtr> &args) -> ValuePtr {
            if (const VaultResolver *v = ctx.vault())
                return v->fileAt(args.isEmpty() ? QString{} : toStr(args[0]));
            return NullValue::instance();
        }});
```

Add `#include "corbomite/bases/VaultResolver.h"` near the top of `Builtins.cpp`.

- [ ] **Step 5: Run to verify pass**

Run: `cmake --build --preset dev -j 10 --target tst_builtins && cd build-dev && ctest -R tst_builtins --output-on-failure`
Expected: both new slots PASS; all prior `tst_builtins` cases still PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/bases/tests/tst_builtins.cpp libs/bases/src/Builtins.cpp
git commit -m "feat(bases): file() global resolves via VaultResolver"
```

---

### Task 5: `LinkValue::asFile` + `linksTo` resolve via the vault

**Files:**
- Test: `libs/bases/tests/tst_builtins.cpp`
- Modify: `libs/bases/src/Builtins.cpp:564-578` (`registerLinkMethods`)

- [ ] **Step 1: Write the failing tests**

Add slots to `TestBuiltins` (reuses `FakeResolver`/`VaultCtx` from Task 4):

```cpp
    void testLinkAsFileResolves()
    {
        FakeResolver r;
        r.linkMap.insert(QStringLiteral("Foo"), QStringLiteral("Notes/Foo.md"));
        r.files.insert(QStringLiteral("Notes/Foo.md"));
        VaultCtx c; c.res = &r;
        c.ids.insert(QStringLiteral("lnk"),
                     std::make_shared<LinkValue>(QStringLiteral("Foo"),
                                                 QStringLiteral("src.md")));
        auto v = run(QStringLiteral("lnk.asFile"), c);
        QCOMPARE(v->type(), QStringLiteral("File"));
    }

    void testLinkLinksToResolvesCanonical()
    {
        FakeResolver r;
        r.linkMap.insert(QStringLiteral("Foo"), QStringLiteral("Notes/Foo.md"));
        r.linkMap.insert(QStringLiteral("Notes/Foo.md"), QStringLiteral("Notes/Foo.md"));
        VaultCtx c; c.res = &r;
        c.ids.insert(QStringLiteral("lnk"),
                     std::make_shared<LinkValue>(QStringLiteral("Foo"),
                                                 QStringLiteral("src.md")));
        auto yes = run(QStringLiteral("lnk.linksTo('Notes/Foo.md')"), c);
        auto no  = run(QStringLiteral("lnk.linksTo('Other.md')"), c);
        QCOMPARE(std::static_pointer_cast<BooleanValue>(yes)->data(), true);
        QCOMPARE(std::static_pointer_cast<BooleanValue>(no)->data(), false);
    }
```

(If `BooleanValue::data()` is named differently, match it — check `BooleanValue.h`.)

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build --preset dev -j 10 --target tst_builtins && cd build-dev && ctest -R tst_builtins --output-on-failure`
Expected: `testLinkAsFileResolves` FAILS (stub returns NullValue → type "null"); `testLinkLinksToResolvesCanonical` FAILS for the `yes` case (string-equality `"Foo" != "Notes/Foo.md"`).

- [ ] **Step 3: Implement `asFile` + `linksTo`**

In `Builtins.cpp`, replace the two `addForType(typeid(LinkValue), …)` registrations in `registerLinkMethods`:

```cpp
    r.addForType(typeid(LinkValue), { QStringLiteral("asFile"), {},
        [subj](const EvalContext &ctx, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *lv = subj(args);
            if (!lv) return NullValue::instance();
            if (const VaultResolver *v = ctx.vault()) {
                const QString path = v->resolveLinkTarget(lv->data(), lv->sourcePath());
                if (!path.isEmpty()) return v->fileAt(path);
            }
            return NullValue::instance();
        }});
    r.addForType(typeid(LinkValue), { QStringLiteral("linksTo"),
        {requiredParam(QStringLiteral("other"))},
        [subj](const EvalContext &ctx, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *lv = subj(args);
            if (!lv) return std::make_shared<BooleanValue>(false);
            const QString other = toStr(args.value(1));
            if (const VaultResolver *v = ctx.vault()) {
                const QString tgt = v->resolveLinkTarget(lv->data(), lv->sourcePath());
                const QString oth = v->resolveLinkTarget(other, lv->sourcePath());
                return std::make_shared<BooleanValue>(!tgt.isEmpty() && tgt == oth);
            }
            return std::make_shared<BooleanValue>(lv->data() == other);  // unbound fallback
        }});
```

- [ ] **Step 4: Run to verify pass**

Run: `cmake --build --preset dev -j 10 --target tst_builtins && cd build-dev && ctest -R tst_builtins --output-on-failure`
Expected: both new slots PASS; existing cases still PASS.

- [ ] **Step 5: Commit**

```bash
git add libs/bases/tests/tst_builtins.cpp libs/bases/src/Builtins.cpp
git commit -m "feat(bases): LinkValue.asFile + linksTo resolve via VaultResolver"
```

---

### Task 6: `TagValue::tagMatches` one-directional

**Files:**
- Test: `libs/bases/tests/tst_value_string_subclasses.cpp`
- Modify: `libs/bases/src/StringSubclasses.cpp:11-19`

- [ ] **Step 1: Write the failing test**

Add a slot to the test class in `tst_value_string_subclasses.cpp` (match the file's existing class name + style):

```cpp
    void testTagMatchesOneDirectional()
    {
        TagValue child(QStringLiteral("#parent/child"));
        QVERIFY(child.tagMatches(QStringLiteral("#parent")));          // stored under query
        QVERIFY(child.tagMatches(QStringLiteral("#parent/child")));    // equal
        QVERIFY(!child.tagMatches(QStringLiteral("#parent/other")));   // sibling

        TagValue parent(QStringLiteral("#parent"));
        QVERIFY(!parent.tagMatches(QStringLiteral("#parent/child")));  // reverse must NOT match
    }
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build --preset dev -j 10 --target tst_value_string_subclasses && cd build-dev && ctest -R tst_value_string_subclasses --output-on-failure`
Expected: FAILS on the last assertion (current bidirectional code returns true for the reverse).

- [ ] **Step 3: Make `tagMatches` one-directional**

In `StringSubclasses.cpp`, replace the body of `TagValue::tagMatches`:

```cpp
bool TagValue::tagMatches(const QString &other) const
{
    if (m_data == other) return true;
    // One-directional (Obsidian): this stored tag matches a query `other`
    // iff this is `other` or a subtag of it. The reverse does NOT match.
    return m_data.startsWith(other + QLatin1Char('/'));
}
```

- [ ] **Step 4: Run to verify pass**

Run: `cmake --build --preset dev -j 10 --target tst_value_string_subclasses && cd build-dev && ctest -R 'tst_value_string_subclasses|tst_value_' --output-on-failure`
Expected: new slot PASSES; existing value tests still PASS. (If a pre-existing test relied on the reverse direction, update that test to the corrected semantics — the spec defines one-directional as correct.)

- [ ] **Step 5: Commit**

```bash
git add libs/bases/tests/tst_value_string_subclasses.cpp libs/bases/src/StringSubclasses.cpp
git commit -m "fix(bases): TagValue::tagMatches one-directional (subtag-of-query only)"
```

---

### Task 7: `ListValue::includes` special-cases tag elements

**Files:**
- Test: `libs/bases/tests/tst_value_list.cpp`
- Modify: `libs/bases/src/ListValue.cpp:49-56`

- [ ] **Step 1: Write the failing test**

Add a slot to the test class in `tst_value_list.cpp`:

```cpp
    void testIncludesHierarchicalTag()
    {
        QVector<ValuePtr> elems{ std::make_shared<TagValue>(QStringLiteral("#parent/child")) };
        auto list = std::make_shared<ListValue>(elems);

        QVERIFY(list->includes(std::make_shared<TagValue>(QStringLiteral("#parent"))));
        QVERIFY(list->includes(std::make_shared<StringValue>(QStringLiteral("#parent"))));
        QVERIFY(list->includes(std::make_shared<TagValue>(QStringLiteral("#parent/child"))));
        QVERIFY(!list->includes(std::make_shared<TagValue>(QStringLiteral("#parent/other"))));
    }
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build --preset dev -j 10 --target tst_value_list && cd build-dev && ctest -R tst_value_list --output-on-failure`
Expected: FAILS — generic `staticLooseEquals` won't match `#parent` against `#parent/child`.

- [ ] **Step 3: Special-case tag elements in `includes`**

In `ListValue.cpp`, replace `includes`:

```cpp
bool ListValue::includes(const ValuePtr &v) const
{
    const QString needle = v ? v->toString() : QString{};
    for (const auto &x : m_data) {
        if (auto *tag = dynamic_cast<TagValue *>(x.get())) {
            if (tag->tagMatches(needle)) return true;
        } else if (Value::staticLooseEquals(x.get(), v.get())) {
            return true;
        }
    }
    return false;
}
```

Ensure `TagValue` is visible — `ListValue.cpp` includes `Values.h` (which declares `TagValue`); add the include if absent.

- [ ] **Step 4: Run to verify pass**

Run: `cmake --build --preset dev -j 10 --target tst_value_list && cd build-dev && ctest -R tst_value_list --output-on-failure`
Expected: new slot PASSES; existing list tests still PASS.

- [ ] **Step 5: Commit**

```bash
git add libs/bases/tests/tst_value_list.cpp libs/bases/src/ListValue.cpp
git commit -m "feat(bases): ListValue::includes special-cases hierarchical tag elements"
```

---

### Task 8: Verify-and-pin `ListValue::sort()` null ordering + full suite

**Files:**
- Test: `libs/bases/tests/tst_value_list.cpp`
- Modify (only if the test fails): `libs/bases/src/ListValue.cpp:103-110`

- [ ] **Step 1: Write the pinning test**

Add a slot to `tst_value_list.cpp`:

```cpp
    void testSortPutsNullsLast()
    {
        QVector<ValuePtr> elems{ std::make_shared<NumberValue>(3.0),
                                 NullValue::instance(),
                                 std::make_shared<NumberValue>(1.0) };
        auto sorted = std::make_shared<ListValue>(elems)->sort();
        const auto &out = sorted->data();
        QCOMPARE(out.size(), 3);
        QCOMPARE(std::static_pointer_cast<NumberValue>(out[0])->data(), 1.0);
        QCOMPARE(std::static_pointer_cast<NumberValue>(out[1])->data(), 3.0);
        QVERIFY(dynamic_cast<NullValue *>(out[2].get()) != nullptr);
    }
```

- [ ] **Step 2: Run the test**

Run: `cmake --build --preset dev -j 10 --target tst_value_list && cd build-dev && ctest -R tst_value_list --output-on-failure`
Expected: **PASS** — the current comparator reads as already null-last. If it PASSES, the audit's "nulls first" concern was stale; the test now pins correct behaviour. Skip Step 3.

- [ ] **Step 3: (Only if Step 2 failed) Fix the comparator**

If and only if the test failed, in `ListValue::sort()` ensure nulls sort last by making the comparator: a null `a` is never `<` b (`return false`), a null `b` (with non-null `a`) is always `>` so `a < b` (`return true`). The existing lines already express this; if reversed, swap them so non-null precedes null. Re-run Step 2 to confirm PASS.

- [ ] **Step 4: looseEquals — confirm no change needed**

`looseEquals` is exercised by `ListValue::includes` (Task 7, non-tag branch via `staticLooseEquals`) and the existing `tst_value_*` suites. No dedicated change: run the full value suite and confirm green.

Run: `cd build-dev && ctest -R 'tst_value_|tst_builtins|tst_evaluator|tst_basesentry|tst_yaml_schema' --output-on-failure`
Expected: ALL PASS.

- [ ] **Step 5: Full bases suite + commit**

Run: `cd build-dev && ctest --output-on-failure -j 10`
Expected: entire suite green (no regressions outside bases either).

```bash
git add libs/bases/tests/tst_value_list.cpp libs/bases/src/ListValue.cpp
git commit -m "test(bases): pin ListValue::sort() nulls-last; close D.1"
```

---

## Definition of done

- `file()`, `LinkValue.asFile`, `LinkValue.linksTo` resolve against a vault via `VaultResolver`; unbound contexts keep safe fallbacks (tests cover both).
- `file.tags.contains("#parent")` matches `#parent/child` and not the reverse (`tagMatches` + `includes` tests).
- `ListValue::sort()` null ordering pinned by test; `looseEquals` confirmed unchanged.
- Full `libs/bases` suite green; no UI changes; substrate-independent.

## Post-cluster follow-up (not D.1)

- Add a punch-list item: **`unrecognizedData` non-scalar preservation** (`BasesQuery.cpp` top-level path uses `yamlToVariant` like `BasesViewConfig.cpp` does) — serialization round-trip fidelity, sibling of the Cluster A key-order work.
