// SPDX-License-Identifier: GPL-3.0-or-later
#include "WikiLinkSuggest.h"

#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace {
QStringList aliasesFromFrontmatter(const QJsonObject &fm)
{
    QStringList out;
    for (const char *key : {"aliases", "alias"}) {
        const QJsonValue v = fm.value(QLatin1String(key));
        if (v.isString()) {
            out << v.toString();
        } else if (v.isArray()) {
            const QJsonArray arr = v.toArray();
            for (const QJsonValue &e : arr)
                if (e.isString()) out << e.toString();
        }
    }
    out.removeAll(QString());
    return out;
}
} // namespace

namespace Corbomite {

WikiLinkSuggest::WikiLinkSuggest(Vault *vault)
    : m_vault(vault)
{
}

std::optional<EditorSuggestTriggerInfo>
WikiLinkSuggest::onTrigger(int cursorPos, const QString &lineText, NoteDocument *file)
{
    m_sourcePath = file ? file->relativePath() : QString();
    // Walk back from the cursor (column-relative) looking for `[[`. Bail on
    // newline (caller passes a single line so this is implicit) or `]` that
    // would close the link.
    if (cursorPos < 0 || cursorPos > lineText.length()) return std::nullopt;
    int i = cursorPos - 1;
    while (i >= 1) {
        const QChar c = lineText.at(i);
        if (c == QLatin1Char(']')) return std::nullopt;
        if (c == QLatin1Char('[') && lineText.at(i - 1) == QLatin1Char('[')) {
            EditorSuggestTriggerInfo info;
            info.start = i + 1;     // after the second '['
            info.end = cursorPos;
            info.query = lineText.mid(info.start, info.end - info.start);
            // Consume a pre-existing "]]" right after the cursor so accepting
            // a candidate doesn't produce "]]]]" (spec §8).
            if (lineText.mid(cursorPos, 2) == QLatin1String("]]"))
                info.replaceEnd = cursorPos + 2;
            return info;
        }
        --i;
    }
    return std::nullopt;
}

EditorSuggestionSet WikiLinkSuggest::getSuggestions(const EditorSuggestTriggerInfo &ctx)
{
    EditorSuggestionSet set;
    set.filter = ctx.query;
    if (!m_vault) return set;

    // Sub-target modes: `target#headingQuery` / `target#^blockQuery`.
    const int hash = ctx.query.indexOf(QLatin1Char('#'));
    if (hash >= 0) {
        const QString target = ctx.query.left(hash);
        const QString sub = ctx.query.mid(hash + 1);
        if (sub.startsWith(QLatin1Char('^'))) {
            return blockSuggestions(target, sub.mid(1));   // A3 (returns empty until then)
        }
        return headingSuggestions(target, sub);
    }

    const auto files = m_vault->getMarkdownFiles();
    set.items.reserve(files.size());
    for (auto *tf : files) {
        if (!tf) continue;
        EditorSuggestItem item;
        item.display = tf->basename;
        // Shortest target that resolves uniquely: basename when unique
        // vault-wide, else the relative path (sans .md). LinkResolver's
        // name map is keyed by the lowercased filename WITH extension
        // (its basenameOf strips folders but keeps `.md`), which equals
        // TAbstractFile::name — NOT the extension-less TFile::basename.
        QString target = tf->basename;
        if (m_resolver && m_resolver->candidateCount(tf->name.toLower()) > 1) {
            target = tf->path;
            if (target.endsWith(QStringLiteral(".md"))) target.chop(3);
        }
        item.insertText = target + QStringLiteral("]]");
        item.detail = tf->path;
        set.items.append(item);

        // Aliases: frontmatter `aliases`/`alias` entries become candidates
        // that insert `target|alias]]` (the alias is the display text).
        if (m_cache) {
            if (const auto md = m_cache->getFileCache(tf->path)) {
                if (md->frontmatter) {
                    const QStringList aliases = aliasesFromFrontmatter(*md->frontmatter);
                    for (const QString &alias : aliases) {
                        EditorSuggestItem ai;
                        ai.display = alias;
                        ai.insertText = target + QStringLiteral("|") + alias + QStringLiteral("]]");
                        ai.detail = QStringLiteral("→ ") + tf->basename;
                        set.items.append(ai);
                    }
                }
            }
        }
    }
    return set;
}

EditorSuggestionSet WikiLinkSuggest::headingSuggestions(const QString &target,
                                                        const QString &sub)
{
    EditorSuggestionSet set;
    set.filter = sub;
    if (!m_resolver || !m_cache) return set;
    const ResolvedLink link = m_resolver->resolve(m_sourcePath, target);
    if (!link.resolved) return set;
    const auto md = m_cache->getFileCache(link.path);
    if (!md || !md->headings) return set;
    for (const auto &h : *md->headings) {
        EditorSuggestItem item;
        item.display = h.heading;
        item.insertText = target + QStringLiteral("#") + h.heading + QStringLiteral("]]");
        item.detail = link.path;
        set.items.append(item);
    }
    return set;
}

EditorSuggestionSet WikiLinkSuggest::blockSuggestions(const QString &target,
                                                      const QString &sub)
{
    EditorSuggestionSet set;
    set.filter = sub;
    return set;   // Task 16 (A3) fills this in.
}

} // namespace Corbomite
