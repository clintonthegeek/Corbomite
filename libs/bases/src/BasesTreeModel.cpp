// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesTreeModel.h"

#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/BasesViewConfig.h"
#include "corbomite/bases/QueryController.h"
#include "corbomite/bases/Values.h"

#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/TFile.h"

#include <QVariantMap>

Q_DECLARE_METATYPE(Corbomite::Bases::ValuePtr)

namespace Corbomite::Bases {

BasesTreeModel::BasesTreeModel(QueryController *controller, FileManager *fileManager,
                               QObject *parent)
    : QAbstractItemModel(parent), m_controller(controller), m_fm(fileManager)
{
    qRegisterMetaType<ValuePtr>("Corbomite::Bases::ValuePtr");
    if (m_controller)
        connect(m_controller, &QueryController::resultsChanged, this,
                &BasesTreeModel::onResultsChanged);
    onResultsChanged();
}

BasesTreeModel::~BasesTreeModel() = default;

bool BasesTreeModel::isFlat() const
{
    return m_groups.size() == 1 && !m_groups.front().hasKey();
}

QModelIndex BasesTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column < 0 || column >= m_columns.size()) return {};
    if (!parent.isValid())
        return createIndex(row, column, isFlat() ? FLAT_ID : GROUP_ID);
    // parent must be a group node
    if (parent.internalId() != GROUP_ID) return {};
    return createIndex(row, column, quintptr(parent.row()));
}

QModelIndex BasesTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) return {};
    const quintptr id = child.internalId();
    if (id == GROUP_ID || id == FLAT_ID) return {};   // top-level
    return createIndex(int(id), 0, GROUP_ID);          // entry's parent group
}

int BasesTreeModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        if (m_columns.isEmpty()) return 0;
        return isFlat() ? int(m_groups.front().entries.size())
                        : int(m_groups.size());
    }
    // Only column 0 nodes act as parents (standard Qt tree-model convention).
    if (parent.column() != 0) return 0;
    if (parent.internalId() == GROUP_ID) {
        const int g = parent.row();
        if (g < 0 || g >= m_groups.size()) return 0;
        return int(m_groups[g].entries.size());
    }
    return 0;  // entry leaves have no children
}

int BasesTreeModel::columnCount(const QModelIndex &) const { return m_columns.size(); }

bool BasesTreeModel::isGroupRow(const QModelIndex &index) const
{
    return index.isValid() && index.internalId() == GROUP_ID;
}

PropertyId BasesTreeModel::propertyAt(int column) const
{
    if (column < 0 || column >= m_columns.size()) return {};
    return m_columns[column];
}

BasesEntry *BasesTreeModel::entryAt(const QModelIndex &index) const
{
    if (!index.isValid() || isGroupRow(index)) return nullptr;
    const QVector<std::shared_ptr<BasesEntry>> *entries = nullptr;
    if (index.internalId() == FLAT_ID)
        entries = m_groups.isEmpty() ? nullptr : &m_groups.front().entries;
    else {
        const int g = int(index.internalId());
        if (g < 0 || g >= m_groups.size()) return nullptr;
        entries = &m_groups[g].entries;
    }
    if (!entries || index.row() < 0 || index.row() >= entries->size()) return nullptr;
    return (*entries)[index.row()].get();
}

void BasesTreeModel::populateForTesting(const QVector<BasesEntryGroup> &groups,
                                        const QVector<PropertyId> &columns)
{
    beginResetModel();
    m_groups = groups;
    m_columns = columns;
    m_summaries.clear();
    endResetModel();
}

void BasesTreeModel::onResultsChanged()
{
    beginResetModel();
    m_groups.clear();
    m_columns.clear();
    m_summaries.clear();
    if (m_controller && m_controller->result()) {
        m_groups = m_controller->result()->groups();
        m_columns = m_controller->result()->properties();
        if (const auto *cfg = m_controller->viewConfig()) m_summaries = cfg->summaries;
    }
    endResetModel();
}

QVariant BasesTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};

    if (isGroupRow(index)) {
        const int g = index.row();
        if (g < 0 || g >= m_groups.size()) return {};
        const BasesEntryGroup &grp = m_groups[g];
        if (role == IsGroupRowRole) return true;
        if (role == GroupCountRole) return int(grp.entries.size());
        if (role == Qt::DisplayRole) {
            if (index.column() == 0)
                return grp.hasKey() ? grp.key->toString() : QStringLiteral("(no value)");
            // summary cell for this column iff a summary fn is configured
            const PropertyId pid = propertyAt(index.column());
            const QString fn = m_summaries.value(pid);
            if (!fn.isEmpty() && m_controller && m_controller->result()) {
                auto sv = m_controller->result()->summaryValue(g, pid, fn);
                return sv ? sv->toString() : QString{};
            }
            return {};
        }
        return {};
    }

    // entry row
    if (role == IsGroupRowRole) return false;
    const auto v = valueAt(index);
    if (!v) return {};
    if (role == ValueTypeRole) return v->type();
    if (role == ValuePtrRole)  return QVariant::fromValue(v);
    if (role == Qt::DisplayRole || role == Qt::EditRole) return v->toString();
    return {};
}

ValuePtr BasesTreeModel::valueAt(const QModelIndex &index) const
{
    auto *entry = entryAt(index);
    if (!entry) return nullptr;
    return entry->getValue(propertyAt(index.column()));
}

bool BasesTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || isGroupRow(index) || !m_fm) return false;
    auto *entry = entryAt(index);
    if (!entry || !entry->file()) return false;
    const PropertyId pid = propertyAt(index.column());
    if (pid.kind != PropertyKind::Note) return false;   // only frontmatter editable
    m_fm->processFrontMatter(entry->file(), [&](QVariantMap &fm) { fm.insert(pid.name, value); });
    return true;  // QueryController recompute -> resultsChanged -> reset
}

Qt::ItemFlags BasesTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (!isGroupRow(index) && propertyAt(index.column()).kind == PropertyKind::Note)
        f |= Qt::ItemIsEditable;
    return f;
}

QVariant BasesTreeModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (role != Qt::DisplayRole) return {};
    if (o == Qt::Horizontal) {
        if (section < 0 || section >= m_columns.size()) return {};
        return m_columns[section].toString();
    }
    return {};
}

}  // namespace Corbomite::Bases
