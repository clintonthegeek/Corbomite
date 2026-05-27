// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/models/PropertyType.h"

#include <markoff/parser/YamlValue.h>

#include <QWidget>

class QLabel;
class QLineEdit;
class QStackedWidget;
class QToolButton;

namespace Corbomite {

class PropertyEditorWidget;

/// True if a frontmatter value can be losslessly round-tripped by the
/// per-type editors (scalars + flat string lists). Maps and lists containing
/// non-scalar elements return false → such rows are read-only + preserved.
bool isEditableFrontmatterValue(const Markoff::YamlValue &value);

/// One row in the Properties panel: [grip] [key] [editor|summary] [delete].
/// Editable rows host a PropertyEditorWidget; read-only rows show a greyed
/// summary and carry preserveFromDisk so the view writes the on-disk value
/// verbatim.
class PropertyRow : public QWidget
{
    Q_OBJECT
public:
    PropertyRow(const QString &key,
                PropertyType type,
                const Markoff::YamlValue &value,
                bool editable,
                QWidget *parent = nullptr);

    QString key() const { return m_key; }
    PropertyType type() const { return m_type; }
    bool isReadOnly() const { return !m_editable; }
    bool preserveFromDisk() const { return !m_editable; }

    /// Current editor value (editable rows only; read-only rows return Null).
    Markoff::YamlValue currentValue() const;

Q_SIGNALS:
    void valueChanged();
    void deleteRequested();
    void keyRenameRequested(const QString &oldKey, const QString &newKey);
    void reorderRequested(int fromVisualY);  // emitted on drag-drop; view maps to index

private:
    void beginInlineRename();
    void commitInlineRename();

    QString m_key;
    PropertyType m_type;
    bool m_editable;

    QToolButton *m_grip = nullptr;
    QStackedWidget *m_keyStack = nullptr;  // label <-> line edit
    QLabel *m_keyLabel = nullptr;
    QLineEdit *m_keyEdit = nullptr;
    PropertyEditorWidget *m_editor = nullptr;  // null for read-only rows
    QToolButton *m_deleteButton = nullptr;
};

}  // namespace Corbomite
