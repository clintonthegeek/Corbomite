// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"

#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/CachedMetadata.h"
#include "dialogs/DeleteConfirmDialog.h"
#include "dialogs/MoveFileDialog.h"
#include "dialogs/RenameDialog.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <markoff/parser/Document.h>
#include <markoff/parser/YamlValue.h>

#include <QFileInfo>
#include <QSet>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QWidget>

#include <algorithm>
#include <optional>

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

// Apply mutations from `map` onto the YamlValue tree in `keyOrder`, preserving
// unchanged keys. Missing keys in `map` are dropped; keys present in `map` but
// absent from `keyOrder` are skipped (callers must extend the order list).
// Iterating `keyOrder` rather than `map` directly is required because
// QVariantMap is sorted alphabetically, which would otherwise reshuffle every
// frontmatter write — see FileManager::processFrontMatter for the assembled
// order (original keys first, then new keys).
void applyVariantMapToYaml(const QVariantMap &map,
                           const QStringList &keyOrder,
                           Markoff::YamlValue &yaml)
{
    // Remove keys absent from the map.
    for (const QString &k : yaml.keys()) {
        if (!map.contains(k)) yaml.remove(k);
    }
    for (const QString &key : keyOrder) {
        auto it = map.constFind(key);
        if (it == map.cend()) continue;
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

// ---------------------------------------------------------------------------
// Surgical link rewriting for renameFile.
//
// Given the source slice of a complete link literal (`[[..]]`, `![[..]]`,
// `[..](..)`, `![..](..)`) that points to oldPath, build the equivalent
// literal pointing to newPath while preserving form (wiki vs markdown,
// embed vs not), alias text, and subpath.
//
// Returns an empty string when the form is unrecognised or the user-written
// target doesn't match a known oldPath shape — caller treats that as
// "skip this rewrite". Callers must verify the literal references the
// rename target before invoking; this function trusts that contract.
// ---------------------------------------------------------------------------

QString stripExtIfMd(const QString &p)
{
    return p.endsWith(QStringLiteral(".md"))
        ? p.left(p.size() - 3)
        : p;
}

QString basenameOf(const QString &p)
{
    const int slash = p.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? p : p.mid(slash + 1);
}

// Translate the user-written target text (without subpath/alias decoration
// — just the path/name part) for newPath. Returns empty if the form
// doesn't match any expected shape.
//
// URL-percent-decoding is intentionally NOT performed: a markdown URL like
// `[F](My%20File.md)` will fall through. Tracked as a follow-up; same
// pattern as Phase 5's deferred work in the original FileManager comments.
QString translateTarget(const QString &userTarget,
                        const QString &oldPath, const QString &newPath)
{
    const bool hadExt = userTarget.endsWith(QStringLiteral(".md"));
    const QString stem = stripExtIfMd(userTarget);

    const QString oldStem = stripExtIfMd(oldPath);
    const QString newStem = stripExtIfMd(newPath);
    const QString oldBase = basenameOf(oldStem);
    const QString newBase = basenameOf(newStem);

    QString translated;
    if (stem == oldBase) {
        // user wrote bare basename: "Foo"
        translated = newBase;
    } else if (stem == oldStem) {
        // user wrote full vault path: "folder/Foo"
        translated = newStem;
    } else if (stem.endsWith(QLatin1Char('/') + oldBase)) {
        // user wrote a partial path that disambiguates by basename
        // (e.g. "subfolder/Foo" where the resolver picked oldPath).
        // Replace just the basename portion.
        translated = stem.left(stem.size() - oldBase.size()) + newBase;
    } else {
        return QString();
    }

    return hadExt ? translated + QStringLiteral(".md") : translated;
}

// Take a complete link literal (caller-trusted to reference oldPath) and
// return its rewritten form. Returns empty on any unrecognised input.
QString rewriteLinkLiteral(const QString &literal,
                           const QString &oldPath, const QString &newPath)
{
    // Wiki / embed form: optional `!` + `[[ <inner> ]]`
    const QString embedPrefix = QStringLiteral("![[");
    const QString wikiPrefix  = QStringLiteral("[[");
    const QString wikiSuffix  = QStringLiteral("]]");

    auto rebuildWiki = [&](const QString &prefix, int innerStart) -> QString {
        if (!literal.endsWith(wikiSuffix)) return QString();
        const QString inner = literal.mid(innerStart,
            literal.size() - innerStart - wikiSuffix.size());

        // <target>[#<sub>][|<display>]
        QString target = inner;
        QString subAlias;
        const int hashIdx = target.indexOf(QLatin1Char('#'));
        const int pipeIdx = target.indexOf(QLatin1Char('|'));
        const int splitAt = (hashIdx < 0)
            ? pipeIdx
            : (pipeIdx < 0 ? hashIdx : std::min(hashIdx, pipeIdx));
        if (splitAt >= 0) {
            subAlias = target.mid(splitAt);
            target = target.left(splitAt);
        }
        const QString newTarget = translateTarget(target, oldPath, newPath);
        if (newTarget.isEmpty()) return QString();
        return prefix + newTarget + subAlias + wikiSuffix;
    };

    if (literal.startsWith(embedPrefix))
        return rebuildWiki(embedPrefix, embedPrefix.size());
    if (literal.startsWith(wikiPrefix))
        return rebuildWiki(wikiPrefix, wikiPrefix.size());

    // Markdown / image form: optional `!` + `[<label>](<url>)`
    auto rebuildMarkdown = [&](int labelStart) -> QString {
        // Walk to matching `]` at top level (no nesting expected for the
        // forms cache produces, but be defensive).
        const int labelEnd = literal.indexOf(QLatin1Char(']'), labelStart);
        if (labelEnd < 0) return QString();
        if (labelEnd + 1 >= literal.size()
            || literal.at(labelEnd + 1) != QLatin1Char('('))
            return QString();
        if (!literal.endsWith(QLatin1Char(')'))) return QString();

        const QString prefix = literal.left(labelStart); // "[" or "![" — actually the leading "[" or "!["
        const QString label  = literal.mid(labelStart, labelEnd - labelStart);
        const int urlStart = labelEnd + 2;
        const int urlEnd = literal.size() - 1;
        QString url = literal.mid(urlStart, urlEnd - urlStart);

        // Split out subpath fragment
        QString sub;
        const int hashIdx = url.indexOf(QLatin1Char('#'));
        if (hashIdx >= 0) {
            sub = url.mid(hashIdx);
            url = url.left(hashIdx);
        }

        const QString newUrl = translateTarget(url, oldPath, newPath);
        if (newUrl.isEmpty()) return QString();
        return prefix + label + QStringLiteral("](") + newUrl + sub
             + QStringLiteral(")");
    };

    if (literal.startsWith(QStringLiteral("![")))
        return rebuildMarkdown(2); // skip "!["
    if (literal.startsWith(QLatin1Char('[')))
        return rebuildMarkdown(1); // skip "["
    return QString();
}

// Strip the optional `#subpath` suffix from a resolved link path so it
// can be compared against a vault-relative file path.
QString linkPathWithoutSubpath(const QString &linkPath)
{
    const int hash = linkPath.indexOf(QLatin1Char('#'));
    return hash < 0 ? linkPath : linkPath.left(hash);
}

struct LinkRewrite
{
    int start = 0;
    int end = 0; // exclusive
    QString replacement;
};

// Apply rewrites to body. Sorts internally by start descending so each
// edit's offsets remain valid across application.
QByteArray applyRewrites(const QByteArray &body, QVector<LinkRewrite> rewrites)
{
    std::sort(rewrites.begin(), rewrites.end(),
              [](const LinkRewrite &a, const LinkRewrite &b) {
                  return a.start > b.start;
              });
    QString s = QString::fromUtf8(body);
    for (const LinkRewrite &r : rewrites) {
        if (r.start < 0 || r.end > s.size() || r.start > r.end) continue;
        s.replace(r.start, r.end - r.start, r.replacement);
    }
    return s.toUtf8();
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

        // Capture pre-mutation key order so the rebuilt frontmatter preserves
        // it. QVariantMap is sorted alphabetically, so the mutator-facing map
        // can't carry order on its own.
        const QStringList originalKeys = working.keys();

        QVariantMap map = yamlMapToVariantMap(working);
        mut(map);

        QStringList keyOrder;
        keyOrder.reserve(map.size());
        QSet<QString> placed;
        for (const QString &k : originalKeys) {
            if (map.contains(k) && !placed.contains(k)) {
                keyOrder.append(k);
                placed.insert(k);
            }
        }
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            if (!placed.contains(it.key())) {
                keyOrder.append(it.key());
                placed.insert(it.key());
            }
        }

        // Rebuild a fresh map to avoid partial-update state from `working`.
        Markoff::YamlValue next = Markoff::YamlValue::emptyMap();
        applyVariantMapToYaml(map, keyOrder, next);

        // If the mutator emptied the map, strip the frontmatter block
        // entirely. withFrontmatter(emptyMap) emits a `---\n\n---\n` shell;
        // Obsidian removes the fence when the value is empty — passing a
        // default-constructed (null) YamlValue takes the strip branch.
        const Markoff::YamlValue out =
            map.isEmpty() ? Markoff::YamlValue() : next;
        return doc->withFrontmatter(out).toUtf8();
    });
}

