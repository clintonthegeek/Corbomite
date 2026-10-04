// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace Corbomite {

class DataAdapter;

/// Reads and writes the `.obsidian/` config directory with the
/// *unknown-key preservation* invariant (see
/// `docs/obsidian-audit/VAULT-FORMAT.md §1`).
///
/// Typed accessors cover the known fields; the full JSON object is also
/// exposed so callers can preserve residual keys written by Obsidian, by
/// other Corbomite versions, or by future plugins. All writes round-trip
/// the full object byte-for-byte (modulo 2-space re-indent), never
/// dropping unknown keys.
///
/// Serialisation matches Obsidian's `JSON.stringify(obj, undefined, 2)`:
/// 2-space indent, UTF-8, no trailing newline. ⚠ Key **order is not**
/// preserved — Qt 6's `QJsonObject` stores members sorted, so a Corbomite
/// rewrite emits keys in codepoint order regardless of what was on disk.
/// Obsidian reads it back identically (it parses into a plain object), but a
/// hand-inspected diff against an Obsidian-written file will not be
/// line-stable.
///
/// **Single save path.** Obsidian keeps one merged config map and splits it
/// across two files on every write (`app/own/1129-UD.js:144-149`). The
/// per-file `writeAppJson`/`writeAppearanceJson` helpers below are raw file
/// writers; anything going through the merged config must use
/// `setConfigValue()` or `saveConfig()` so a key cannot land in the wrong
/// file.
class VaultConfig
{
public:
    /// Serialise a JSON document using Obsidian's exact wire format:
    /// `JSON.stringify(value, undefined, 2)` — 2-space indent, UTF-8, no
    /// trailing newline. Shared by every Corbomite writer that targets a
    /// `.obsidian/*.json` file so plugin-side and built-in writers agree
    /// byte-for-byte (avoids diff churn when vaults are shared with
    /// Obsidian).
    static QByteArray serializeObsidianStyle(const QJsonDocument &doc);
    static QByteArray serializeObsidianStyle(const QJsonObject &obj);

    /// Translate Corbomite's KConfig `Appearance/Theme` token (one of
    /// `system` / `light` / `dark`) into the value vocabulary Obsidian uses
    /// inside `.obsidian/appearance.json` (`""` for follow-OS, `moonstone`
    /// for light, `obsidian` for dark; arbitrary CSS-theme names pass
    /// through verbatim). Without this translation the file is technically
    /// "round-tripped" but means different things to each tool.
    static QString obsidianAppearanceTheme(const QString &corbomiteTheme);

    VaultConfig(DataAdapter *fs, const QString &vaultRoot);

    /// Ensure `.obsidian/` exists; returns true on success.
    bool ensureConfigDir() const;

    /// Absolute path to the `.obsidian/` directory.
    QString configDir() const;

    /// Absolute path to a specific config file, e.g. `"app.json"`.
    QString configFilePath(const QString &fileName) const;

    // --- Generic JSON I/O (works for any .obsidian/*.json file) ---

    /// Load a JSON object from `configDir()/fileName`. Returns nullopt on
    /// missing or malformed file.
    std::optional<QJsonObject> readJson(const QString &fileName) const;

    /// Write a JSON object to `configDir()/fileName` using Obsidian's
    /// exact format (2-space indent, no trailing newline). Creates the
    /// parent directory if needed.
    bool writeJson(const QString &fileName, const QJsonObject &obj) const;

    /// Read `configDir()/fileName` (if it exists), update only the keys
    /// present in `updates`, preserving every other key verbatim, and write
    /// the merged object back. If the file does not exist, writes a fresh
    /// file containing only `updates`. Returns true on write success.
    ///
    /// Top-level merge only — if a key in `updates` holds a nested object,
    /// the entire existing value is replaced wholesale, not deep-merged.
    /// Callers writing into files with nested unknown sub-keys (e.g.
    /// `app.json`'s plugin sub-objects) must read+merge at the relevant
    /// depth themselves before passing the update.
    ///
    /// Use this when persisting Corbomite-owned settings to a vault config
    /// file that may also contain Obsidian-authored keys we don't recognise.
    bool mergeJson(const QString &fileName, const QJsonObject &updates) const;

