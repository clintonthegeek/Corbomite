// libs/core/src/ItemView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/ItemView.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QIcon>
#include <QCursor>
#include <KLocalizedString>

namespace Corbomite {

ItemView::ItemView(WorkspaceLeaf *leaf, QWidget *parent)
    : View(leaf, parent)
{
    buildHeader();
}

void ItemView::buildHeader()
{
    auto *outerLayout = qobject_cast<QVBoxLayout *>(layout());
    if (!outerLayout) return;

    outerLayout->removeWidget(containerWidget());

    m_headerWidget = new QWidget(this);
    m_headerWidget->setObjectName(QStringLiteral("view-header"));
    auto *headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(4, 2, 4, 2);
    headerLayout->setSpacing(4);

    m_iconLabel = new QLabel(m_headerWidget);
    m_titleLabel = new QLabel(m_headerWidget);
    m_titleLabel->setObjectName(QStringLiteral("view-header-title"));

    m_actionsLayout = new QHBoxLayout;
    m_actionsLayout->setSpacing(2);

    auto *moreBtn = new QToolButton(m_headerWidget);
    moreBtn->setIcon(QIcon::fromTheme(QStringLiteral("overflow-menu")));
    moreBtn->setToolTip(i18n("More options"));
    moreBtn->setAutoRaise(true);
    connect(moreBtn, &QToolButton::clicked, this, &ItemView::showMoreOptionsMenu);

    headerLayout->addWidget(m_iconLabel);
    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addLayout(m_actionsLayout);
    headerLayout->addWidget(moreBtn);

    m_contentWidget = new QWidget(this);

    outerLayout->addWidget(m_headerWidget);
    outerLayout->addWidget(m_contentWidget, 1);
}

QWidget *ItemView::contentWidget() const { return m_contentWidget; }
QWidget *ItemView::headerWidget() const { return m_headerWidget; }

void ItemView::addAction(const QString &icon, const QString &title,
                         std::function<void()> callback)
{
    auto *btn = new QToolButton(m_headerWidget);
    btn->setIcon(QIcon::fromTheme(icon));
    btn->setToolTip(title);
    btn->setAutoRaise(true);
    connect(btn, &QToolButton::clicked, this, [cb = std::move(callback)] { cb(); });
    m_actionsLayout->addWidget(btn);
}

void ItemView::onOpen()
{
    View::onOpen();
    m_titleLabel->setText(getDisplayText());
    m_iconLabel->setPixmap(QIcon::fromTheme(getIcon()).pixmap(16, 16));
}

void ItemView::onMoreOptionsMenu(QMenu *) {}

void ItemView::showMoreOptionsMenu()
{
    QMenu menu(this);
    onMoreOptionsMenu(&menu);
    onPaneMenu(&menu);
    if (!menu.isEmpty())
        menu.exec(QCursor::pos());
}

} // namespace Corbomite
