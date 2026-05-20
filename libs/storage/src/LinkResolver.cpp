// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/LinkResolver.h"

#include <markoff/parser/LinkTextParser.h>

#include <QStringList>

#include <algorithm>

namespace Corbomite {

namespace {

QString basenameOf(const QString &relativePath)
{
    const int slash = relativePath.lastIndexOf(QLatin1Char('/'));
    return (slash < 0) ? relativePath : relativePath.mid(slash + 1);
}

QString folderOf(const QString &relativePath)
{
    const int slash = relativePath.lastIndexOf(QLatin1Char('/'));
    return (slash < 0) ? QString() : relativePath.left(slash);
}

int pathDepth(const QString &p)
{
    return p.count(QLatin1Char('/'));
}

// VL comparator: shortest path wins, alpha tiebreak on lowercased path.
bool pathShorterThan(const QString &a, const QString &b)
{
    const int da = pathDepth(a);
    const int db = pathDepth(b);
    if (da != db) return da < db;
    return a.toLower() < b.toLower();
}

// Resolve `./` / `../` against `sourceFolder`, returning the normalised
// vault-relative path. Empty string = couldn't resolve (went above root).
QString resolveRelative(const QString &sourceFolder, const QString &rel)
{
    QStringList parts = sourceFolder.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const QStringList segs = rel.split(QLatin1Char('/'), Qt::SkipEmptyParts);

    for (const QString &seg : segs) {
        if (seg == QStringLiteral(".") || seg.isEmpty()) continue;
        if (seg == QStringLiteral("..")) {
            if (parts.isEmpty()) return {};
            parts.removeLast();
            continue;
        }
        parts.append(seg);
    }
    return parts.join(QLatin1Char('/'));
}

} // namespace

LinkResolver::LinkResolver() = default;

void LinkResolver::setVaultPaths(const QStringList &allPaths)
{
    m_nameToPaths.clear();
    m_exactLowerToActual.clear();
    for (const QString &p : allPaths) {
        addVaultPath(p);
    }
}

void LinkResolver::addVaultPath(const QString &relativePath)
{
    if (relativePath.isEmpty()) return;
    const QString base = basenameOf(relativePath).toLower();
    QStringList &bucket = m_nameToPaths[base];
    if (!bucket.contains(relativePath)) {
        bucket.append(relativePath);
    }
    m_exactLowerToActual.insert(relativePath.toLower(), relativePath);
}

void LinkResolver::removeVaultPath(const QString &relativePath)
{
    if (relativePath.isEmpty()) return;
    const QString base = basenameOf(relativePath).toLower();
    auto it = m_nameToPaths.find(base);
    if (it != m_nameToPaths.end()) {
        it.value().removeAll(relativePath);
        if (it.value().isEmpty()) m_nameToPaths.erase(it);
    }
    m_exactLowerToActual.remove(relativePath.toLower());
}

int LinkResolver::candidateCount(const QString &basenameLower) const
{
    return m_nameToPaths.value(basenameLower).size();
}

ResolvedLink LinkResolver::resolve(const QString &sourcePath,
                                   const QString &rawTarget) const
{
    // Split off subpath first.
    const Markoff::LinkTarget split = Markoff::parseLinktext(rawTarget);
    QString linktext = split.path.trimmed();

    ResolvedLink r;
    r.subpath = split.subpath;

    // --- Step 1: Empty linktext + sourcePath → self-reference. ---
    if (linktext.isEmpty()) {
        if (!sourcePath.isEmpty()) {
            r.path = sourcePath;
            r.resolved = true;
        }
        return r;
    }

    // --- Step 5 pre-check: leading '/' = rooted absolute ---
    // Helper: append `.md` only when the *final segment* lacks an extension.
    // A whole-path `contains('.')` check would mis-fire for vaults with
    // dot-named folders like `2026.04/notes` — finding the dot in the
    // folder, skipping the `.md` append, and then missing in lookup.
    auto appendMdIfFinalSegmentHasNoExt = [](QString &p) {
        const int slash = p.lastIndexOf(QLatin1Char('/'));
        const QStringView tail = QStringView(p).mid(slash + 1);
        if (!tail.contains(QLatin1Char('.'))) p += QStringLiteral(".md");
    };

    if (linktext.startsWith(QLatin1Char('/'))) {
        QString rooted = linktext.mid(1);
        appendMdIfFinalSegmentHasNoExt(rooted);
        auto hit = m_exactLowerToActual.find(rooted.toLower());
        if (hit != m_exactLowerToActual.end()) {
            r.path = hit.value();
            r.resolved = true;
        }
        // Rooted miss returns empty (per audit §8.5 — "do not fall through").
        return r;
    }

    // --- Step 4: relative path ('./', '../', or any path containing '/') ---
    const bool hasSlash = linktext.contains(QLatin1Char('/'));
    const bool isDotRelative = linktext.startsWith(QStringLiteral("./"))
                            || linktext.startsWith(QStringLiteral("../"))
                            || linktext == QStringLiteral(".")
                            || linktext == QStringLiteral("..");

    if (isDotRelative || hasSlash) {
        const QString sourceFolder = folderOf(sourcePath);
        QString resolved = resolveRelative(sourceFolder, linktext);
        appendMdIfFinalSegmentHasNoExt(resolved);

        auto hit = m_exactLowerToActual.find(resolved.toLower());
        if (hit != m_exactLowerToActual.end()) {
            r.path = hit.value();
            r.resolved = true;
            return r;
        }
        // Fall through to step 6 for non-dot paths that look like folder/basename
        // but don't exactly match — Obsidian still attempts basename match on
        // the final segment. For dot-relative, a miss is final.
        if (isDotRelative) return r;
    }

    // --- Step 2: basename lookup ---
    QString basename = hasSlash ? basenameOf(linktext) : linktext;
    const bool linktextHasExt = basename.contains(QLatin1Char('.'));
    QString lookupKey = basename.toLower();
    if (!linktextHasExt) lookupKey += QStringLiteral(".md");

    const QStringList candidates = m_nameToPaths.value(lookupKey);
    if (candidates.isEmpty()) return r;

    // --- Step 3: exactly one candidate + literal extension match → that ---
    if (candidates.size() == 1 && linktextHasExt) {
        r.path = candidates.first();
        r.resolved = true;
        return r;
    }

    // --- Step 6: short-name disambiguation ---
    const QString sourceFolder = folderOf(sourcePath).toLower();
    const QString sourcePrefix = sourceFolder.isEmpty()
        ? QString()
        : (sourceFolder + QLatin1Char('/'));

    QStringList sameFolder;
    QStringList otherFolder;
    for (const QString &cand : candidates) {
        if (!sourcePrefix.isEmpty()
            && cand.toLower().startsWith(sourcePrefix)) {
            sameFolder.append(cand);
        } else if (sourcePrefix.isEmpty() && !cand.contains(QLatin1Char('/'))) {
            // Root source + root candidate → same-folder.
            sameFolder.append(cand);
        } else {
            otherFolder.append(cand);
        }
    }
    std::sort(sameFolder.begin(), sameFolder.end(), pathShorterThan);
    std::sort(otherFolder.begin(), otherFolder.end(), pathShorterThan);

    const QString winner = !sameFolder.isEmpty() ? sameFolder.first()
                                                 : otherFolder.first();
    r.path = winner;
    r.resolved = true;
    return r;
}

} // namespace Corbomite
