// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/VaultConfig.h"

#include "corbomite/core/DataAdapter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

namespace Corbomite {

namespace {

constexpr auto kConfigDirName = "/.obsidian";

QString joinConfigPath(const QString &vaultRoot, const QString &tail)
{
    QString base = vaultRoot;
    if (base.endsWith(QLatin1Char('/'))) base.chop(1);
    return base + QString::fromLatin1(kConfigDirName) + QLatin1Char('/') + tail;
}

} // namespace

// Serialise using `JSON.stringify(value, undefined, 2)` — 2-space indent, no
// trailing newline. Qt's QJsonDocument::Indented emits 4-space indent, so we
// squeeze leading runs of spaces at line starts. Key order is Qt's, not the
// file's: Qt 6 sorts QJsonObject members, so a rewrite emits codepoint order
// (VAULT-FORMAT §1 tolerates any order on read).
QByteArray VaultConfig::serializeObsidianStyle(const QJsonDocument &doc)
{
    const QByteArray indented = doc.toJson(QJsonDocument::Indented);

    QByteArray out;
    out.reserve(indented.size());
    for (int i = 0; i < indented.size(); ) {
        const bool atLineStart = (i == 0 || indented[i - 1] == '\n');
        if (atLineStart && indented[i] == ' ') {
            int spaces = 0;
            while (i + spaces < indented.size() && indented[i + spaces] == ' ') ++spaces;
            out.append(QByteArray(spaces / 2, ' '));
            i += spaces;
        } else {
            out.append(indented[i]);
            ++i;
        }
    }
    while (out.endsWith('\n')) out.chop(1);
    return out;
}

QByteArray VaultConfig::serializeObsidianStyle(const QJsonObject &obj)
{
    return serializeObsidianStyle(QJsonDocument(obj));
}

QString VaultConfig::obsidianAppearanceTheme(const QString &corbomiteTheme)
{
    if (corbomiteTheme.isEmpty()
        || corbomiteTheme == QLatin1String("system")) {
        // Obsidian's "follow OS" sentinel is the empty string.
        return {};
    }
    if (corbomiteTheme == QLatin1String("light")) {
        return QStringLiteral("moonstone");
    }
    if (corbomiteTheme == QLatin1String("dark")) {
        return QStringLiteral("obsidian");
    }
    // Anything else is a custom CSS-theme name — pass through unchanged.
    return corbomiteTheme;
}

VaultConfig::VaultConfig(DataAdapter *fs, const QString &vaultRoot)
    : m_fs(fs), m_vaultRoot(vaultRoot)
{
}

QString VaultConfig::configDir() const
{
    QString base = m_vaultRoot;
    if (base.endsWith(QLatin1Char('/'))) base.chop(1);
    return base + QString::fromLatin1(kConfigDirName);
}

QString VaultConfig::configFilePath(const QString &fileName) const
{
    return joinConfigPath(m_vaultRoot, fileName);
}

bool VaultConfig::ensureConfigDir() const
{
    if (!m_fs) return false;
    return m_fs->mkpath(configDir());
}

std::optional<QJsonObject> VaultConfig::readJson(const QString &fileName) const
{
    if (!m_fs) return std::nullopt;
    const auto bytes = m_fs->readBinary(configFilePath(fileName));
    if (!bytes.has_value()) return std::nullopt;
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(*bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }
    return doc.object();
}

bool VaultConfig::writeJson(const QString &fileName, const QJsonObject &obj) const
{
    if (!m_fs) return false;
    if (!ensureConfigDir()) return false;
    return m_fs->writeBinary(configFilePath(fileName), serializeObsidianStyle(obj));
}

bool VaultConfig::mergeJson(const QString &fileName,
                            const QJsonObject &updates) const
{
    QJsonObject merged;
    if (auto existing = readJson(fileName)) {
        merged = *existing;
    }
    for (auto it = updates.begin(); it != updates.end(); ++it) {
        merged.insert(it.key(), it.value());
    }
    return writeJson(fileName, merged);
}

std::optional<QJsonObject> VaultConfig::readAppJson() const
{
    return readJson(QStringLiteral("app.json"));
}

bool VaultConfig::writeAppJson(const QJsonObject &obj) const
{
    return writeJson(QStringLiteral("app.json"), obj);
}

std::optional<QJsonObject> VaultConfig::readAppearanceJson() const
{
    return readJson(QStringLiteral("appearance.json"));
}

bool VaultConfig::writeAppearanceJson(const QJsonObject &obj) const
{
    return writeJson(QStringLiteral("appearance.json"), obj);
}

// --- app.json / appearance.json partition ---

const QStringList &VaultConfig::appearanceKeys()
{
    static const QStringList keys{
        QStringLiteral("accentColor"),
        QStringLiteral("theme"),
        QStringLiteral("cssTheme"),
        QStringLiteral("enabledCssSnippets"),
        QStringLiteral("showViewHeader"),
        QStringLiteral("showRibbon"),
        QStringLiteral("nativeMenus"),
        QStringLiteral("translucency"),
        QStringLiteral("textFontFamily"),
        QStringLiteral("interfaceFontFamily"),
        QStringLiteral("monospaceFontFamily"),
        QStringLiteral("baseFontSize"),
        QStringLiteral("baseFontSizeAction"),
        QStringLiteral("slidingSidebar"),
        QStringLiteral("floatingNavigation"),
        QStringLiteral("autoFullScreen"),
    };
    return keys;
}

bool VaultConfig::isAppearanceKey(QStringView key)
{
    const auto &keys = appearanceKeys();
    for (const QString &k : keys) {
        if (k == key) return true;
    }
    return false;
}

QJsonObject VaultConfig::readConfig() const
{
    QJsonObject merged = readAppearanceJson().value_or(QJsonObject{});
    const QJsonObject app = readAppJson().value_or(QJsonObject{});
    for (auto it = app.begin(); it != app.end(); ++it) {
        merged.insert(it.key(), it.value());
    }
    return merged;
}

bool VaultConfig::saveConfig(const QJsonObject &merged) const
{
    QJsonObject app;
    QJsonObject appearance;
    for (auto it = merged.begin(); it != merged.end(); ++it) {
        if (isAppearanceKey(it.key())) {
            appearance.insert(it.key(), it.value());
        } else {
            app.insert(it.key(), it.value());
        }
    }
    // Both writes are unconditional (app/own/1129-UD.js:145-147): leaving a
    // stale copy behind in the other file is what makes Obsidian silently
    // revert a Corbomite-side change.
    //
    // Obsidian also awaits the two back to back and lets a rejection on the
    // second stand, so app.json holds the new values while appearance.json
    // keeps the old ones — a config split across two files with no record that
    // it is split, and configTs left un-advanced. Two paths cannot be renamed
    // in one step, so all-or-nothing has to come from the pre-write bytes:
    // serialise both payloads up front (so a serialisation failure precedes
    // any write), snapshot both, then put back the one that landed if the
    // other fails.
    if (!m_fs) return false;
    const QByteArray appBytes = serializeObsidianStyle(app);
    const QByteArray appearanceBytes = serializeObsidianStyle(appearance);
    const QString appPath = configFilePath(QStringLiteral("app.json"));
    const QString appearancePath = configFilePath(QStringLiteral("appearance.json"));
    const auto appBefore = m_fs->readBinary(appPath);
    if (!ensureConfigDir()) return false;
    if (!m_fs->writeBinary(appPath, appBytes)) return false;
    if (m_fs->writeBinary(appearancePath, appearanceBytes)) return true;
    if (appBefore) m_fs->writeBinary(appPath, *appBefore);
    else          m_fs->remove(appPath);
    return false;
}

bool VaultConfig::setConfigValue(const QString &key, const QJsonValue &value) const
{
    QJsonObject merged = readConfig();
    if (value.isUndefined()) {
        if (!merged.contains(key)) return true;
        merged.remove(key);
    } else {
        const auto it = merged.find(key);
        if (it != merged.end() && *it == value) return true;
        merged.insert(key, value);
    }
    return saveConfig(merged);
}

bool VaultConfig::removeConfigKey(const QString &key) const
{
    return setConfigValue(key, QJsonValue(QJsonValue::Undefined));
}

// --- Core-plugin data: `.obsidian/<corePluginId>.json` ---

bool VaultConfig::isSafeConfigName(const QString &name)
{
    if (name.isEmpty()) return false;
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))) return false;
    return !name.contains(QLatin1String(".."));
}

