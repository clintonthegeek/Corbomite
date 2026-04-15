// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/PaneLayout.h"

#include <QJsonArray>
#include <QUuid>

namespace Corbomite {

namespace {

QString makeId()
{
    // 16-char lowercase hex — matches Obsidian's 16-char random token.
    return QUuid::createUuid().toString(QUuid::WithoutBraces).remove(
        QLatin1Char('-')).left(16);
}

// --- JSON helpers ---

constexpr auto kType = "type";
constexpr auto kTypeSplit = "split";
constexpr auto kTypeTabs = "tabs";
constexpr auto kTypeLeaf = "leaf";

constexpr auto kId = "id";
constexpr auto kChildren = "children";
constexpr auto kDirection = "direction";
constexpr auto kDirHorizontal = "horizontal";
constexpr auto kDirVertical = "vertical";
constexpr auto kDimension = "dimension";
constexpr auto kCurrentTab = "currentTab";
constexpr auto kStacked = "stacked";
constexpr auto kState = "state";
constexpr auto kPinned = "pinned";
constexpr auto kGroup = "group";

QJsonObject objectExcept(const QJsonObject &in, const QStringList &keys)
{
    QJsonObject out;
    for (auto it = in.begin(); it != in.end(); ++it) {
        if (!keys.contains(it.key())) out.insert(it.key(), it.value());
    }
    return out;
}

} // namespace

// ============================================================================
// PaneLayoutIndex
// ============================================================================

PaneLayoutIndex::PaneLayoutIndex() = default;

PaneLayoutIndex::PaneLayoutIndex(PaneLayoutIndex *parent)
    : m_parent(parent)
{
}

PaneLayoutIndex::~PaneLayoutIndex() = default;

bool PaneLayoutIndex::isSplit() const
{
    return m_first || m_second;
}

PaneLayoutIndex *PaneLayoutIndex::parent() const { return m_parent; }
PaneLayoutIndex *PaneLayoutIndex::first() const { return m_first.get(); }
PaneLayoutIndex *PaneLayoutIndex::second() const { return m_second.get(); }

Qt::Orientation PaneLayoutIndex::orientation() const { return m_orientation; }
void PaneLayoutIndex::setOrientation(Qt::Orientation o) { m_orientation = o; }

std::optional<double> PaneLayoutIndex::dimension() const { return m_dimension; }
void PaneLayoutIndex::setDimension(std::optional<double> d) { m_dimension = d; }

QString PaneLayoutIndex::indexId() const
{
    if (m_indexId.isEmpty()) m_indexId = makeId();
    return m_indexId;
}

void PaneLayoutIndex::setIndexId(const QString &id) { m_indexId = id; }

QJsonObject PaneLayoutIndex::unknownKeys() const { return m_unknown; }
void PaneLayoutIndex::setUnknownKeys(const QJsonObject &o) { m_unknown = o; }

const QList<PaneLeaf> &PaneLayoutIndex::views() const { return m_views; }
int PaneLayoutIndex::viewCount() const { return m_views.size(); }

const PaneLeaf *PaneLayoutIndex::viewAt(int pos) const
{
    return (pos >= 0 && pos < m_views.size()) ? &m_views[pos] : nullptr;
}

PaneLeaf *PaneLayoutIndex::viewAt(int pos)
{
    return (pos >= 0 && pos < m_views.size()) ? &m_views[pos] : nullptr;
}

int PaneLayoutIndex::currentTab() const { return m_currentTab; }
void PaneLayoutIndex::setCurrentTab(int t) { m_currentTab = t; }

bool PaneLayoutIndex::stacked() const { return m_stacked; }
void PaneLayoutIndex::setStacked(bool s) { m_stacked = s; }

void PaneLayoutIndex::addView(PaneLeaf leaf, int afterIndex)
{
    // Invariant from Sublime: no views on split nodes.
    if (isSplit()) return;
    if (leaf.id.isEmpty()) leaf.id = makeId();
    if (afterIndex < 0 || afterIndex >= m_views.size()) {
        m_views.append(std::move(leaf));
    } else {
        m_views.insert(afterIndex + 1, std::move(leaf));
    }
}

void PaneLayoutIndex::removeView(const QString &leafId)
{
    if (isSplit()) return;
    for (int i = 0; i < m_views.size(); ++i) {
        if (m_views[i].id == leafId) {
            m_views.removeAt(i);
            break;
        }
    }
    if (m_parent && m_views.isEmpty()) {
        m_parent->unsplit(this);
    }
}

void PaneLayoutIndex::moveViewPosition(const QString &leafId, int newPos)
{
    for (int i = 0; i < m_views.size(); ++i) {
        if (m_views[i].id == leafId) {
            if (newPos < 0) newPos = 0;
            if (newPos > m_views.size() - 1) newPos = m_views.size() - 1;
            m_views.move(i, newPos);
            return;
        }
    }
}

void PaneLayoutIndex::split(Qt::Orientation dir, bool moveViewsToSecond)
{
    if (isSplit()) return;
    m_first = std::unique_ptr<PaneLayoutIndex>(new PaneLayoutIndex(this));
    m_second = std::unique_ptr<PaneLayoutIndex>(new PaneLayoutIndex(this));
    m_orientation = dir;
    if (moveViewsToSecond) moveViewsTo(m_second.get());
    else                   moveViewsTo(m_first.get());
}

void PaneLayoutIndex::splitWithNewLeaf(PaneLeaf newLeaf, Qt::Orientation dir)
{
    split(dir);
    m_second->addView(std::move(newLeaf));
}

void PaneLayoutIndex::unsplit(PaneLayoutIndex *childToRemove)
{
    if (!isSplit()) return;

    // Determine surviving sibling.
    std::unique_ptr<PaneLayoutIndex> survivor;
    std::unique_ptr<PaneLayoutIndex> goner;
    if (m_first.get() == childToRemove) {
        goner = std::move(m_first);
        survivor = std::move(m_second);
    } else if (m_second.get() == childToRemove) {
        goner = std::move(m_second);
        survivor = std::move(m_first);
    } else {
        return; // not our child
    }

    // Move survivor's contents up into this node. The Sublime pattern
    // copies views, orientation, children, then deletes both child nodes.
    survivor->moveViewsTo(this);
    m_orientation = survivor->orientation();
    m_currentTab = survivor->currentTab();
    m_stacked = survivor->stacked();
    // Copy the survivor's children (if any) up.
    m_first = std::move(survivor->m_first);
    m_second = std::move(survivor->m_second);
    if (m_first) m_first->setParent(this);
    if (m_second) m_second->setParent(this);

    // survivor and goner go out of scope here and delete cleanly.
}

void PaneLayoutIndex::walk(const std::function<bool(PaneLayoutIndex *)> &visitor)
{
    if (!visitor) return;
    if (!visitor(this)) return;
    if (m_first) m_first->walk(visitor);
    if (m_second) m_second->walk(visitor);
}

void PaneLayoutIndex::walk(const std::function<bool(const PaneLayoutIndex *)> &visitor) const
{
    if (!visitor) return;
    if (!visitor(this)) return;
    if (m_first) m_first->walk(visitor);
    if (m_second) m_second->walk(visitor);
}

void PaneLayoutIndex::setParent(PaneLayoutIndex *p) { m_parent = p; }

void PaneLayoutIndex::setFirst(std::unique_ptr<PaneLayoutIndex> child)
{
    m_first = std::move(child);
    if (m_first) m_first->setParent(this);
}

void PaneLayoutIndex::setSecond(std::unique_ptr<PaneLayoutIndex> child)
{
    m_second = std::move(child);
    if (m_second) m_second->setParent(this);
}

void PaneLayoutIndex::setViews(QList<PaneLeaf> views)
{
    m_views = std::move(views);
}

void PaneLayoutIndex::adoptFrom(PaneLayoutIndex *source)
{
    if (!source) return;
    m_views = std::move(source->m_views);
    m_currentTab = source->m_currentTab;
    m_stacked = source->m_stacked;
    m_orientation = source->m_orientation;
    m_first = std::move(source->m_first);
    m_second = std::move(source->m_second);
    if (m_first) m_first->setParent(this);
    if (m_second) m_second->setParent(this);
}

void PaneLayoutIndex::moveViewsTo(PaneLayoutIndex *target)
{
    target->m_views = std::move(m_views);
    m_views.clear();
    target->m_currentTab = m_currentTab;
    target->m_stacked = m_stacked;
}

void PaneLayoutIndex::copyChildrenTo(PaneLayoutIndex *target)
{
    if (!m_first || !m_second) return;
    target->m_first = std::move(m_first);
    target->m_second = std::move(m_second);
    if (target->m_first) target->m_first->setParent(target);
    if (target->m_second) target->m_second->setParent(target);
}

// ============================================================================
// PaneLayout JSON round-trip
// ============================================================================

PaneLayout::PaneLayout()
    : m_root(std::unique_ptr<PaneLayoutIndex>(new PaneLayoutIndex()))
{
}

PaneLayout::~PaneLayout() = default;
PaneLayout::PaneLayout(PaneLayout &&) noexcept = default;
PaneLayout &PaneLayout::operator=(PaneLayout &&) noexcept = default;

PaneLayoutIndex *PaneLayout::root() { return m_root.get(); }
const PaneLayoutIndex *PaneLayout::root() const { return m_root.get(); }

QString PaneLayout::activeLeafId() const { return m_activeLeafId; }
void PaneLayout::setActiveLeafId(const QString &id) { m_activeLeafId = id; }

PaneLeaf *PaneLayout::findLeaf(const QString &id)
{
    PaneLeaf *hit = nullptr;
    m_root->walk([&](PaneLayoutIndex *node) {
        for (int i = 0; i < node->viewCount(); ++i) {
            if (node->viewAt(i)->id == id) {
                hit = node->viewAt(i);
                return false;
            }
        }
        return true;
    });
    return hit;
}

const PaneLeaf *PaneLayout::findLeaf(const QString &id) const
{
    return const_cast<PaneLayout *>(this)->findLeaf(id);
}

QString PaneLayout::newId() { return makeId(); }

// --- fromJson ---

namespace {

// Forward decl.
std::unique_ptr<PaneLayoutIndex> buildIndex(const QJsonObject &node,
                                            PaneLayoutIndex *parent);

PaneLeaf buildLeaf(const QJsonObject &leafObj)
{
    PaneLeaf leaf;
    leaf.id = leafObj.value(QLatin1String(kId)).toString();
    leaf.pinned = leafObj.value(QLatin1String(kPinned)).toBool();
    leaf.group = leafObj.value(QLatin1String(kGroup)).toString();
    leaf.viewState = leafObj.value(QLatin1String(kState)).toObject();
    leaf.viewType = leaf.viewState.value(QLatin1String(kType)).toString();

    const auto innerState = leaf.viewState.value(QLatin1String(kState)).toObject();
    leaf.filePath = innerState.value(QStringLiteral("file")).toString();
    leaf.mode = innerState.value(QStringLiteral("mode")).toString();

    leaf.unknown = objectExcept(leafObj, {
        QLatin1String(kId), QLatin1String(kType), QLatin1String(kState),
        QLatin1String(kPinned), QLatin1String(kGroup),
    });
    return leaf;
}

std::unique_ptr<PaneLayoutIndex> buildTabsIndex(const QJsonObject &tabsObj,
                                                PaneLayoutIndex *parent)
{
    auto idx = std::make_unique<PaneLayoutIndex>(parent);
    idx->setIndexId(tabsObj.value(QLatin1String(kId)).toString());
    idx->setCurrentTab(tabsObj.value(QLatin1String(kCurrentTab)).toInt(0));
    idx->setStacked(tabsObj.value(QLatin1String(kStacked)).toBool(false));
    if (tabsObj.contains(QLatin1String(kDimension))) {
        idx->setDimension(tabsObj.value(QLatin1String(kDimension)).toDouble());
    }
    const auto kids = tabsObj.value(QLatin1String(kChildren)).toArray();
    for (const auto &c : kids) {
        if (!c.isObject()) continue;
        const auto childObj = c.toObject();
        if (childObj.value(QLatin1String(kType)).toString()
                == QLatin1String(kTypeLeaf)) {
            idx->addView(buildLeaf(childObj));
        }
        // Unknown-type children (future Obsidian): skip silently.
    }
    idx->setUnknownKeys(objectExcept(tabsObj, {
        QLatin1String(kType), QLatin1String(kId),
        QLatin1String(kChildren), QLatin1String(kCurrentTab),
        QLatin1String(kStacked), QLatin1String(kDimension),
    }));
    return idx;
}

std::unique_ptr<PaneLayoutIndex> buildLeafAsTabsIndex(const QJsonObject &leafObj,
                                                     PaneLayoutIndex *parent)
{
    // A raw `leaf` node at the tree level is modelled as a single-view index.
    auto idx = std::make_unique<PaneLayoutIndex>(parent);
    if (leafObj.contains(QLatin1String(kDimension))) {
        idx->setDimension(leafObj.value(QLatin1String(kDimension)).toDouble());
    }
    idx->addView(buildLeaf(leafObj));
    return idx;
}

std::unique_ptr<PaneLayoutIndex> buildSplitIndex(const QJsonObject &splitObj,
                                                 PaneLayoutIndex *parent)
{
    auto idx = std::make_unique<PaneLayoutIndex>(parent);
    idx->setIndexId(splitObj.value(QLatin1String(kId)).toString());
    const QString dir = splitObj.value(QLatin1String(kDirection)).toString();
    idx->setOrientation(dir == QLatin1String(kDirVertical)
                         ? Qt::Vertical : Qt::Horizontal);
    if (splitObj.contains(QLatin1String(kDimension))) {
        idx->setDimension(splitObj.value(QLatin1String(kDimension)).toDouble());
    }

    const auto kids = splitObj.value(QLatin1String(kChildren)).toArray();
    // Collect valid child indices; splits in Obsidian are strictly binary,
    // but we tolerate N children by chain-splitting (recovers gracefully
    // from an exotic input while preserving structure best-effort).
    std::vector<std::unique_ptr<PaneLayoutIndex>> built;
    for (const auto &c : kids) {
        if (!c.isObject()) continue;
        auto child = buildIndex(c.toObject(), idx.get());
        if (child) built.push_back(std::move(child));
    }

    if (built.empty()) {
        // No valid children; leave as empty leaf index.
    } else if (built.size() == 1) {
        // Single child — collapse it into this index.
        idx->adoptFrom(built[0].get());
    } else {
        // Binary or N-ary: keep first two directly; if more, wrap extras
        // into nested splits of the same orientation on the second child.
        idx->setFirst(std::move(built[0]));

        if (built.size() == 2) {
            idx->setSecond(std::move(built[1]));
        } else {
            auto chain = std::make_unique<PaneLayoutIndex>(idx.get());
            chain->setOrientation(idx->orientation());
            chain->setFirst(std::move(built[1]));

            auto *cursor = chain.get();
            for (size_t i = 2; i + 1 < built.size(); ++i) {
                auto next = std::make_unique<PaneLayoutIndex>(cursor);
                next->setOrientation(idx->orientation());
                next->setFirst(std::move(built[i]));
                auto *nextRaw = next.get();
                cursor->setSecond(std::move(next));
                cursor = nextRaw;
            }
            cursor->setSecond(std::move(built.back()));

            idx->setSecond(std::move(chain));
        }
    }

    idx->setUnknownKeys(objectExcept(splitObj, {
        QLatin1String(kType), QLatin1String(kId),
        QLatin1String(kDirection), QLatin1String(kDimension),
        QLatin1String(kChildren),
    }));
    return idx;
}

std::unique_ptr<PaneLayoutIndex> buildIndex(const QJsonObject &node,
                                            PaneLayoutIndex *parent)
{
    const QString type = node.value(QLatin1String(kType)).toString();
    if (type == QLatin1String(kTypeSplit))  return buildSplitIndex(node, parent);
    if (type == QLatin1String(kTypeTabs))   return buildTabsIndex(node, parent);
    if (type == QLatin1String(kTypeLeaf))   return buildLeafAsTabsIndex(node, parent);
    // Unknown type (floating / window / mobile-drawer / future) → skip.
    return nullptr;
}

} // namespace

PaneLayout PaneLayout::fromJson(const QJsonObject &splitNode)
{
    PaneLayout out;
    auto built = buildIndex(splitNode, nullptr);
    if (built) {
        out.m_root = std::move(built);
    }
    return out;
}

// --- toJson ---

namespace {

QJsonObject emitLeafNode(const PaneLeaf &leaf)
{
    QJsonObject obj = leaf.unknown; // start with preserved unknowns
    obj.insert(QLatin1String(kId),
               leaf.id.isEmpty() ? makeId() : leaf.id);
    obj.insert(QLatin1String(kType), QLatin1String(kTypeLeaf));
    QJsonObject vs = leaf.viewState;
    // Backfill viewType/file/mode into viewState if the caller populated
    // the convenience fields but left viewState empty.
    if (!vs.contains(QLatin1String(kType)) && !leaf.viewType.isEmpty()) {
        vs.insert(QLatin1String(kType), leaf.viewType);
    }
    if (!leaf.filePath.isEmpty() || !leaf.mode.isEmpty()) {
        QJsonObject inner = vs.value(QLatin1String(kState)).toObject();
        if (!leaf.filePath.isEmpty()
                && !inner.contains(QStringLiteral("file"))) {
            inner.insert(QStringLiteral("file"), leaf.filePath);
        }
        if (!leaf.mode.isEmpty()
                && !inner.contains(QStringLiteral("mode"))) {
            inner.insert(QStringLiteral("mode"), leaf.mode);
        }
        vs.insert(QLatin1String(kState), inner);
    }
    obj.insert(QLatin1String(kState), vs);
    if (leaf.pinned) obj.insert(QLatin1String(kPinned), true);
    if (!leaf.group.isEmpty()) obj.insert(QLatin1String(kGroup), leaf.group);
    return obj;
}

QJsonObject emitIndex(const PaneLayoutIndex &idx);

QJsonObject emitTabsNode(const PaneLayoutIndex &idx)
{
    QJsonObject obj = idx.unknownKeys();
    obj.insert(QLatin1String(kId), idx.indexId());
    obj.insert(QLatin1String(kType), QLatin1String(kTypeTabs));
    QJsonArray kids;
    for (const auto &leaf : idx.views()) {
        kids.append(emitLeafNode(leaf));
    }
    obj.insert(QLatin1String(kChildren), kids);
    if (idx.currentTab() != 0) {
        obj.insert(QLatin1String(kCurrentTab), idx.currentTab());
    }
    if (idx.stacked()) {
        obj.insert(QLatin1String(kStacked), true);
    }
    if (idx.dimension().has_value()) {
        obj.insert(QLatin1String(kDimension), *idx.dimension());
    }
    return obj;
}

QJsonObject emitSplitNode(const PaneLayoutIndex &idx)
{
    QJsonObject obj = idx.unknownKeys();
    obj.insert(QLatin1String(kId), idx.indexId());
    obj.insert(QLatin1String(kType), QLatin1String(kTypeSplit));
    obj.insert(QLatin1String(kDirection),
               idx.orientation() == Qt::Vertical
                   ? QLatin1String(kDirVertical) : QLatin1String(kDirHorizontal));
    QJsonArray kids;
    if (idx.first())  kids.append(emitIndex(*idx.first()));
    if (idx.second()) kids.append(emitIndex(*idx.second()));
    obj.insert(QLatin1String(kChildren), kids);
    if (idx.dimension().has_value()) {
        obj.insert(QLatin1String(kDimension), *idx.dimension());
    }
    return obj;
}

QJsonObject emitIndex(const PaneLayoutIndex &idx)
{
    if (idx.isSplit()) return emitSplitNode(idx);
    // Leaf-index with exactly one view and no index-level extras →
    // emit bare leaf. Otherwise emit tabs wrapping one-or-more leaves
    // (keeps the always-tabs shape Obsidian expects for multi-tab).
    if (idx.viewCount() == 1 && idx.unknownKeys().isEmpty()
            && !idx.stacked() && idx.currentTab() == 0) {
        QJsonObject leafObj = emitLeafNode(*idx.viewAt(0));
        if (idx.dimension().has_value()) {
            leafObj.insert(QLatin1String(kDimension), *idx.dimension());
        }
        return leafObj;
    }
    return emitTabsNode(idx);
}

} // namespace

QJsonObject PaneLayout::toJson() const
{
    if (!m_root) return {};
    return emitIndex(*m_root);
}

} // namespace Corbomite
