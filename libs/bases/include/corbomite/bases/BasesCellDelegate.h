// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStyledItemDelegate>

namespace Corbomite::Bases {

/// Per-type cell renderer + inline editor for a BasesTableModel.
///
/// Dispatches on `BasesTableModel::ValueTypeRole`:
///   - Boolean  -> QCheckBox editor.
///   - Number   -> QDoubleSpinBox.
///   - Date     -> QDateEdit (date-only) / QDateTimeEdit (with time).
///   - Error    -> warning-styled painted text, not editable.
///   - String/Tag/Link/URL/Icon/Image/HTML/Markdown/List/Object -> QLineEdit.
///
/// Rich rendering for Image / HTML / Markdown defers to a placeholder
/// label; a Cluster-J `EmbedRenderer` wire-in is a follow-up.
class BasesCellDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit BasesCellDelegate(QObject *parent = nullptr);
    ~BasesCellDelegate() override;

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override;

Q_SIGNALS:
    void linkClicked(const QString &target, Qt::KeyboardModifiers mods);
    void tagClicked(const QString &tag);
    void urlClicked(const QString &url);
};

}  // namespace Corbomite::Bases