bool FileManager::setFrontMatter(TFile *f, const QList<FrontMatterEntry> &ordered)
{
    if (!f || !m_vault) return false;
    if (f->extension != QStringLiteral("md")) return false;

    return m_vault->process(f, [&](const QByteArray &cur) -> QByteArray {
        auto doc = Markoff::Document::fromMarkdown(QString::fromUtf8(cur));
        if (!doc) return cur;

        Markoff::YamlValue current = doc->parsedFrontmatter();
        Markoff::YamlValue working = current.isNull()
            ? Markoff::YamlValue::emptyMap()
            : current.clone();

        Markoff::YamlValue next = Markoff::YamlValue::emptyMap();
        for (const FrontMatterEntry &e : ordered) {
            if (e.preserveFromDisk) {
                if (working.contains(e.key))
                    next.setChildFrom(e.key, working.get(e.key));
                else
                    next.setNull(e.key);
                continue;
            }
            const QVariant &val = e.value;
            switch (val.typeId()) {
            case QMetaType::Bool:
                next.setBool(e.key, val.toBool()); break;
            case QMetaType::Int:
            case QMetaType::LongLong:
            case QMetaType::UInt:
            case QMetaType::ULongLong:
                next.setInt(e.key, val.toLongLong()); break;
            case QMetaType::Double:
            case QMetaType::Float:
                next.setDouble(e.key, val.toDouble()); break;
            case QMetaType::QStringList:
                next.setSeq(e.key, val.toStringList()); break;
            case QMetaType::QString:
                next.setString(e.key, val.toString()); break;
            default:
                if (!val.isValid() || val.isNull()) next.setNull(e.key);
                else next.setString(e.key, val.toString());
                break;
            }
        }

        const Markoff::YamlValue out =
            ordered.isEmpty() ? Markoff::YamlValue() : next;
        return doc->withFrontmatter(out).toUtf8();
    });
}

