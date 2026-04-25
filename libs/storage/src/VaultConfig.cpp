// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/VaultConfig.h"

#include "corbomite/storage/DataAdapter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

namespace Corbomite {

namespace {

constexpr auto kConfigDirName = "/.obsidian";

// Serialise a JSON object using Obsidian's exact format:
//   JSON.stringify(obj, undefined, 2) — 2-space indent, no trailing newline.
// Qt's QJsonDocument::Indented produces 4-space indent, so we post-process.
QByteArray serializeObsidianStyle(const QJsonObject &obj)
{
    // QJsonDocument::Indented emits 4-space indent; we squeeze to 2-space
    // by replacing leading runs of 4 spaces at line starts with 2 spaces.
    // This preserves key order (QJsonObject is insertion-ordered for the
    // keys it was built with — QJsonObject internally sorts alphabetically,
    // but Qt 6 preserves parse order via the document's internal ordering.
    // For our purposes, any consistent order is acceptable per VAULT-FORMAT
    // §1 "tolerate any key order on read").
    const QJsonDocument doc(obj);
    const QByteArray indented = doc.toJson(QJsonDocument::Indented);

    // Squeeze 4-space indents to 2-space.
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

QString joinConfigPath(const QString &vaultRoot, const QString &tail)
{
    QString base = vaultRoot;
    if (base.endsWith(QLatin1Char('/'))) base.chop(1);
    return base + QString::fromLatin1(kConfigDirName) + QLatin1Char('/') + tail;
}

} // namespace

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

    writeCorePlugins(cp);
    if (migrationBytes.has_value()) {
        m_fs->remove(configFilePath(QStringLiteral("core-plugins-migration.json")));
    }
    return cp;
}

bool VaultConfig::writeCorePlugins(const CorePlugins &cp) const
{
    return writeJson(QStringLiteral("core-plugins.json"), cp.raw);
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

} // namespace Corbomite
