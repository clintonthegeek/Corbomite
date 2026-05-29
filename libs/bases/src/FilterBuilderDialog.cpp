// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterBuilderDialog.h"

#include "corbomite/bases/FilterBuilderWidget.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Corbomite::Bases {

FilterBuilderDialog::FilterBuilderDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Edit filters"));
    auto *root = new QVBoxLayout(this);

    m_scope = new QComboBox(this);
    m_scope->addItem(i18n("This view"));          // index 0
    m_scope->addItem(i18n("All views (global)")); // index 1
    root->addWidget(m_scope);

    m_stack = new QStackedWidget(this);
    m_perView = new FilterBuilderWidget(this);   // stack index 0
    m_global  = new FilterBuilderWidget(this);   // stack index 1
    m_stack->addWidget(m_perView);
    m_stack->addWidget(m_global);
    root->addWidget(m_stack, 1);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(m_buttons);

    connect(m_scope, QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_perView, &FilterBuilderWidget::validityChanged, this, &FilterBuilderDialog::updateOkState);
    connect(m_global,  &FilterBuilderWidget::validityChanged, this, &FilterBuilderDialog::updateOkState);

    updateOkState();
}

void FilterBuilderDialog::setScopes(const FilterSpec &globalSpec, const FilterSpec &perViewSpec,
                                    const QStringList &candidates)
{
    m_perView->setSpec(perViewSpec, candidates);
    m_global->setSpec(globalSpec, candidates);
    updateOkState();
}

FilterSpec FilterBuilderDialog::globalSpec() const  { return m_global->spec(); }
FilterSpec FilterBuilderDialog::perViewSpec() const { return m_perView->spec(); }

void FilterBuilderDialog::updateOkState()
{
    const bool ok = m_perView->isValid() && m_global->isValid();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

}  // namespace Corbomite::Bases
