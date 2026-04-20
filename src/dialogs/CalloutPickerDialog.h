// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;

namespace Corbomite {

/// Modal picker for Obsidian-style callout blocks. Returns the selected
/// callout type (`note`, `tip`, ...) and an optional title string. The
/// caller composes the markdown (`> [!type] title\n> `) at insertion
/// time — this dialog is UI-only.
class CalloutPickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit CalloutPickerDialog(QWidget *parent = nullptr);

    QString selectedType() const;
    QString title() const;

private:
    void updatePreview();

    QComboBox *m_combo;
    QLineEdit *m_title;
    QLabel    *m_preview;
};

} // namespace Corbomite
