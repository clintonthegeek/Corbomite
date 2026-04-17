// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QToolButton;
class QVBoxLayout;

namespace Corbomite {

class CollapsibleSection : public QWidget {
    Q_OBJECT

public:
    explicit CollapsibleSection(const QString &title, QWidget *parent = nullptr);

    void setContentWidget(QWidget *content);
    QWidget *contentWidget() const;
    void setExpanded(bool expanded);
    bool isExpanded() const;

public Q_SLOTS:
    void toggle();

private:
    QToolButton *m_headerButton;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_layout;
    bool m_expanded = false;
};

} // namespace Corbomite
