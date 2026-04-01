// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QListWidget>

namespace Corbomite {

class TemplatePicker : public QDialog {
    Q_OBJECT
public:
    explicit TemplatePicker(const QStringList &templates, QWidget *parent = nullptr);
    QString selectedTemplate() const;

private:
    QListWidget *m_list;
};

} // namespace Corbomite
