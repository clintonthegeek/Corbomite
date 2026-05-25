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

// --- data()/setData()/flags()/headerData() land in Task 3 ---
QVariant BasesTreeModel::data(const QModelIndex &, int) const { return {}; }
bool BasesTreeModel::setData(const QModelIndex &, const QVariant &, int) { return false; }
Qt::ItemFlags BasesTreeModel::flags(const QModelIndex &index) const
{ return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags; }
QVariant BasesTreeModel::headerData(int, Qt::Orientation, int) const { return {}; }
ValuePtr BasesTreeModel::valueAt(const QModelIndex &) const { return nullptr; }

}  // namespace Corbomite::Bases
