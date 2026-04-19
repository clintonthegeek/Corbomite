// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"

#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/CachedMetadata.h"
#include "dialogs/MoveFileDialog.h"
#include "dialogs/RenameDialog.h"

#include <markoff-parser/Document.h>
#include <markoff-parser/YamlValue.h>

#include <QFileInfo>
#include <QVariant>
#include <QWidget>

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

bool FileManager::renameFile(TAbstractFile *f, const QString &newPath)
{
    if (!f || !m_vault) return false;
    const QString oldPath = f->path;
    const QString oldBase = QFileInfo(oldPath).completeBaseName();
    const QString newBase = QFileInfo(newPath).completeBaseName();

    Q_EMIT renameStarted(f, newPath);

    // Snapshot backlinks BEFORE the rename — otherwise MetadataCache loses
    // the entry when the rename reshapes it. Walk allPaths() checking each
    // cache's links/embeds/frontmatterLinks for the old base name.
    QVector<QString> sources;
    if (m_cache) {
        const QStringList all = m_cache->allPaths();
        for (const QString &src : all) {
            if (src == oldPath) continue;
            const auto cm = m_cache->getFileCache(src);
            if (!cm.has_value()) continue;
            bool refs = false;
            auto matchesOld = [&](const QString &target) {
                if (target.isEmpty()) return false;
                if (target == oldPath) return true;
                if (target == oldBase) return true;
                const int hash = target.indexOf(QLatin1Char('#'));
                const QString bare = hash >= 0 ? target.left(hash) : target;
                return bare == oldBase || bare == oldPath;
            };
            if (cm->links) {
                for (const auto &l : *cm->links) {
                    if (matchesOld(l.link)) { refs = true; break; }
                }
            }
            if (!refs && cm->embeds) {
                for (const auto &l : *cm->embeds) {
                    if (matchesOld(l.link)) { refs = true; break; }
                }
            }
            if (refs) sources.append(src);
        }
    }

    if (!m_vault->rename(f, newPath)) return false;

    int done = 0;
    const int total = sources.size();
    for (const QString &src : sources) {
        auto *sf = m_vault->getFileByPath(src);
        ++done;
        if (!sf) { Q_EMIT linkUpdateProgress(done, total); continue; }
        m_vault->process(sf, [&](const QByteArray &body) -> QByteArray {
            QString s = QString::fromUtf8(body);
            // Phase 5 slice: [[oldBase]] → [[newBase]] and [[oldBase| → [[newBase|.
            // Subpath preservation + markdown-link rewrite + alias handling
            // land as follow-ups (see spec §11).
            s.replace(QStringLiteral("[[") + oldBase + QStringLiteral("]]"),
                      QStringLiteral("[[") + newBase + QStringLiteral("]]"));
            s.replace(QStringLiteral("[[") + oldBase + QStringLiteral("|"),
                      QStringLiteral("[[") + newBase + QStringLiteral("|"));
            s.replace(QStringLiteral("[[") + oldBase + QStringLiteral("#"),
                      QStringLiteral("[[") + newBase + QStringLiteral("#"));
            return s.toUtf8();
        });
        Q_EMIT linkUpdateProgress(done, total);
    }

    Q_EMIT renameFinished(f, oldPath);
    return true;
}
bool FileManager::deleteProperty(const QString &) { return false; }
bool FileManager::renameProperty(const QString &, const QString &) { return false; }
namespace {
QString collisionFreeName(Corbomite::Vault *v, const QString &parentPrefix,
                          const QString &desired, const QString &ext)
{
    const QString base = desired.isEmpty() ? QStringLiteral("Untitled") : desired;
    const QString suffix = ext.isEmpty() ? QString() : (QLatin1Char('.') + ext);
    QString candidate = parentPrefix + base + suffix;
    if (!v->getAbstractFileByPath(candidate)) return candidate;
    int n = 2;
    while (true) {
        candidate = parentPrefix + base + QStringLiteral(" ") + QString::number(n) + suffix;
        if (!v->getAbstractFileByPath(candidate)) return candidate;
        ++n;
    }
}
} // namespace

TFolder *FileManager::getNewFileParent(const QString &hintPath,
                                       const QString &) const
{
    if (!m_vault) return nullptr;
    if (hintPath.isEmpty()) return m_vault->getRoot();
    const int slash = hintPath.lastIndexOf(QLatin1Char('/'));
    if (slash <= 0) return m_vault->getRoot();
    if (auto *p = m_vault->getFolderByPath(hintPath.left(slash))) return p;
    return m_vault->getRoot();
}

TFile *FileManager::createNewMarkdownFile(TFolder *parent,
                                          const QString &name,
                                          const QByteArray &content)
{
    if (!m_vault) return nullptr;
    if (!parent) parent = m_vault->getRoot();
    const QString prefix = parent->getParentPrefix();
    const QString path = collisionFreeName(m_vault, prefix, name, QStringLiteral("md"));
    return m_vault->create(path, content);
}

TFile *FileManager::createNewMarkdownFileFromLinktext(const QString &linkText,
                                                      const QString &hintPath)
{
    TFolder *parent = getNewFileParent(hintPath);
    return createNewMarkdownFile(parent, linkText);
}

