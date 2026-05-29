// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FilterSpec.h"

#include <QDialog>
#include <QStringList>

class QComboBox;
class QStackedWidget;
class QDialogButtonBox;

namespace Corbomite::Bases {

class FilterBuilderWidget;

/// Edits both filter scopes. A scope combobox switches a QStackedWidget between
/// the per-view builder (index 0) and the global builder (index 1). OK is
/// disabled while either builder is invalid.
class FilterBuilderDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FilterBuilderDialog(QWidget *parent = nullptr);

    void setScopes(const FilterSpec &globalSpec, const FilterSpec &perViewSpec,
                   const QStringList &candidates);

    FilterSpec globalSpec() const;
    FilterSpec perViewSpec() const;

private Q_SLOTS:
    void updateOkState();

private:
    QComboBox *m_scope = nullptr;
    QStackedWidget *m_stack = nullptr;
    FilterBuilderWidget *m_perView = nullptr;
    FilterBuilderWidget *m_global = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
};

}  // namespace Corbomite::Bases
