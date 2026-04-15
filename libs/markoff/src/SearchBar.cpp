// SPDX-License-Identifier: GPL-3.0-or-later
#include "markoff/SearchBar.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QKeyEvent>
#include <QIcon>

namespace Markoff {

SearchBar::SearchBar(QWidget *parent)
    : QWidget(parent)
{
    // Opaque background + a top border so the bar reads as a distinct
    // docked strip rather than a transparent overlay on the document.
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Window);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "Markoff--SearchBar { "
        "  background: palette(window); "
        "  border-top: 1px solid palette(mid); "
        "}"));

    buildUi();
    hide();
}

void SearchBar::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(2);

    // Find row
    auto *findRow = new QHBoxLayout;
    findRow->setSpacing(2);

    m_findEdit = new QLineEdit;
    m_findEdit->setPlaceholderText(tr("Find..."));
    m_findEdit->setClearButtonEnabled(true);

    m_prevButton = new QToolButton;
    m_prevButton->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    m_prevButton->setToolTip(tr("Find Previous (Shift+Enter)"));
    m_prevButton->setAutoRaise(true);

    m_nextButton = new QToolButton;
    m_nextButton->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
    m_nextButton->setToolTip(tr("Find Next (Enter)"));
    m_nextButton->setAutoRaise(true);

    m_caseButton = new QToolButton;
    m_caseButton->setText(QStringLiteral("Aa"));
    m_caseButton->setToolTip(tr("Match Case"));
    m_caseButton->setCheckable(true);
    m_caseButton->setAutoRaise(true);

    m_countLabel = new QLabel;
    m_countLabel->setMinimumWidth(60);

    m_closeButton = new QToolButton;
    m_closeButton->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
    m_closeButton->setToolTip(tr("Close (Escape)"));
    m_closeButton->setAutoRaise(true);

    findRow->addWidget(m_findEdit, 1);
    findRow->addWidget(m_prevButton);
    findRow->addWidget(m_nextButton);
    findRow->addWidget(m_caseButton);
    findRow->addWidget(m_countLabel);
    findRow->addWidget(m_closeButton);

    mainLayout->addLayout(findRow);

    // Replace row
    m_replaceRow = new QWidget;
    auto *replaceLayout = new QHBoxLayout(m_replaceRow);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(2);

    m_replaceEdit = new QLineEdit;
    m_replaceEdit->setPlaceholderText(tr("Replace..."));
    m_replaceEdit->setClearButtonEnabled(true);

    m_replaceButton = new QToolButton;
    m_replaceButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-find-replace")));
    m_replaceButton->setToolTip(tr("Replace (Enter in replace field)"));
    m_replaceButton->setAutoRaise(true);

    m_replaceAllButton = new QToolButton;
    m_replaceAllButton->setText(tr("All"));
    m_replaceAllButton->setToolTip(tr("Replace All"));
    m_replaceAllButton->setAutoRaise(true);

    replaceLayout->addWidget(m_replaceEdit, 1);
    replaceLayout->addWidget(m_replaceButton);
    replaceLayout->addWidget(m_replaceAllButton);
    // Spacer to align with find row buttons
    replaceLayout->addSpacing(m_caseButton->sizeHint().width()
                              + m_countLabel->minimumWidth()
                              + m_closeButton->sizeHint().width()
                              + 6); // spacing

    mainLayout->addWidget(m_replaceRow);
    m_replaceRow->hide();

    // Connections
    connect(m_findEdit, &QLineEdit::textChanged,
            this, &SearchBar::searchTextChanged);
    connect(m_nextButton, &QToolButton::clicked,
            this, &SearchBar::findNext);
    connect(m_prevButton, &QToolButton::clicked,
            this, &SearchBar::findPrevious);
    connect(m_closeButton, &QToolButton::clicked,
            this, &SearchBar::closed);
    connect(m_caseButton, &QToolButton::toggled,
            this, [this]() { emit searchTextChanged(m_findEdit->text()); });
    connect(m_replaceButton, &QToolButton::clicked,
            this, &SearchBar::replaceRequested);
    connect(m_replaceAllButton, &QToolButton::clicked,
            this, &SearchBar::replaceAllRequested);
}

QString SearchBar::searchText() const
{
    return m_findEdit->text();
}

void SearchBar::setSearchText(const QString &text)
{
    m_findEdit->setText(text);
}

bool SearchBar::matchCase() const
{
    return m_caseButton->isChecked();
}

QString SearchBar::replaceText() const
{
    return m_replaceEdit->text();
}

void SearchBar::setMatchCount(int current, int total)
{
    if (total == 0) {
        m_countLabel->setText(tr("No results"));
    } else if (total > 65536) {
        m_countLabel->setText(tr("65536+ matches"));
    } else {
        m_countLabel->setText(tr("%1 of %2").arg(current).arg(total));
    }
}

void SearchBar::showFind()
{
    m_replaceRow->hide();
    show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void SearchBar::showReplace()
{
    m_replaceRow->show();
    show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void SearchBar::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        emit closed();
        return;
    }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (m_replaceEdit->hasFocus()) {
            emit replaceRequested();
        } else if (e->modifiers() & Qt::ShiftModifier) {
            emit findPrevious();
        } else {
            emit findNext();
        }
        return;
    }
    QWidget::keyPressEvent(e);
}

} // namespace Markoff
