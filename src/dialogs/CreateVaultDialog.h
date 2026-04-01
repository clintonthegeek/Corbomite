// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

namespace Corbomite {

class CreateVaultDialog : public QDialog {
    Q_OBJECT

public:
    explicit CreateVaultDialog(QWidget *parent = nullptr);

    QString vaultPath() const;

private:
    void browse();
    void updateOkButton();

    QLineEdit *m_nameEdit;
    QLineEdit *m_locationEdit;
    QPushButton *m_okButton;
};

} // namespace Corbomite
