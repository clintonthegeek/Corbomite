// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace Corbomite {

/// Outcome of resolving a wikilink against a vault.
struct ResolvedLink {
    QString path;     ///< Vault-relative path, e.g. "folder/Note.md". Empty if unresolved.
    QString subpath;  ///< "#heading" / "#^block" / "" — byte-passthrough of what parseLinktext captured.
    bool resolved = false;
};

/// Vault-wide wikilink resolver implementing Obsidian's 6-step
/// `getLinkpathDest` algorithm (see `docs/obsidian-audit/domains/metadata.md §8`).
///
/// Algorithm:
///   1. Empty linktext → source path (if present).
///   2. Case-insensitive basename lookup; append `.md` if no extension.
///   3. Exactly one candidate + linktext had a literal extension → that.
///   4. Relative `./` / `../` → resolve against source's parent, literal match.
///   5. Leading `/` → rooted absolute, exact match only; miss → empty.
///   6. Short-name disambiguation: partition into same-folder / other-folder,
///      sort each by shortest-path-wins (fewer separators, alpha tiebreak),
///      return first of `sameFolder + otherFolder`.
///
/// Not thread-safe: call `setVaultPaths` / `addVaultPath` / `removeVaultPath`
/// only from the indexing thread that constructed the resolver.
class LinkResolver
{
public:
    LinkResolver();

    /// Replace the full set of vault paths.
    void setVaultPaths(const QStringList &allPaths);

    /// Incrementally add/remove a path from the index.
    void addVaultPath(const QString &relativePath);
    void removeVaultPath(const QString &relativePath);

    /// Resolve `rawTarget` (e.g. `Note`, `Note.md`, `folder/Note#heading`,
    /// `./sibling`, `../up`, `/rooted`) against the folder context of
    /// `sourcePath` (vault-relative).
    ResolvedLink resolve(const QString &sourcePath, const QString &rawTarget) const;

    /// For tests / diagnostics.
    int candidateCount(const QString &basenameLower) const;

private:
    // basename.toLower() -> list of all vault-relative paths with that basename
    QHash<QString, QStringList> m_nameToPaths;

    // Full set of vault-relative paths, keyed by their lowercased form, for
    // rooted-absolute lookup. A multimap, not a map: two paths can share a
    // lowercased form (`B/Foo.md` / `b/foo.md`) and collapsing them would make
    // the survivor depend on insertion order. Resolution picks among them with
    // the same rule step 6 uses.
    QHash<QString, QStringList> m_exactLowerToActual;
};

} // namespace Corbomite
