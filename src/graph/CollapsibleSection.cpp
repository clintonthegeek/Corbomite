// SPDX-License-Identifier: GPL-3.0-or-later
#include "CollapsibleSection.h"

#include <QToolButton>
#include <QVBoxLayout>

namespace Corbomite {

CollapsibleSection::CollapsibleSection(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    m_headerButton = new QToolButton(this);
    m_headerButton->setText(title);
    m_headerButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_headerButton->setArrowType(Qt::RightArrow);
    m_headerButton->setCheckable(true);
    m_headerButton->setChecked(false);
    m_headerButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_headerButton->setStyleSheet(
        QStringLiteral("QToolButton { border: none; font-weight: bold; padding: 4px; }")
    );

    m_layout->addWidget(m_headerButton);

    connect(m_headerButton, &QToolButton::toggled, this, [this](bool checked) {
        setExpanded(checked);
    });
}

void CollapsibleSection::setContentWidget(QWidget *content)
{
    if (m_content) {
        m_layout->removeWidget(m_content);
        m_content->setParent(nullptr);
    }

    m_content = content;
    if (m_content) {
        m_layout->addWidget(m_content);
        m_content->setVisible(m_expanded);
    }
}

QWidget *CollapsibleSection::contentWidget() const
{
    return m_content;
}

void CollapsibleSection::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_headerButton->setChecked(expanded);
    m_headerButton->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    if (m_content) {
        m_content->setVisible(expanded);
    }
}

bool CollapsibleSection::isExpanded() const
{
    return m_expanded;
}

void CollapsibleSection::toggle()
{
    setExpanded(!m_expanded);
}

} // namespace Corbomite