std::optional<QJsonObject> VaultConfig::readCorePluginConfig(const QString &corePluginId) const
{
    if (!isSafeConfigName(corePluginId)) return std::nullopt;
    return readJson(corePluginId + QStringLiteral(".json"));
}

bool VaultConfig::writeCorePluginConfig(const QString &corePluginId, const QJsonObject &obj)
{
    if (!isSafeConfigName(corePluginId)) return false;
    return writeJson(corePluginId + QStringLiteral(".json"), obj);
}

bool VaultConfig::deleteCorePluginConfig(const QString &corePluginId)
{
    if (!m_fs || !isSafeConfigName(corePluginId)) return false;
    return m_fs->remove(configFilePath(corePluginId + QStringLiteral(".json")));
}

// --- Value encodings ---

const QStringList &VaultConfig::openBehaviorModes()
{
    static const QStringList modes{
        QString(),            // last opened
        QStringLiteral("new"),
        QStringLiteral("file"),
        QStringLiteral("daily"),
    };
    return modes;
}

QString VaultConfig::openBehaviorPath(const QString &stored)
{
    static const QString prefix = QStringLiteral("file:");
    if (!stored.startsWith(prefix)) return {};
    return stored.mid(prefix.size());
}

QString VaultConfig::encodeOpenBehaviorFile(const QString &path)
{
    return QStringLiteral("file:") + path;
}

