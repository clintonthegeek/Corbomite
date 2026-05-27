// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaInput.h"

#include "corbomite/bases/Formula.h"

#include <QAction>
#include <QCompleter>
#include <QIcon>
#include <QStringListModel>

namespace Corbomite::Bases {

FormulaInput::FormulaInput(QWidget *parent)
    : QLineEdit(parent)
{
    m_indicator = addAction(QIcon(), QLineEdit::TrailingPosition);

    m_candModel = new QStringListModel(this);
    m_completer = new QCompleter(m_candModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setWidget(this);

    connect(this, &QLineEdit::textChanged, this, &FormulaInput::revalidate);
    connect(this, &QLineEdit::textChanged, this, &FormulaInput::maybePopupCompleter);
    connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
            this, &FormulaInput::onCompletionActivated);

    revalidate();
}

void FormulaInput::setCandidates(const QStringList &candidates)
{
    m_candModel->setStringList(candidates);
}

void FormulaInput::revalidate()
{
    const QString src = text();
    bool valid;
    QString err;
    if (src.trimmed().isEmpty()) {
        valid = true;               // neutral
    } else {
        Formula f(src);
        valid = f.isValid();
        if (!valid) err = f.parseError().value_or(QString());
    }

    m_indicator->setIcon(
        src.trimmed().isEmpty() ? QIcon()
        : valid ? QIcon::fromTheme(QStringLiteral("dialog-ok-apply"))
                : QIcon::fromTheme(QStringLiteral("dialog-error")));
    m_indicator->setToolTip(valid ? QString() : err);

    if (valid != m_valid) {
        m_valid = valid;
        Q_EMIT validityChanged(m_valid);
    }
}

void FormulaInput::maybePopupCompleter() { /* implemented in the next task */ }
void FormulaInput::onCompletionActivated(const QString &) { /* implemented in the next task */ }

}  // namespace Corbomite::Bases
