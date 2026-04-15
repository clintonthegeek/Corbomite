// SPDX-License-Identifier: GPL-3.0-or-later
#include "FoldingModel.h"
#include <QJsonArray>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcFolding, "markoff.folding")

namespace Markoff {

FoldingModel::FoldingModel(QObject *parent) : QObject(parent) {}

bool FoldingModel::isFolded(const FoldRegionKey &path) const {
    return m_folded.contains(path);
}

QList<FoldRegionKey> FoldingModel::foldedPaths() const {
    return QList<FoldRegionKey>(m_folded.begin(), m_folded.end());
}

QList<FoldRegionKey> FoldingModel::allPaths() const {
    QList<FoldRegionKey> r;
    r.reserve(m_headings.size());
    for (const auto &h : m_headings) r << h.path;
    return r;
}

bool FoldingModel::isHiddenByFold(const FoldRegionKey &path) const {
    // Any ancestor prefix (not the path itself) being folded hides it.
    // A heading hides its CHILDREN; the heading line itself stays visible.
    for (int i = 1; i < path.size(); ++i) {
        FoldRegionKey prefix = path.mid(0, i);
        if (m_folded.contains(prefix)) return true;
    }
    return false;
}

void FoldingModel::fold(const FoldRegionKey &path) {
    const bool inserted = !m_folded.contains(path);
    if (inserted) { m_folded.insert(path); emit foldStateChanged(); }
}

void FoldingModel::unfold(const FoldRegionKey &path) {
    const bool removed = m_folded.remove(path);
    if (removed) emit foldStateChanged();
}

void FoldingModel::toggle(const FoldRegionKey &path) {
    if (m_folded.contains(path)) unfold(path);
    else fold(path);
}

// --- stubs for later tasks ---
void FoldingModel::foldAll() {}
void FoldingModel::unfoldAll() {}
void FoldingModel::foldAllAtLevel(int) {}
void FoldingModel::unfoldAllAtLevel(int) {}
void FoldingModel::foldLevel(int) {}
void FoldingModel::unfoldLevel(int) {}
QJsonObject FoldingModel::serialize() const { return {}; }
void FoldingModel::restore(const QJsonObject &) {}
void FoldingModel::reconcile(const QList<HeadingInfo> &) {}
QList<FoldRegionKey> FoldingModel::unfoldAncestors(const FoldRegionKey &) { return {}; }

} // namespace Markoff
