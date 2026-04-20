// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

class QCheckBox;
class QSpinBox;

namespace Corbomite {

/// Row/column/header picker for "Insert > Table" from the main menu.
class InsertTableDialog : public QDialog {
    Q_OBJECT

public:
    explicit InsertTableDialog(QWidget *parent = nullptr);

    int rows() const;
    int cols() const;
    bool firstRowAsHeader() const;

private:
    QSpinBox  *m_rows;
    QSpinBox  *m_cols;
    QCheckBox *m_header;
};

} // namespace Corbomite