VaultConfig::AttachmentPath VaultConfig::decodeAttachmentFolderPath(const QString &stored)
{
    AttachmentPath out;
    if (stored == QLatin1String("/")) {
        out.location = AttachmentLocation::Root;
        return out;
    }
    static const QString subfolderPrefix = QStringLiteral("./");
    if (stored == QLatin1String(".") || stored == subfolderPrefix) {
        out.location = AttachmentLocation::CurrentFolder;
        return out;
    }
    if (stored.startsWith(subfolderPrefix)) {
        out.location = AttachmentLocation::Subfolder;
        out.value = stored.mid(subfolderPrefix.size());
        return out;
    }
    out.location = AttachmentLocation::Folder;
    out.value = stored;
    return out;
}

QString VaultConfig::encodeAttachmentFolderPath(const AttachmentPath &decoded,
                                                const QString &fallback)
{
    switch (decoded.location) {
    case AttachmentLocation::Root:
        return QStringLiteral("/");
    case AttachmentLocation::CurrentFolder:
        return QStringLiteral("./");
    case AttachmentLocation::Subfolder:
        return QStringLiteral("./")
            + (decoded.value.isEmpty() ? fallback : decoded.value);
    case AttachmentLocation::Folder:
        break;
    }
    return decoded.value.isEmpty() ? fallback : decoded.value;
}

QStringList VaultConfig::decodeFontFamilyStack(const QString &stored)
{
    QString normalised = stored;
    normalised.replace(QLatin1Char('"'), QString());
    QStringList families = normalised.split(QLatin1Char(','));
    for (QString &f : families) f = f.trimmed();
    families.removeAll(QString());
    return families;
}

QString VaultConfig::encodeFontFamilyStack(const QStringList &families)
{
    return families.join(QLatin1Char(','));
}

std::optional<bool> VaultConfig::nativeMenus() const
{
    const auto config = readConfig();
    const auto it = config.constFind(QStringLiteral("nativeMenus"));
    if (it == config.constEnd() || !it->isBool()) return std::nullopt;
    return it->toBool();
}

bool VaultConfig::setNativeMenus(std::optional<bool> followOs)
{
    // An explicit JSON `null` rather than a deleted key: it is what Obsidian's
    // defaults table carries (app/own/0920-eE.js:1203) and it keeps the third
    // state visible on disk.
    return setConfigValue(QStringLiteral("nativeMenus"),
                          followOs ? QJsonValue(*followOs) : QJsonValue(QJsonValue::Null));
}

QString VaultConfig::openBehavior() const
{
    const auto config = readConfig();
    const auto it = config.constFind(QStringLiteral("openBehavior"));
    return it != config.constEnd() && it->isString() ? it->toString() : QString();
}

