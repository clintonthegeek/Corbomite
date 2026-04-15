// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_FOLDINGMODEL_H
#define MARKOFF_FOLDINGMODEL_H

#include <markoff/FoldingTypes.h>
#include <markoff-parser/Document.h>
#include <QObject>
#include <QSet>
#include <QList>
#include <QJsonObject>

namespace Markoff {

/// Owns the set of folded region paths. Pure data, no widgets. Fed by
/// `Editor` from the reparse signal; consulted by `SceneCoordinator`
/// for item visibility and by `FoldGutter` for paint.
class FoldingModel : public QObject {
    Q_OBJECT
public:
    // Existing HeadingEntry wrapper kept for v1 compat.
    struct HeadingEntry {
        FoldRegionKey path;
        HeadingInfo info;
    };

    explicit FoldingModel(QObject *parent = nullptr);

    bool isFolded(const FoldRegionKey &path) const;
    QList<FoldRegionKey> foldedPaths() const;
    QList<FoldRegionKey> allPaths() const;

    // --- v1-compatible views over m_regions ---
    QList<HeadingEntry> headings() const;
    QList<FoldableRegion> codeBlockRegions() const;
    const QList<FoldableRegion> &regions() const { return m_regions; }

    /// True if `path` is folded OR any of its ancestor prefixes is folded.
    /// Used to decide item visibility.
    bool isHiddenByFold(const FoldRegionKey &path) const;

    // --- Individual mutation ---
    void fold(const FoldRegionKey &path);
    void unfold(const FoldRegionKey &path);
    void toggle(const FoldRegionKey &path);

    // --- Bulk ops — operate on regions[] with type-aware predicates ---
    void foldAll();
    void unfoldAll();
    void foldAllAtLevel(int level);        // headings only
    void unfoldAllAtLevel(int level);      // headings only
    void foldLevel(int n);                 // headings only
    void unfoldLevel(int n);               // headings only

    // New code-block bulk ops:
    void foldAllCodeBlocks();
    void unfoldAllCodeBlocks();
    void foldAllCodeBlocksInSection(const FoldRegionKey &headingPath);
    void unfoldAllCodeBlocksInSection(const FoldRegionKey &headingPath);

    // --- Persistence ---
    QJsonObject serialize() const;
    void restore(const QJsonObject &);

    // reconcile NOW takes a precomputed region list (not a QList<HeadingInfo>).
    void reconcile(const QList<FoldableRegion> &newRegions);

    /// Walk `path` from root and unfold any folded prefix. Returns the
    /// prefixes actually unfolded (empty if none were folded). Used by
    /// auto-unfold on navigation and find.
    QList<FoldRegionKey> unfoldAncestors(const FoldRegionKey &path);

    // Test-only.
    void setRegionsForTesting(QList<FoldableRegion> r) { m_regions = std::move(r); }
    // Kept for v1-compat tests that predate the refactor.
    void setHeadingsForTesting(QList<HeadingEntry> h);

Q_SIGNALS:
    void foldStateChanged();

private:
    QSet<FoldRegionKey> m_folded;
    QList<FoldableRegion> m_regions;
};

} // namespace Markoff

#endif // MARKOFF_FOLDINGMODEL_H