TFolder *FileManager::createNewFolder(TFolder *parent)
{
    if (!m_vault) return nullptr;
    if (!parent) parent = m_vault->getRoot();
    const QString prefix = parent->getParentPrefix();
    const QString path = collisionFreeName(m_vault, prefix,
                                           QStringLiteral("untitled folder"),
                                           QString());
    return m_vault->createFolder(path);
}

QString FileManager::getAvailablePathForAttachment(const QString &linktext,
                                                   const QString &sourcePathHint) const
{
    if (!m_vault) return {};
    // Phase 5 slice: attachments go in the same folder as the source file
    // (Obsidian's default attachmentFolderPath = "." behaviour). Full config
    // honouring (attachmentFolderPath with "./sub" or absolute paths) is a
    // follow-up — see spec §11.
    QString parentPrefix;
    if (!sourcePathHint.isEmpty()) {
        const int slash = sourcePathHint.lastIndexOf(QLatin1Char('/'));
        if (slash > 0) parentPrefix = sourcePathHint.left(slash) + QLatin1Char('/');
    }

    const QFileInfo fi(linktext);
    const QString base = fi.completeBaseName();
    const QString ext  = fi.suffix();
    return collisionFreeName(m_vault, parentPrefix, base, ext);
}

bool FileManager::insertIntoFile(TFile *f, const QByteArray &content, InsertMode mode)
{
    if (!f || !m_vault) return false;
    return m_vault->process(f, [&](const QByteArray &cur) -> QByteArray {
        // Frontmatter-aware merge is scope-deferred (spec §11); Phase 5
        // ships plain append/prepend.
        return mode == InsertMode::Append ? (cur + content) : (content + cur);
    });
}

QString FileManager::generateMarkdownLink(TFile *target,
                                          const QString &sourcePath,
                                          const QString &subpath,
                                          const QString &displayText) const
{
    if (!target) return {};
    // Phase 5 slice: emit shortest unique wiki-link (basename for .md files,
    // full relative path for others). `useMarkdownLinks` + `newLinkFormat`
    // config honouring is a follow-up once PropertiesPanel or similar
    // surfaces it — spec §11.
    Q_UNUSED(sourcePath);
    const QString core = target->extension == QStringLiteral("md")
                       ? target->basename : target->path;
    QString out = QStringLiteral("[[") + core;
    if (!subpath.isEmpty()) out += subpath;   // "#heading" / "^blockid"
    if (!displayText.isEmpty()) out += QLatin1Char('|') + displayText;
    out += QStringLiteral("]]");
    return out;
}
bool FileManager::trashFile(TAbstractFile *f) { return m_vault && m_vault->trash(f, false); }

TFile *FileManager::createMarkdownNote(const QString &name, const QString &folder)
{
    if (!m_vault) return nullptr;

    TFolder *parent = m_vault->getRoot();
    if (!folder.isEmpty()) {
        if (auto *existing = m_vault->getFolderByPath(folder)) {
            parent = existing;
        } else if (auto *created = m_vault->createFolder(folder)) {
            parent = created;
        }
    }
    return createNewMarkdownFile(parent, name);
}

bool FileManager::renameFileByPath(const QString &oldRel, const QString &newRel)
{
    if (!m_vault) return false;
    TAbstractFile *f = m_vault->getAbstractFileByPath(oldRel);
    if (!f) return false;
    return renameFile(f, newRel);
}

bool FileManager::trashFileByPath(const QString &relPath)
{
    if (!m_vault) return false;
    TAbstractFile *f = m_vault->getAbstractFileByPath(relPath);
    if (!f) return false;
    return trashFile(f);
}

QString FileManager::promptForFileRename(TAbstractFile *file, QWidget *parent)
{
    if (!file || !m_vault) return QString();

    RenameDialog dlg(file, m_vault, parent);
    if (dlg.exec() != QDialog::Accepted) return QString();

    const QString newName = dlg.proposedNewName();
    if (newName.isEmpty() || newName == file->name) return QString();

    // Compute the new full vault-relative path. TFolder::getParentPrefix
    // returns "" for the root folder and "<folderPath>/" for nested
    // folders, matching what Vault::rename expects as newPath.
    const QString parentPrefix =
        file->parent ? file->parent->getParentPrefix() : QString();
    const QString newPath = parentPrefix + newName;

    // Delegate to renameFile — this is the link-rewrite aware path.
    const bool ok = renameFile(file, newPath);
    return ok ? newPath : QString();
}

QString FileManager::promptForMove(TAbstractFile *file, QWidget *parent)
{
    if (!file || !m_vault) return QString();

    MoveFileDialog dlg(file, m_vault, parent);
    if (dlg.exec() != QDialog::Accepted) return QString();

    QString folderPath = dlg.selectedFolderPath();
    if (folderPath.isEmpty()) return QString();

    // Normalise root ("/") → empty so the prefix concat below produces a
    // plain root-relative path.
    if (folderPath == QStringLiteral("/")) folderPath.clear();

    const QString newPath = folderPath.isEmpty()
        ? file->name
        : folderPath + QStringLiteral("/") + file->name;

    // Collision check: target folder already has a file by this name?
    // UX follow-up will surface a Notice; for now quietly abort.
    if (m_vault->getAbstractFileByPath(newPath) != nullptr)
        return QString();

    const bool ok = renameFile(file, newPath);
    return ok ? newPath : QString();
}

} // namespace Corbomite