VaultConfig::AttachmentPath VaultConfig::attachmentFolderPath() const
{
    const auto config = readConfig();
    const auto it = config.constFind(QStringLiteral("attachmentFolderPath"));
    // Obsidian's getConfig falls back to ME, whose attachmentFolderPath is
    // "/" — vault root (app/own/0920-eE.js:1204).
    return decodeAttachmentFolderPath(
        it != config.constEnd() && it->isString()
            ? it->toString() : QStringLiteral("/"));
}

QStringList VaultConfig::fontFamilyStack(const QString &key) const
{
    const auto config = readConfig();
    const auto it = config.constFind(key);
    return decodeFontFamilyStack(
        it != config.constEnd() && it->isString() ? it->toString() : QString());
}

std::optional<QStringList> VaultConfig::readCommunityPlugins() const
{
    if (!m_fs) return std::nullopt;
    const auto bytes = m_fs->readBinary(
        configFilePath(QStringLiteral("community-plugins.json")));
    if (!bytes.has_value()) return std::nullopt;
    const auto doc = QJsonDocument::fromJson(*bytes);
    if (!doc.isArray()) return std::nullopt;
    QStringList ids;
    for (const auto &v : doc.array()) {
        if (v.isString()) ids.append(v.toString());
    }
    return ids;
}

bool VaultConfig::writeCommunityPlugins(const QStringList &pluginIds) const
{
    if (!m_fs) return false;
    if (!ensureConfigDir()) return false;
    QJsonArray arr;
    for (const auto &id : pluginIds) arr.append(id);
    const QJsonDocument doc(arr);
    QByteArray out = doc.toJson(QJsonDocument::Indented);
    // Match Obsidian's 2-space indent + no trailing newline (same post-
    // processing as serializeObsidianStyle).
    QByteArray squeezed;
    squeezed.reserve(out.size());
    for (int i = 0; i < out.size(); ) {
        const bool atLineStart = (i == 0 || out[i - 1] == '\n');
        if (atLineStart && out[i] == ' ') {
            int spaces = 0;
            while (i + spaces < out.size() && out[i + spaces] == ' ') ++spaces;
            squeezed.append(QByteArray(spaces / 2, ' '));
            i += spaces;
        } else {
            squeezed.append(out[i]);
            ++i;
        }
    }
    while (squeezed.endsWith('\n')) squeezed.chop(1);
    return m_fs->writeBinary(
        configFilePath(QStringLiteral("community-plugins.json")), squeezed);
}

std::optional<QJsonObject> VaultConfig::readHotkeys() const
{
    return readJson(QStringLiteral("hotkeys.json"));
}

bool VaultConfig::writeHotkeys(const QJsonObject &obj) const
{
    return writeJson(QStringLiteral("hotkeys.json"), obj);
}

std::optional<QJsonObject> VaultConfig::readDailyNotesJson() const
{
    return readJson(QStringLiteral("daily-notes.json"));
}

bool VaultConfig::writeDailyNotesJson(const QJsonObject &obj) const
{
    return writeJson(QStringLiteral("daily-notes.json"), obj);
}

std::optional<QJsonObject> VaultConfig::readTemplatesJson() const
{
    return readJson(QStringLiteral("templates.json"));
}

bool VaultConfig::writeTemplatesJson(const QJsonObject &obj) const
{
    return writeJson(QStringLiteral("templates.json"), obj);
}

