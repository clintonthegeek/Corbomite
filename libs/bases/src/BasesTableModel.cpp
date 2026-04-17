// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesTableModel.h"

#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/BasesQueryResult.h"
#include "corbomite/bases/QueryController.h"
#include "corbomite/bases/Values.h"

#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/TFile.h"

#include <QVariantMap>

Q_DECLARE_METATYPE(Corbomite::Bases::ValuePtr)

namespace Corbomite::Bases {

BasesTableModel::BasesTableModel(QueryController *controller,
                                 FileManager *fileManager,
                                 QObject *parent)
    : QAbstractTableModel(parent),
      m_controller(controller),
      m_fm(fileManager)
{
    qRegisterMetaType<ValuePtr>("Corbomite::Bases::ValuePtr");
    if (m_controller) {
        connect(m_controller, &QueryController::resultsChanged, this,
                &BasesTableModel::onResultsChanged);
    }
    onResultsChanged();
}

BasesTableModel::~BasesTableModel() = default;

int BasesTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    if (!m_controller || !m_controller->result()) return 0;
    return static_cast<int>(m_controller->result()->rows().size());
}

int BasesTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_columns.size();
}

QVariant BasesTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};
    const auto v = valueAt(index);
    if (!v) return {};

    if (role == ValueTypeRole) return v->type();
    if (role == ValuePtrRole)  return QVariant::fromValue(v);

    if (role == Qt::DisplayRole || role == Qt::EditRole)
        return v->toString();

    return {};
}

ValuePtr BasesTableModel::valueAt(const QModelIndex &index) const
{
    if (!m_controller || !m_controller->result()) return nullptr;
    const auto &rows = m_controller->result()->rows();
    if (index.row() < 0 || index.row() >= rows.size()) return nullptr;
    if (index.column() < 0 || index.column() >= m_columns.size()) return nullptr;
    return rows[index.row()]->getValue(m_columns[index.column()]);
}

PropertyId BasesTableModel::propertyAt(int column) const
{
    if (column < 0 || column >= m_columns.size()) return {};
    return m_columns[column];
}

bool BasesTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole) return false;
    if (!index.isValid()) return false;
    if (!m_controller || !m_controller->result()) return false;
    if (!m_fm) return false;

    const auto &rows = m_controller->result()->rows();
    if (index.row() < 0 || index.row() >= rows.size()) return false;
    const PropertyId pid = propertyAt(index.column());
    if (pid.kind != PropertyKind::Note) return false;  // only frontmatter editable

    auto *entry = rows[index.row()].get();
    auto *file = entry->file();
    if (!file) return false;

    // processFrontMatter takes a mutator callback operating on a
    // QVariantMap representation of the frontmatter.
    m_fm->processFrontMatter(file, [&](QVariantMap &fm) {
        fm.insert(pid.name, value);
    });
    // MetadataCache will re-fire cacheChanged; QueryController will
    // recompute and emit resultsChanged; model resets.
    return true;
}

Qt::ItemFlags BasesTableModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (!index.isValid()) return f;
    const PropertyId pid = propertyAt(index.column());
    if (pid.kind == PropertyKind::Note) f |= Qt::ItemIsEditable;
    return f;
}

QVariant BasesTableModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (role != Qt::DisplayRole) return {};
    if (o == Qt::Horizontal) {
        if (section < 0 || section >= m_columns.size()) return {};
        const auto &p = m_columns[section];
        return p.toString();
    }
    return section + 1;
}

void BasesTableModel::onResultsChanged()
{
    beginResetModel();
    m_columns.clear();
    if (m_controller && m_controller->result())
        m_columns = m_controller->result()->properties();
    endResetModel();
}

}  // namespace Corbomite::Bases
