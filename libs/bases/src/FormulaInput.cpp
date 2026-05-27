// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaInput.h"

#include "corbomite/bases/Formula.h"
#include "corbomite/bases/FormulaCandidates.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCompleter>
#include <QIcon>
#include <QScrollBar>
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
    // Pop the completer only on real user input. Using textEdited (not
    // textChanged) means the programmatic setText() inside
    // onCompletionActivated() does not re-trigger the popup right after a
    // selection (reentrancy), and validation still runs on programmatic edits.
    connect(this, &QLineEdit::textEdited, this, &FormulaInput::maybePopupCompleter);
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

void FormulaInput::maybePopupCompleter()
{
    const auto span = FormulaCandidates::tokenAt(text(), cursorPosition());
    if (span.token.isEmpty()) {
        m_completer->popup()->hide();
        return;
    }
    m_completer->setCompletionPrefix(span.token);
    if (m_completer->completionCount() == 0) {
        m_completer->popup()->hide();
        return;
    }
    QRect r = cursorRect();
    r.setWidth(m_completer->popup()->sizeHintForColumn(0)
               + m_completer->popup()->verticalScrollBar()->sizeHint().width());
    m_completer->complete(r);
}

void FormulaInput::onCompletionActivated(const QString &completion)
{
    const int cursor = cursorPosition();
    const auto span = FormulaCandidates::tokenAt(text(), cursor);
    QString t = text();
    t.replace(span.start, cursor - span.start, completion);
    setText(t);
    setCursorPosition(span.start + completion.size());
}

}  // namespace Corbomite::Bases