std::optional<VaultConfig::CorePlugins> VaultConfig::readCorePlugins() const
{
    if (!m_fs) return std::nullopt;
    const auto primary = m_fs->readBinary(
        configFilePath(QStringLiteral("core-plugins.json")));
    if (!primary.has_value()) return std::nullopt;

    const auto doc = QJsonDocument::fromJson(*primary);
    if (doc.isObject()) {
        CorePlugins cp;
        cp.raw = doc.object();
        return cp;
    }

    // Legacy array format — merge with core-plugins-migration.json, write
    // modern object-format back, delete the migration file.
    if (!doc.isArray()) return std::nullopt;

    CorePlugins cp;
    for (const auto &v : doc.array()) {
        if (v.isString()) cp.raw.insert(v.toString(), true);
    }

    const auto migrationBytes = m_fs->readBinary(
        configFilePath(QStringLiteral("core-plugins-migration.json")));
    if (migrationBytes.has_value()) {
        const auto migDoc = QJsonDocument::fromJson(*migrationBytes);
        if (migDoc.isObject()) {
            const auto migObj = migDoc.object();
            for (auto it = migObj.begin(); it != migObj.end(); ++it) {
                // Legacy migration shape: {id: {enabled: bool}} — coerce
                // to flat bool. Any unknown tail is preserved by mirroring
                // the enabled value.
                if (it.value().isObject()) {
                    const bool enabled = it.value().toObject()
                        .value(QStringLiteral("enabled")).toBool();
                    cp.raw.insert(it.key(), enabled);
                }
            }
        }
    }

    // Retire the migration file only once the modern write has landed: it is
    // the only remaining record of the intended state, and the legacy array it
    // is being merged into is presence-encoded (every id true), so deleting it
    // after a failed write silently re-enables every core plugin the migration
    // file carried.
    if (writeCorePlugins(cp) && migrationBytes.has_value()) {
        m_fs->remove(configFilePath(QStringLiteral("core-plugins-migration.json")));
    }
    return cp;
}

bool VaultConfig::writeCorePlugins(const CorePlugins &cp) const
{
    return writeJson(QStringLiteral("core-plugins.json"), cp.raw);
}

namespace {

/// Read a boolean from app.json, treating a non-boolean or absent value as
/// "not specified" so the caller's fallback wins. Obsidian writes real
/// booleans, but a hand-edited vault can contain anything.
bool appBool(const std::optional<QJsonObject> &app, const char *key,
             bool fallback)
{
    if (!app) return fallback;
    const auto it = app->find(QLatin1StringView(key));
    if (it == app->end() || !it->isBool()) return fallback;
    return it->toBool();
}

QString appString(const std::optional<QJsonObject> &app, const char *key)
{
    if (!app) return {};
    const auto it = app->find(QLatin1StringView(key));
    if (it == app->end() || !it->isString()) return {};
    return it->toString();
}

} // namespace

VaultConfig::TrashMode VaultConfig::appTrashMode(TrashMode fallback) const
{
    const QString v = appString(readAppJson(), "trashOption");
    // Unknown vocabulary falls back rather than guessing: silently escalating
    // to a permanent delete, or de-escalating to the system trash, would both
    // be worse than honouring the app-level setting the user can see.
    if (v == QLatin1String("system"))   return TrashMode::SystemTrash;
    if (v == QLatin1String("vault"))    return TrashMode::VaultTrash;
    if (v == QLatin1String("permanent"))return TrashMode::Permanent;
    return fallback;
}

bool VaultConfig::appPromptDelete(bool fallback) const
{
    return appBool(readAppJson(), "promptDelete", fallback);
}

bool VaultConfig::appAlwaysUpdateLinks(bool fallback) const
{
    return appBool(readAppJson(), "alwaysUpdateLinks", fallback);
}

bool VaultConfig::appUseMarkdownLinks(bool fallback) const
{
    return appBool(readAppJson(), "useMarkdownLinks", fallback);
}

QString VaultConfig::appNewLinkFormat() const
{
    return appString(readAppJson(), "newLinkFormat");
}

QStringList VaultConfig::userIgnoreFilters() const
{
    const auto app = readAppJson();
    if (!app.has_value()) return {};
    const auto v = app->value(QStringLiteral("userIgnoreFilters"));
    if (!v.isArray()) return {};
    QStringList filters;
    for (const auto &entry : v.toArray()) {
        if (entry.isString()) filters.append(entry.toString());
    }
    return filters;
}

bool VaultConfig::setUserIgnoreFilters(const QStringList &filters)
{
    if (filters.isEmpty()) {
        return setConfigValue(QStringLiteral("userIgnoreFilters"),
                              QJsonValue(QJsonValue::Null));
    }
    QJsonArray arr;
    for (const QString &f : filters) arr.append(f);
    return setConfigValue(QStringLiteral("userIgnoreFilters"), arr);
}

} // namespace Corbomite
