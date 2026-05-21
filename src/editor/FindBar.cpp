// SPDX-License-Identifier: GPL-3.0-or-later
#include "FindBar.h"

#include <markoff/core/FindController.h>

#include <QHBoxLayout>
#include <QKeyEvent>
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

    QObject::connect(m_prevButton, &QPushButton::clicked, this, [this]() {
        if (m_controller) m_controller->findPrevious();
    });
    QObject::connect(m_nextButton, &QPushButton::clicked, this, [this]() {
        if (m_controller) m_controller->findNext();
    });

    QObject::connect(m_closeButton, &QToolButton::clicked,
                     this, &FindBar::closeRequested);

    QObject::connect(m_lineEdit, &QLineEdit::textChanged,
                     this, &FindBar::onLineEditTextChanged);
}

FindBar::~FindBar() = default;

void FindBar::setController(Markoff::FindController *controller)
{
    if (m_controller == controller) return;

    if (m_controller) {
        QObject::disconnect(m_controller, nullptr, this, nullptr);
    }

    m_controller = controller;

    if (m_controller) {
        QObject::connect(m_controller, &Markoff::FindController::needleChanged,
                         this, &FindBar::onNeedleChanged);
        QObject::connect(m_controller, &Markoff::FindController::matchesChanged,
                         this, &FindBar::onMatchesChanged);
        QObject::connect(m_controller, &Markoff::FindController::currentMatchChanged,
                         this, &FindBar::onCurrentMatchChanged);
    }

    refreshCountLabel();
    refreshButtonEnableState();
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
    if (obj == m_lineEdit && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (m_controller) {
                if (ke->modifiers() & Qt::ShiftModifier) m_controller->findPrevious();
                else                                     m_controller->findNext();
            }
            return true;  // consume — don't let Return cascade
        }
        if (ke->key() == Qt::Key_Escape) {
            Q_EMIT closeRequested();
            return true;
        }
    }
    return QFrame::eventFilter(obj, event);
}

void FindBar::onLineEditTextChanged(const QString &text)
{
    if (m_applyingControllerNeedle) return;
    if (!m_controller) return;
    m_controller->setNeedle(text);
}

void FindBar::onNeedleChanged()
{
    if (!m_controller) return;
    if (m_lineEdit->text() == m_controller->needle()) return;
    m_applyingControllerNeedle = true;
    m_lineEdit->setText(m_controller->needle());
    m_applyingControllerNeedle = false;
}

void FindBar::refreshCountLabel()
{
    if (!m_controller || m_controller->needle().isEmpty()) {
        m_countLabel->setText(QString());
        return;
    }
    const int matchCount = m_controller->matchCount();
    if (matchCount == 0) {
        m_countLabel->setText(tr("No matches"));
        return;
    }
    const int current = m_controller->currentMatchIndex();
    m_countLabel->setText(tr("%1 of %2").arg(current + 1).arg(matchCount));
}

void FindBar::refreshButtonEnableState()
{
    const bool hasMatches = m_controller && m_controller->matchCount() > 0;
    m_prevButton->setEnabled(hasMatches);
    m_nextButton->setEnabled(hasMatches);
}

void FindBar::onMatchesChanged()
{
    refreshCountLabel();
    refreshButtonEnableState();
}

void FindBar::onCurrentMatchChanged()
{
    refreshCountLabel();
}

} // namespace Corbomite
