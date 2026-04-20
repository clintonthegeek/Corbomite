# RapidYAML Port — Design Spec

**Date:** 2026-04-14
**Scope:** Replace yaml-cpp with RapidYAML (ryml) in `libs/markoff-parser`
**Upstream contract:** `libs/markoff-parser/docs/2026-04-14-yaml-api-contract-for-cluster-a.md`

---

## 1. Motivation

yaml-cpp is the sole YAML parser in Corbomite, used only in `libs/markoff-parser/src/Document.cpp` (~75 lines). Cluster A's vault-scanning loop will parse frontmatter for every `.md` file on vault open. ryml benchmarks at 10–200x faster than yaml-cpp. The port also introduces the write/emit side needed by Cluster A's `FrontMatterWriter`.

## 2. License Compatibility

All GPLv3-compatible:

| Component | License |
|---|---|
| rapidyaml | MIT |
| c4core | MIT |
| debugbreak | BSD-2-Clause |
| fast_float | Apache-2.0 / Boost-1.0 / MIT |

## 3. Integration Approach

**Vendored via `add_subdirectory`.** ryml is already cloned at `libs/rapidyaml/` with submodules. The top-level `CMakeLists.txt` adds it before `markoff-parser`. Pinned to current commit (v0.11.1+32).

```cmake
# Top-level CMakeLists.txt — add before markoff-parser
set(RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS ON CACHE BOOL "" FORCE)
add_subdirectory(libs/rapidyaml)
```

```cmake
# libs/markoff-parser/CMakeLists.txt — replace yaml-cpp
target_link_libraries(markoff-parser PRIVATE ryml::ryml)
```

**Error handling:** `RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS=ON` makes ryml throw on parse errors instead of calling `abort()`. Our code catches these with `try`/`catch` and returns empty results, matching the existing yaml-cpp pattern.

## 4. New Public Type: `YamlValue`

A lightweight view into a `ryml::Tree`. Holds `std::shared_ptr<ryml::Tree>` + `ryml::id_type` node index. No eager string copies — scalar access converts `ryml::csubstr` to `QString` on demand.

### 4.1 Header: `include/markoff-parser/YamlValue.h`

```cpp
namespace Markoff {

class YamlValue {
public:
    enum class Kind { Null, Bool, Int, Double, String, Seq, Map };

    YamlValue();  // constructs empty (Null)

    Kind kind() const;
    bool isNull() const;
    bool isBool() const;
    bool isInt() const;
    bool isDouble() const;
    bool isString() const;
    bool isScalar() const;
    bool isSeq() const;
    bool isMap() const;

    // Scalar access
    bool asBool() const;
    int64_t asInt() const;
    double asDouble() const;
    QString asString() const;

    // Container access
    int size() const;
    YamlValue at(int index) const;             // sequence indexing
    bool contains(const QString &key) const;
    YamlValue get(const QString &key) const;   // Null if absent
    QStringList keys() const;                  // document order
    void forEach(std::function<void(const QString&, const YamlValue&)> fn) const;

    // Convenience
    QStringList asStringList() const;

    // Mutation (operates on internal tree, intended for cloned copies)
    YamlValue clone() const;
    void setString(const QString &key, const QString &value);
    void setInt(const QString &key, int64_t value);
    void setDouble(const QString &key, double value);
    void setBool(const QString &key, bool value);
    void setNull(const QString &key);
    void setSeq(const QString &key, const QStringList &values);
    YamlValue setMap(const QString &key);
    YamlValue setSeqNode(const QString &key);
    void remove(const QString &key);
    YamlValue appendMap();
    void appendString(const QString &value);

    // Emit
    QString stringify() const;

private:
    struct Private;
    std::shared_ptr<Private> d;

    // Internal: construct from existing tree + node
    YamlValue(std::shared_ptr<Private> priv, ryml::id_type nodeId);
};

} // namespace Markoff
```

### 4.2 Type Resolution (YAML 1.2 Core Schema Strict)

ryml stores all scalars as `csubstr` without type resolution. `YamlValue::kind()` resolves on access:

