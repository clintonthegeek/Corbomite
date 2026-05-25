// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesQueryResult.h"   // BasesEntryGroup
#include "PropertyId.h"
#include "ValuePtr.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QVector>

namespace Corbomite { class FileManager; }

namespace Corbomite::Bases {

class QueryController;

/// 2-level tree facade over a QueryController's BasesQueryResult: group
/// nodes -> entry leaves. Ungrouped (single keyless group) renders flat.
class BasesTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    static constexpr int ValueTypeRole  = Qt::UserRole + 1;  // matches BasesTableModel
    static constexpr int ValuePtrRole   = Qt::UserRole + 2;
    static constexpr int IsGroupRowRole = Qt::UserRole + 3;
    static constexpr int GroupCountRole = Qt::UserRole + 4;

    BasesTreeModel(QueryController *controller, FileManager *fileManager,
                   QObject *parent = nullptr);
    ~BasesTreeModel() override;

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation o, int role = Qt::DisplayRole) const override;

    bool isGroupRow(const QModelIndex &index) const;
    ValuePtr valueAt(const QModelIndex &index) const;       // entry cells; null for group rows
    PropertyId propertyAt(int column) const;

    /// Test seam: populate the snapshot directly, bypassing the controller.
    void populateForTesting(const QVector<BasesEntryGroup> &groups,
                            const QVector<PropertyId> &columns);

private Q_SLOTS:
    void onResultsChanged();

private:
    static constexpr quintptr GROUP_ID = quintptr(-1);
    static constexpr quintptr FLAT_ID  = quintptr(-2);
    bool isFlat() const;
    BasesEntry *entryAt(const QModelIndex &index) const;

    QueryController *m_controller;
    FileManager *m_fm;
    QVector<BasesEntryGroup> m_groups;   // snapshot
    QVector<PropertyId> m_columns;       // snapshot
    QHash<PropertyId, QString> m_summaries;  // from active view config (display only)
};

}  // namespace Corbomite::Bases