    // --- app.json / appearance.json partition --------------------------
    //
    // Obsidian splits ONE merged config map across two files by membership
    // of a 16-element array literal (`app/own/0920-eE.js:1173`), consumed at
    // `app/own/1129-UD.js:144`. The 6-key `AppearanceConfig` in
    // `docs/obsidian-audit/domains/vault.md §2` is wrong, and so is that
    // doc's placement of ten further keys in `app.json`.

    /// The exact 16-member allow-list, in source order.
    static const QStringList &appearanceKeys();

    /// True when `key` belongs in `appearance.json`. False means `app.json` —
    /// including for keys Obsidian has never heard of, which is what makes
    /// this safe for unknown-key round-tripping.
    static bool isAppearanceKey(QStringView key);

    /// The merged config map, `Object.assign({}, appearance, app)`
    /// (`app/own/1129-UD.js:87`, `:125`) — **`app.json` wins** on a key
    /// present in both. Absent files contribute nothing; a missing key is
    /// absent from the result rather than defaulted (Obsidian applies its
    /// `ME` defaults inside `getConfig`, not here).
    QJsonObject readConfig() const;

    /// Partition `merged` on `appearanceKeys()` and write **both** files
    /// unconditionally (`app/own/1129-UD.js:144-149`), so a stale copy of a
    /// key in the other file is always cleared. Returns false only if a
    /// write failed; both files are attempted.
    bool saveConfig(const QJsonObject &merged) const;

    /// The one path for writing a single merged-config key. Reads the
    /// merged map, applies `key`, then saves. No-ops when the stored value is
    /// already equal.
    ///
    /// ⚠ Only `QJsonValue(QJsonValue::Undefined)` deletes the key, matching
    /// `setConfig` (`app/own/1129-UD.js:157`). A **default-constructed
    /// `QJsonValue()` is `Null`, not `Undefined`**, so `setConfigValue(k,
    /// QJsonValue())` writes a JSON `null`. Use `removeConfigKey()` to
    /// delete.
    bool setConfigValue(const QString &key, const QJsonValue &value) const;

    /// Delete `key` from the merged config (removing it from whichever file
    /// the partition puts it in).
    bool removeConfigKey(const QString &key) const;

    // --- Typed convenience (pass-through + light coercion) ---

    /// `.obsidian/app.json`. See VAULT-FORMAT.md §3 for known keys.
    /// Raw file writer: does not remove the key from `appearance.json`.
    std::optional<QJsonObject> readAppJson() const;
    bool writeAppJson(const QJsonObject &obj) const;

    /// `.obsidian/appearance.json`. See VAULT-FORMAT.md §3.
    /// Raw file writer: does not remove the key from `app.json`.
    std::optional<QJsonObject> readAppearanceJson() const;
    bool writeAppearanceJson(const QJsonObject &obj) const;

    /// `.obsidian/community-plugins.json` — always an array of plugin IDs.
    std::optional<QStringList> readCommunityPlugins() const;
    bool writeCommunityPlugins(const QStringList &pluginIds) const;

    /// `.obsidian/hotkeys.json`. Object keyed by command ID.
    std::optional<QJsonObject> readHotkeys() const;
    bool writeHotkeys(const QJsonObject &obj) const;

    /// `.obsidian/daily-notes.json`. See
    /// `addenda/2026-04-15-daily-notes-templates-schemas.md` for the schema.
    std::optional<QJsonObject> readDailyNotesJson() const;
    bool writeDailyNotesJson(const QJsonObject &obj) const;

    /// `.obsidian/templates.json`. See
    /// `addenda/2026-04-15-daily-notes-templates-schemas.md` for the schema.
    std::optional<QJsonObject> readTemplatesJson() const;
    bool writeTemplatesJson(const QJsonObject &obj) const;

    // --- Core plugins (with legacy array→object migration) ---

    struct CorePlugins {
        /// `{"command-palette": true, "file-explorer": false}` — in-memory
        /// representation. Unknown/extra keys outside this map live in
        /// `raw` so round-trip preserves them.
        QJsonObject raw;
    };

    /// Read `.obsidian/core-plugins.json`. If legacy array-format is
    /// detected, also reads `.obsidian/core-plugins-migration.json`,
    /// merges, writes the modern object-format back, and deletes the
    /// legacy migration file — matching Obsidian's
    /// `InternalPlugins.loadConfig` behaviour (VAULT-FORMAT.md §3).
    std::optional<CorePlugins> readCorePlugins() const;
    bool writeCorePlugins(const CorePlugins &cp) const;