| Input | Kind | Notes |
|---|---|---|
| `true`, `True`, `TRUE` | Bool | YAML 1.2 booleans only |
| `false`, `False`, `FALSE` | Bool | |
| `yes`, `no`, `on`, `off`, `y`, `n` | **String** | NOT booleans (YAML 1.2 vs 1.1) |
| `null`, `Null`, `NULL`, `~` | Null | |
| `42`, `-7`, `0x1A`, `0o17` | Int | |
| `3.14`, `.inf`, `-.inf`, `.nan` | Double | |
| everything else | String | |

Quoted scalars (`'foo'`, `"foo"`) are always String regardless of content (ryml tracks `is_val_quoted()`).

### 4.3 Stringify (Obsidian-Compatible Emit)

`YamlValue::stringify()` emits YAML body text (no `---` delimiters) using `ryml::emitrs_yaml<std::string>()`. ryml's emitter preserves node styles (flow/block, quote types, literal/folded), satisfying the round-trip requirement.

For new nodes (mutations), default styles follow Obsidian conventions:
- Maps: block style
- Sequences: block style (one `- item` per line)
- Scalars: plain (unquoted) unless content requires quoting
- Indent: 2 spaces (ryml default)
- Null values: empty string (`key:` not `key: null`)

## 5. Document API Changes

### 5.1 Existing Methods (preserved)

- `frontmatter()` — renamed to `frontmatterRaw()` for clarity. Returns byte-exact raw YAML text between delimiters.
- `parsedFrontmatter()` — return type changes from `QList<FrontmatterProperty>` to `YamlValue`.

### 5.2 New Methods

```cpp
// §1.2 — byte span of entire frontmatter block including delimiters
std::optional<std::pair<int,int>> frontmatterSpan() const;

// §1.5 — closing --- is at EOF with no trailing newline
bool frontmatterHasEofClose() const;

// §2.3 — rebuild file content with new frontmatter
QString withFrontmatter(const YamlValue &value) const;

// §3.2 — diagnostic for malformed YAML
QString frontmatterParseError() const;
```

### 5.3 Deprecated

- `FrontmatterProperty` struct — replaced by `YamlValue` access patterns.

## 6. Round-Trip Invariant

`Document::fromMarkdown(s)->withFrontmatter(Document::fromMarkdown(s)->parsedFrontmatter())` must equal `s`, modulo:
- Trailing newline at EOF may be added if missing
- Comments inside frontmatter may be dropped (document this)

Strategy: `withFrontmatter()` uses `frontmatterRaw()` as a fast path when the YamlValue is the original parse result (not a clone/mutation). For mutated values, emit via ryml.

## 7. Test Plan

### 7.1 Existing Tests (must continue passing)

All 8 tests in `tst_frontmatter.cpp` adapted to new `YamlValue` API.

### 7.2 New Fixture Files

| File | Tests |
|---|---|
| `fixtures/eof-close.md` | EOF-terminated frontmatter |
| `fixtures/yaml-1-1-booleans.md` | `yes`/`no`/`on`/`off` stay strings |
| `fixtures/nested-map-and-list.md` | Order-preserving round-trip |
| `fixtures/bom-and-crlf.md` | Windows-authored vaults |
| `fixtures/comment-in-frontmatter.md` | Comment drop behaviour |
| `fixtures/unicode-keys-and-values.md` | Emoji keys, RTL values |

### 7.3 New Test Cases

- Round-trip invariant on each fixture
- `frontmatterSpan()` byte offsets
- `frontmatterHasEofClose()` flag
- `withFrontmatter()` splice
- YamlValue mutation + stringify
- YAML 1.2 boolean strictness
- Malformed YAML → empty result + diagnostic string
- Thread-safe concurrent reads (same Document)

## 8. Migration Checklist

Per the Cluster A contract §5:

- [ ] `find_package(yaml-cpp)` removed from markoff-parser CMakeLists.txt
- [ ] ryml vendored via `add_subdirectory`; pinned to specific commit
- [ ] `#include <yaml-cpp/yaml.h>` replaced with ryml includes
- [ ] `FrontmatterProperty` deprecated; `YamlValue` in place
- [ ] `frontmatterRaw()`, `frontmatterSpan()`, `frontmatterHasEofClose()` exposed
- [ ] `stringify()` and `withFrontmatter()` exposed
- [ ] Round-trip test passes
- [ ] All existing tests pass