bool FileManager::renameFile(TAbstractFile *f, const QString &newPath)
{
    if (!f || !m_vault) return false;
    const QString rootOldPath = f->path;

    Q_EMIT renameStarted(f, newPath);

    // Build the per-file move list. For a single-file rename it's just one
    // entry. For a folder rename, expand to the folder itself plus every
    // descendant — Vault::rename(folder) moves all descendants in one shot,
    // but body-link rewriting needs to know each descendant's individual
    // old → new mapping so wikilinks like `[[Targets/Foo]]` get translated
    // to `[[Renamed/Foo]]` instead of being missed.
    struct Move {
        QString oldP;
        QString newP;
        QString oldBase;  // QFileInfo::completeBaseName of oldP
    };
    QVector<Move> moves;
    auto pushMove = [&](const QString &oldP, const QString &newP) {
        moves.push_back({oldP, newP, QFileInfo(oldP).completeBaseName()});
    };
    pushMove(rootOldPath, newPath);
    if (dynamic_cast<TFolder *>(f)) {
        const QString oldPrefix = rootOldPath + QLatin1Char('/');
        // Collect descendant paths from the cache (covers every file the
        // metadata layer knows about — same source of truth used by the
        // snapshot below). Folder rename also moves non-`.md` files but
        // those have no inbound link references to rewrite, so the cache
        // walk is sufficient for the link-rewrite phase.
        if (m_cache) {
            for (const QString &p : m_cache->allPaths()) {
                if (p.startsWith(oldPrefix)) {
                    pushMove(p, newPath + QLatin1Char('/') + p.mid(oldPrefix.size()));
                }
            }
        }
    }

    // Find the move that a given resolved link target (e.g. cache `lc.link`)
    // refers to. Returns nullptr when the target doesn't match any move.
    auto matchMove = [&](const QString &target) -> const Move * {
        if (target.isEmpty()) return nullptr;
        const int hash = target.indexOf(QLatin1Char('#'));
        const QString bare = hash >= 0 ? target.left(hash) : target;
        for (const Move &m : moves) {
            if (target == m.oldP || target == m.oldBase) return &m;
            if (bare == m.oldP || bare == m.oldBase) return &m;
        }
        return nullptr;
    };

    // Snapshot backlinks BEFORE the rename — MetadataCache loses these
    // entries when the rename reshapes the source-file index. Walk
    // allPaths() and bag every source whose links/embeds/frontmatterLinks
    // touch any move's old path. Self-references (a moved file linking to
    // another moved file) need rewriting too, so skipping by `src == oldPath`
    // is intentional only for the root of the rename — descendants are
    // both sources AND targets.
    QSet<QString> sourceSet;
    if (m_cache) {
        const QStringList all = m_cache->allPaths();
        for (const QString &src : all) {
            if (src == rootOldPath) continue;
            const auto cm = m_cache->getFileCache(src);
            if (!cm.has_value()) continue;
            bool refs = false;
            if (cm->links) {
                for (const auto &l : *cm->links) {
                    if (matchMove(l.link)) { refs = true; break; }
                }
            }
            if (!refs && cm->embeds) {
                for (const auto &l : *cm->embeds) {
                    if (matchMove(l.link)) { refs = true; break; }
                }
            }
            if (!refs && cm->frontmatterLinks) {
                for (const auto &fml : *cm->frontmatterLinks) {
                    if (matchMove(fml.link)) { refs = true; break; }
                }
            }
            if (refs) sourceSet.insert(src);
        }
    }
    QVector<QString> sources(sourceSet.begin(), sourceSet.end());

    // Single Vault::rename — for folders this also moves descendants
    // (m_fileMap entries get re-keyed and per-descendant `renamed` signals
    // fire).
    if (!m_vault->rename(f, newPath)) return false;

    int done = 0;
    const int total = sources.size();
    for (const QString &src : sources) {
        auto *sf = m_vault->getFileByPath(src);
        ++done;
        if (!sf) { Q_EMIT linkUpdateProgress(done, total); continue; }

        // Snapshot the cache once per source — drainOnePath has already run
        // by the time we got here (renameFile is invoked synchronously from
        // user input, well after indexFinished).
        const auto cm = m_cache ? m_cache->getFileCache(src) : std::optional<CachedMetadata>{};

        m_vault->process(sf, [&](const QByteArray &body) -> QByteArray {
            if (!cm.has_value()) return body;

            QVector<LinkRewrite> rewrites;
            const QString text = QString::fromUtf8(body);

            auto pushBodyRewrite = [&](const LinkCache &lc) {
                const Move *mv = matchMove(linkPathWithoutSubpath(lc.link));
                if (!mv) return;
                const int s = lc.position.start.offset;
                const int e = lc.position.end.offset;
                if (s < 0 || e > text.size() || s >= e) return;
                const QString slice = text.mid(s, e - s);
                const QString rebuilt = rewriteLinkLiteral(slice, mv->oldP, mv->newP);
                if (rebuilt.isEmpty()) return;
                rewrites.push_back({s, e, rebuilt});
            };

            if (cm->links) {
                for (const LinkCache &lc : *cm->links) pushBodyRewrite(lc);
            }
            if (cm->embeds) {
                for (const LinkCache &lc : *cm->embeds) pushBodyRewrite(lc);
            }

            // Frontmatter links: no source positions in cache, so search
            // each entry's `original` literal inside the frontmatter span,
            // advancing the search offset past each match so duplicates
            // resolve to distinct positions.
            if (cm->frontmatterLinks && cm->frontmatterPosition) {
                const int fmStart = cm->frontmatterPosition->start.offset;
                const int fmEnd = cm->frontmatterPosition->end.offset;
                int searchFrom = fmStart;
                for (const FrontmatterLinkCache &fml : *cm->frontmatterLinks) {
                    const Move *mv = matchMove(linkPathWithoutSubpath(fml.link));
                    if (!mv) {
                        // Even unaffected entries consume their span so the
                        // next search starts past them.
                        const int idx = text.indexOf(fml.original, searchFrom);
                        if (idx >= 0 && idx < fmEnd)
                            searchFrom = idx + static_cast<int>(fml.original.size());
                        continue;
                    }
                    const int idx = text.indexOf(fml.original, searchFrom);
                    if (idx < 0 || idx >= fmEnd) continue;
                    const QString rebuilt =
                        rewriteLinkLiteral(fml.original, mv->oldP, mv->newP);
                    if (rebuilt.isEmpty()) {
                        searchFrom = idx + static_cast<int>(fml.original.size());
                        continue;
                    }
                    const int matchEnd = idx + static_cast<int>(fml.original.size());
                    rewrites.push_back({idx, matchEnd, rebuilt});
                    searchFrom = matchEnd;
                }
            }

            return applyRewrites(body, rewrites);
        });
        Q_EMIT linkUpdateProgress(done, total);
    }

    Q_EMIT renameFinished(f, rootOldPath);
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

bool FileManager::promptForDeletion(TAbstractFile *file, QWidget *parent)
{
    if (!file || !m_vault) return false;

    const bool isFolder = dynamic_cast<TFolder *>(file) != nullptr;

    KConfigGroup files(KSharedConfig::openConfig(), QStringLiteral("Files"));
    const bool promptEnabled =
        files.readEntry(QStringLiteral("PromptDelete"), true);
    const QString trashOpt =
        files.readEntry(QStringLiteral("TrashOption"), QStringLiteral("system"));

    const auto performDelete = [&](Vault *v) -> bool {
        if (trashOpt == QStringLiteral("permanent"))
            return v->remove(file, /*recursive=*/isFolder);
        const bool useSystem = (trashOpt == QStringLiteral("system"));
        return v->trash(file, useSystem);
    };

    // Folders always prompt; files respect PromptDelete.
    if (!isFolder && !promptEnabled)
        return performDelete(m_vault);

    DeleteConfirmDialog dlg(file, m_vault, parent);
    if (dlg.exec() != QDialog::Accepted) return false;

    return performDelete(m_vault);
}

} // namespace Corbomite