    // --- Core-plugin data: `.obsidian/<corePluginId>.json` --------------
    //
    // A file class `docs/obsidian-audit/domains/settings.md` omits
    // entirely. Internal (core) plugin data goes through
    // `vault.readConfigJson(pluginId)` / `writeConfigJson(pluginId, …)`
    // (`app/own/1939-m4.js:633`, `:642`), i.e. a single-segment
    // `.obsidian/<id>.json`. That is a *different* path from
    // `.obsidian/plugins/<id>/data.json`, which is community-plugin data
    // only (`app/own/1129-UD.js:158-160`) and lands in a different Sync
    // class (`core-plugin-data` vs `community-plugin-data`,
    // `app/own/2043-Xte.js:314`).

    /// `.obsidian/<corePluginId>.json`, e.g. `graph.json` for `graph` and
    /// `workspace.json` for `workspaces`. `corePluginId` is the bare id, not
    /// a filename: Obsidian appends `.json` itself. Returns nullopt when the
    /// file is missing, malformed, non-object, or the id is rejected as
    /// path-unsafe (empty, or containing `/`, `\`, or `..`).
    std::optional<QJsonObject> readCorePluginConfig(const QString &corePluginId) const;

    /// Write `.obsidian/<corePluginId>.json` in Obsidian's wire format.
    /// Returns false for a path-unsafe id.
    bool writeCorePluginConfig(const QString &corePluginId, const QJsonObject &obj);

    /// `vault.deleteConfigJson(pluginId)` (`app/own/1939-m4.js:655`).
    /// False when the id is rejected or the unlink fails.
    bool deleteCorePluginConfig(const QString &corePluginId);

    // --- Value encodings ------------------------------------------------
    //
    // These are on-disk string contracts, not enums. Getting one wrong
    // writes a value Obsidian silently misreads.

    /// The four `openBehavior` dropdown values (`app/own/2062-uie.js:541`):
    /// `""` (last opened), `"new"`, `"file"`, `"daily"`. `openBehavior` is
    /// **not** the audit's `'default' | 'tab' | 'split' | 'window'`.
    static const QStringList &openBehaviorModes();

    /// `openBehavior` stored value → the file path it names. Empty unless
    /// the value has the `"file:"` prefix (`app/own/2062-uie.js:542`), which
    /// is how the dropdown recognises "a specific file" mode.
    static QString openBehaviorPath(const QString &stored);

    /// Encode a specific-file target as `"file:" + path`
    /// (`app/own/2062-uie.js:534`).
    static QString encodeOpenBehaviorFile(const QString &path);

    /// `attachmentFolderPath` is a four-way discriminated string, decoded at
    /// `app/own/2062-uie.js:594`.
    enum class AttachmentLocation {
        Root,          ///< on disk `"/"` (the **vault root**, not `"."`)
        CurrentFolder, ///< on disk `"."` or `"./"`
        Subfolder,     ///< on disk `"./" + value`
        Folder,        ///< on disk `value`, verbatim
    };

    struct AttachmentPath {
        AttachmentLocation location = AttachmentLocation::Root;
        /// Subfolder name when `location == Subfolder`; folder path when
        /// `location == Folder`; empty otherwise.
        QString value;
    };

    /// A stored value that is none of `"/"`, `"."`, `"./"` or a `"./…"`
    /// prefix decodes as `Folder` carrying itself as the value — faithful to
    /// Obsidian's catch-all arm, including for the empty string.
    static AttachmentPath decodeAttachmentFolderPath(const QString &stored);

    /// Encode back to the wire form (`app/own/2062-uie.js:598`). Obsidian
    /// substitutes a localised placeholder for an empty text field, so pass
    /// that in `fallback` when you have one; the default writes the empty
    /// string for `Folder` and `"./"` for `Subfolder`.
    static QString encodeAttachmentFolderPath(const AttachmentPath &decoded,
                                              const QString &fallback = {});

