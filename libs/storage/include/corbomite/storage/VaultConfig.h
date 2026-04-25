// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include <QJsonObject>
#include <QString>
#include <QStringList>

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
/// 2-space indent, UTF-8, no trailing newline, insertion-order keys.
class VaultConfig
{
public:
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

    // --- Typed convenience (pass-through + light coercion) ---

    /// `.obsidian/app.json`. See VAULT-FORMAT.md §3 for known keys.
    std::optional<QJsonObject> readAppJson() const;
    bool writeAppJson(const QJsonObject &obj) const;

    /// `.obsidian/appearance.json`. See VAULT-FORMAT.md §3.
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

    // --- Convenience: userIgnoreFilters from app.json ---

    /// Extract `userIgnoreFilters` from app.json if present. Empty list on
    /// missing or malformed.
    QStringList userIgnoreFilters() const;

private:
    DataAdapter *m_fs = nullptr;
    QString m_vaultRoot;
};

} // namespace Corbomite
