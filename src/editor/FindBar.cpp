// SPDX-License-Identifier: GPL-3.0-or-later
#include "FindBar.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QIcon>

namespace Corbomite {

FindBar::FindBar(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Sunken);

    m_closeButton = new QToolButton(this);
    m_closeButton->setObjectName(QStringLiteral("findBarClose"));
    m_closeButton->setAutoRaise(true);
    m_closeButton->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
    m_closeButton->setToolTip(tr("Close find bar (Esc)"));

    auto *label = new QLabel(tr("F&ind:"), this);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setObjectName(QStringLiteral("findBarLineEdit"));
    m_lineEdit->setClearButtonEnabled(true);
    m_lineEdit->installEventFilter(this);
    label->setBuddy(m_lineEdit);

    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("findBarCountLabel"));

    m_prevButton = new QPushButton(this);
    m_prevButton->setObjectName(QStringLiteral("findBarPrev"));
    m_prevButton->setIcon(QIcon::fromTheme(QStringLiteral("go-up-search")));
    m_prevButton->setToolTip(tr("Previous match (Shift+F3)"));
    m_prevButton->setEnabled(false);

    m_nextButton = new QPushButton(this);
    m_nextButton->setObjectName(QStringLiteral("findBarNext"));
    m_nextButton->setIcon(QIcon::fromTheme(QStringLiteral("go-down-search")));
    m_nextButton->setToolTip(tr("Next match (F3)"));
    m_nextButton->setEnabled(false);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(4);
    layout->addWidget(m_closeButton);
    layout->addWidget(label);
    layout->addWidget(m_lineEdit, 1);
    layout->addWidget(m_countLabel);
    layout->addWidget(m_prevButton);
    layout->addWidget(m_nextButton);
}

FindBar::~FindBar() = default;

void FindBar::setController(Markoff::FindController *controller)
{
    // Implemented in Task 3.
    m_controller = controller;
}

Markoff::FindController *FindBar::controller() const
{
    return m_controller;
}

void FindBar::focusLineEdit()
{
    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
}

bool FindBar::eventFilter(QObject *obj, QEvent *event)
{
    // Implemented in Tasks 5 and 6.
    return QFrame::eventFilter(obj, event);
}

void FindBar::onLineEditTextChanged(const QString &) { /* Task 3 */ }
void FindBar::onNeedleChanged()                       { /* Task 3 */ }
void FindBar::onMatchesChanged()                      { /* Task 4 */ }
void FindBar::onCurrentMatchChanged()                 { /* Task 4 */ }
void FindBar::refreshCountLabel()                     { /* Task 4 */ }
void FindBar::refreshButtonEnableState()              { /* Task 4 */ }

} // namespace Corbomite