    /// Read side for `textFontFamily` / `interfaceFontFamily` /
    /// `monospaceFontFamily`: the three keys hold a **comma-joined string**,
    /// not a JSON array. Every `"` is stripped, so a quoted family name
    /// containing a comma cannot round-trip
    /// (`app/own/2049-zne.js:2-8`).
    static QStringList decodeFontFamilyStack(const QString &stored);

    /// Write side: `families.join(",")`, verbatim and unescaped
    /// (`app/own/2061-oie.js:271`).
    static QString encodeFontFamilyStack(const QStringList &families);

    /// `nativeMenus` is **tri-state**: `null`/absent means "follow the OS"
    /// (`app/own/0920-eE.js:1203`); the toggle's read side is the live menu
    /// singleton and only its write side touches config
    /// (`app/own/2062-uie.js:1025-1026`). Returns nullopt for null, absent,
    /// or any non-boolean.
    std::optional<bool> nativeMenus() const;

    /// Persist the tri-state; `std::nullopt` writes JSON `null` (the
    /// "follow the OS" sentinel), not a missing key.
    bool setNativeMenus(std::optional<bool> followOs);

    /// `openBehavior` from the merged config map; empty when absent.
    QString openBehavior() const;

    /// `attachmentFolderPath` from the merged config map. Absent decodes as
    /// the Obsidian default (`"/"` → `Root`).
    AttachmentPath attachmentFolderPath() const;

    /// One of the three font keys, decoded. `key` is normally one of
    /// `textFontFamily` / `interfaceFontFamily` / `monospaceFontFamily`.
    QStringList fontFamilyStack(const QString &key) const;

    // --- app.json "Files & Links" settings -------------------------------
    //
    // Obsidian stores these per-vault in `.obsidian/app.json`
    // (obsidian-audit/domains/settings.md §".obsidian/app.json"). Corbomite
    // used to read the equivalent behaviour from its own KConfig only, so a
    // vault authored in Obsidian silently lost those settings. These accessors
    // make the vault value authoritative with the KConfig value as fallback.

    /// How deletes are performed. Mirrors Obsidian's `trashOption` vocabulary.
    enum class TrashMode {
        SystemTrash,  ///< "system"   — move to the OS trash
        VaultTrash,   ///< "vault"    — move into <vault>/.trash
        Permanent,    ///< "permanent"— unlink, no trash at all
    };

    /// `app.json`'s `trashOption`. Unknown/absent values fall back to
    /// `fallback` (normally the Corbomite KConfig setting), so a hand-edited or
    /// future-vocabulary value cannot silently change delete behaviour.
    TrashMode appTrashMode(TrashMode fallback) const;

    /// `app.json`'s `promptDelete`. Absent key => `fallback`.
    bool appPromptDelete(bool fallback) const;

    /// `app.json`'s `alwaysUpdateLinks` — whether a file rename rewrites links
    /// pointing at it. Absent key => `fallback`.
    bool appAlwaysUpdateLinks(bool fallback) const;

    /// `app.json`'s `useMarkdownLinks` — false means Obsidian's
    /// shortest-unique *wiki* links, true means Markdown `[text](path)` links.
    /// Absent key => `fallback`.
    bool appUseMarkdownLinks(bool fallback) const;

    /// `app.json`'s `newLinkFormat` — "relative" (default) or "absolute".
    /// Only meaningful when `useMarkdownLinks` is true. Returns an empty string
    /// when absent.
    QString appNewLinkFormat() const;

    // --- Convenience: userIgnoreFilters from app.json ---

    /// Extract `userIgnoreFilters` from app.json if present. Empty list on
    /// missing or malformed. The default is `null`, not `[]`
    /// (`app/own/0920-eE.js:1208`), and the settings page writes `null` when
    /// the list is emptied (`app/own/2062-uie.js:738`) — both decode to an
    /// empty list here. Entries are regexes, not globs; feed them to
    /// `IgnoreFilter::fromPatterns`.
    QStringList userIgnoreFilters() const;

    /// Persist `userIgnoreFilters` with Obsidian's empty→`null` rule
    /// (`app/own/2062-uie.js:738`).
    bool setUserIgnoreFilters(const QStringList &filters);

private:
    static bool isSafeConfigName(const QString &name);

    DataAdapter *m_fs = nullptr;
    QString m_vaultRoot;
};

} // namespace Corbomite
