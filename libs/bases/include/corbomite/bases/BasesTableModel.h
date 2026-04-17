// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PropertyId.h"
#include "ValuePtr.h"

#include <QAbstractTableModel>
#include <QVector>

namespace Corbomite {
class FileManager;
}  // namespace Corbomite

namespace Corbomite::Bases {

class QueryController;

/// Qt table-model facade over a QueryController's BasesQueryResult.
///
/// Each row is a BasesEntry; each column is a PropertyId from the
/// active view's `order` (or the result's default property set).
class BasesTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    /// Role used to retrieve the raw Value::type() string (used by the
    /// cell delegate for per-type render dispatch).
    static constexpr int ValueTypeRole = Qt::UserRole + 1;
    /// Role used to retrieve the ValuePtr itself (wrapped as QVariant).
    static constexpr int ValuePtrRole = Qt::UserRole + 2;

    BasesTableModel(QueryController *controller,
                    FileManager *fileManager,
                    QObject *parent = nullptr);
    ~BasesTableModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation o,
                        int role = Qt::DisplayRole) const override;

    /// Accessors used by the cell delegate.
    ValuePtr valueAt(const QModelIndex &index) const;
    PropertyId propertyAt(int column) const;

private Q_SLOTS:
    void onResultsChanged();

private:
    QueryController *m_controller;
    FileManager *m_fm;
    QVector<PropertyId> m_columns;
};

}  // namespace Corbomite::Bases
