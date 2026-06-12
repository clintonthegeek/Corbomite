// SPDX-License-Identifier: GPL-3.0-or-later
#include "FindBar.h"

#include <markoff/core/FindController.h>

#include <QHBoxLayout>
#include <QVBoxLayout>
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

    // --- find row ---
    auto *findRow = new QWidget(this);
    auto *findLayout = new QHBoxLayout(findRow);
    findLayout->setContentsMargins(0, 0, 0, 0);
    findLayout->setSpacing(4);
    findLayout->addWidget(m_closeButton);
    findLayout->addWidget(label);
    findLayout->addWidget(m_lineEdit, 1);
    findLayout->addWidget(m_countLabel);
    findLayout->addWidget(m_prevButton);
    findLayout->addWidget(m_nextButton);

    // --- replace row (hidden until setReplaceMode(true)) ---
    m_replaceRow = new QWidget(this);
    auto *replaceLabel = new QLabel(tr("Repla&ce:"), m_replaceRow);
    m_replaceLineEdit = new QLineEdit(m_replaceRow);
    m_replaceLineEdit->setObjectName(QStringLiteral("findBarReplaceLineEdit"));
    m_replaceLineEdit->setClearButtonEnabled(true);
    replaceLabel->setBuddy(m_replaceLineEdit);
    m_replaceButton = new QPushButton(tr("Replace"), m_replaceRow);
    m_replaceButton->setObjectName(QStringLiteral("findBarReplace"));
    m_replaceAllButton = new QPushButton(tr("Replace All"), m_replaceRow);
    m_replaceAllButton->setObjectName(QStringLiteral("findBarReplaceAll"));
    auto *replaceLayout = new QHBoxLayout(m_replaceRow);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(4);
    // Pad-left so the replace field aligns under the find field.
    replaceLayout->addSpacing(m_closeButton->sizeHint().width() + 4);
    replaceLayout->addWidget(replaceLabel);
    replaceLayout->addWidget(m_replaceLineEdit, 1);
    replaceLayout->addWidget(m_replaceButton);
    replaceLayout->addWidget(m_replaceAllButton);
    m_replaceRow->setVisible(false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(2);
    layout->addWidget(findRow);
    layout->addWidget(m_replaceRow);

    QObject::connect(m_replaceButton, &QPushButton::clicked,
                     this, &FindBar::replaceRequested);
    QObject::connect(m_replaceAllButton, &QPushButton::clicked,
                     this, &FindBar::replaceAllRequested);

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

void FindBar::setReplaceMode(bool on)
{
    m_replaceMode = on;
    m_replaceRow->setVisible(on);
}

QString FindBar::replacementText() const
{
    return m_replaceLineEdit->text();
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
