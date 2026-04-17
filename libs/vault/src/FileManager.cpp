// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"

#include <markoff-parser/Document.h>
#include <markoff-parser/YamlValue.h>

#include <QVariant>

namespace Corbomite {

FileManager::FileManager(Vault *vault, MetadataCache *cache, QObject *parent)
    : QObject(parent)
    , m_vault(vault)
    , m_cache(cache)
{
}

namespace {

// Convert a YamlValue (expected to be a map) into QVariantMap. Only the
// types PropertiesPanel + common frontmatter use are covered: bool / int /
// double / string / seq<scalar>. Nested maps are stringified to their YAML
// emission for lossless round-trip of the top-level mutation API. Plugin
// consumers that need richer YAML access should drop to Markoff::YamlValue
// directly via a FileManager follow-up.
QVariantMap yamlMapToVariantMap(const Markoff::YamlValue &yaml)
{
    QVariantMap out;
    if (!yaml.isMap()) return out;
    yaml.forEach([&](const QString &key, const Markoff::YamlValue &v) {
        switch (v.kind()) {
        case Markoff::YamlValue::Kind::Null:
            out.insert(key, QVariant());
            break;
        case Markoff::YamlValue::Kind::Bool:
            out.insert(key, v.asBool());
            break;
        case Markoff::YamlValue::Kind::Int:
            out.insert(key, static_cast<qlonglong>(v.asInt()));
            break;
        case Markoff::YamlValue::Kind::Double:
            out.insert(key, v.asDouble());
            break;
        case Markoff::YamlValue::Kind::String:
            out.insert(key, v.asString());
            break;
        case Markoff::YamlValue::Kind::Seq:
            out.insert(key, v.asStringList());
            break;
        case Markoff::YamlValue::Kind::Map:
            // Nested maps round-trip as their emitted YAML string. Mutating
            // this key replaces the subtree with a plain string — accepted
            // Phase-5 limitation per spec §11.
            out.insert(key, v.stringify());
            break;
        }
    });
    return out;
}

// Apply mutations from `map` onto the YamlValue tree, preserving unchanged
// keys. Missing keys in `map` are dropped; new keys in `map` are added.
void applyVariantMapToYaml(const QVariantMap &map, Markoff::YamlValue &yaml)
{
    // Remove keys absent from the map.
    for (const QString &k : yaml.keys()) {
        if (!map.contains(k)) yaml.remove(k);
    }
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        const QString &key = it.key();
        const QVariant &val = it.value();
        switch (val.typeId()) {
        case QMetaType::Bool:
            yaml.setBool(key, val.toBool());
            break;
        case QMetaType::Int:
        case QMetaType::LongLong:
        case QMetaType::UInt:
        case QMetaType::ULongLong:
            yaml.setInt(key, val.toLongLong());
            break;
        case QMetaType::Double:
        case QMetaType::Float:
            yaml.setDouble(key, val.toDouble());
            break;
        case QMetaType::QStringList:
            yaml.setSeq(key, val.toStringList());
            break;
        case QMetaType::QString:
            yaml.setString(key, val.toString());
            break;
        default:
            if (val.isNull() || !val.isValid()) {
                yaml.setNull(key);
            } else if (val.canConvert<QStringList>()) {
                yaml.setSeq(key, val.toStringList());
            } else if (val.canConvert<QString>()) {
                yaml.setString(key, val.toString());
            } else {
                yaml.setNull(key);
            }
            break;
        }
    }
}

} // namespace

bool FileManager::processFrontMatter(TFile *f, FrontMatterMutator mut)
{
    if (!f || !m_vault || !mut) return false;
    if (f->extension != QStringLiteral("md")) return false;

    return m_vault->process(f, [&](const QByteArray &cur) -> QByteArray {
        auto doc = Markoff::Document::fromMarkdown(QString::fromUtf8(cur));
        if (!doc) return cur;

        Markoff::YamlValue current = doc->parsedFrontmatter();
        Markoff::YamlValue working = current.isNull()
            ? Markoff::YamlValue::emptyMap()
            : current.clone();

        QVariantMap map = yamlMapToVariantMap(working);
        mut(map);
        // Rebuild a fresh map to avoid partial-update state from `working`.
        Markoff::YamlValue next = Markoff::YamlValue::emptyMap();
        applyVariantMapToYaml(map, next);

        return doc->withFrontmatter(next).toUtf8();
    });
}

bool FileManager::renameFile(TAbstractFile *, const QString &) { return false; }
bool FileManager::deleteProperty(const QString &) { return false; }
bool FileManager::renameProperty(const QString &, const QString &) { return false; }
TFolder *FileManager::getNewFileParent(const QString &, const QString &) const { return nullptr; }
TFile *FileManager::createNewMarkdownFile(TFolder *, const QString &, const QByteArray &) { return nullptr; }
TFile *FileManager::createNewMarkdownFileFromLinktext(const QString &, const QString &) { return nullptr; }
TFolder *FileManager::createNewFolder(TFolder *) { return nullptr; }
QString FileManager::getAvailablePathForAttachment(const QString &, const QString &) const { return {}; }
bool FileManager::insertIntoFile(TFile *, const QByteArray &, InsertMode) { return false; }
QString FileManager::generateMarkdownLink(TFile *, const QString &, const QString &, const QString &) const { return {}; }
bool FileManager::trashFile(TAbstractFile *f) { return m_vault && m_vault->trash(f, false); }

} // namespace Corbomite
