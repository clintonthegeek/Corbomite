// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterBuilderWidget.h"

#include "corbomite/bases/FilterRuleRow.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace Corbomite::Bases {

FilterBuilderWidget::FilterBuilderWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto *header = new QHBoxLayout;
    m_conj = new QComboBox(this);
    m_conj->addItem(i18n("All"),  int(Conj::And));   // index 0
    m_conj->addItem(i18n("Any"),  int(Conj::Or));    // index 1
    m_conj->addItem(i18n("None"), int(Conj::Not));   // index 2
    header->addWidget(m_conj);
    header->addStretch(1);

    auto *addRule = new QPushButton(i18n("+ rule"), this);
    addRule->setObjectName(QStringLiteral("addRuleButton"));
    auto *addGroup = new QPushButton(i18n("+ group"), this);
    addGroup->setObjectName(QStringLiteral("addGroupButton"));
    header->addWidget(addRule);
    header->addWidget(addGroup);
    root->addLayout(header);

    m_rowsLayout = new QVBoxLayout;
    m_rowsLayout->setContentsMargins(16, 0, 0, 0);   // indent children
    root->addLayout(m_rowsLayout);

    connect(m_conj, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { onAnyChange(); });
    connect(addRule,  &QPushButton::clicked, this,
            [this]() { addLeafRow(QString()); onAnyChange(); });
    connect(addGroup, &QPushButton::clicked, this,
            [this]() { addGroupRow(FilterSpec::group(Conj::And)); onAnyChange(); });
}

void FilterBuilderWidget::clearRows()
{
    for (const Row &r : m_rows) r.container->deleteLater();
    m_rows.clear();
}

void FilterBuilderWidget::setSpec(const FilterSpec &group, const QVector<FilterPropertyInfo> &properties,
                                  const QStringList &candidates)
{
    m_properties = properties;
    m_candidates = candidates;
    const int idx = m_conj->findData(int(group.conj));
    m_conj->setCurrentIndex(idx >= 0 ? idx : 0);
    clearRows();
    for (const FilterSpec &child : group.children) {
        if (child.kind == FilterSpec::Kind::Leaf) addLeafRow(child.expression);
        else addGroupRow(child);
    }
    // Set initial validity baseline without emitting.
    m_lastValid = isValid();
}

void FilterBuilderWidget::addLeafRow(const QString &expr)
{
    auto *container = new QWidget(this);
    auto *h = new QHBoxLayout(container);
    h->setContentsMargins(0, 0, 0, 0);

    auto *leaf = new FilterRuleRow(container);
    leaf->setProperties(m_properties);
    leaf->setCandidates(m_candidates);
    leaf->setExpression(expr);
    h->addWidget(leaf, 1);

    auto *del = new QToolButton(container);
    del->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    del->setToolTip(i18nc("@action:button", "Remove condition"));
    h->addWidget(del);

    m_rowsLayout->addWidget(container);
    m_rows.push_back({ container, leaf, nullptr });

    connect(leaf, &FilterRuleRow::changed, this, [this]() { onAnyChange(); });
    connect(leaf, &FilterRuleRow::validityChanged, this, [this](bool) { onAnyChange(); });
    connect(del, &QToolButton::clicked, this,
            [this, container]() { removeRow(container); onAnyChange(); });
}

void FilterBuilderWidget::addGroupRow(const FilterSpec &groupSpec)
{
    auto *container = new QWidget(this);
    auto *h = new QHBoxLayout(container);
    h->setContentsMargins(0, 0, 0, 0);

    auto *nested = new FilterBuilderWidget(container);
    nested->setSpec(groupSpec, m_properties, m_candidates);
    h->addWidget(nested, 1);

    auto *del = new QToolButton(container);
    del->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    del->setToolTip(i18nc("@action:button", "Remove group"));
    h->addWidget(del, 0, Qt::AlignTop);

    m_rowsLayout->addWidget(container);
    m_rows.push_back({ container, nullptr, nested });

    connect(nested, &FilterBuilderWidget::changed, this, [this]() { onAnyChange(); });
    connect(nested, &FilterBuilderWidget::validityChanged, this, [this](bool) { onAnyChange(); });
    connect(del, &QToolButton::clicked, this,
            [this, container]() { removeRow(container); onAnyChange(); });
}

void FilterBuilderWidget::removeRow(QWidget *container)
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].container == container) {
            m_rows.removeAt(i);
            break;
        }
    }
    container->deleteLater();
}

FilterSpec FilterBuilderWidget::spec() const
{
    const Conj c = Conj(m_conj->currentData().toInt());
    QVector<FilterSpec> kids;
    for (const Row &r : m_rows) {
        if (r.leaf) kids.push_back(FilterSpec::leaf(r.leaf->expression()));
        else if (r.group) kids.push_back(r.group->spec());
    }
    return FilterSpec::group(c, std::move(kids));
}

bool FilterBuilderWidget::isValid() const
{
    for (const Row &r : m_rows) {
        if (r.leaf && !r.leaf->isExpressionValid()) return false;
        if (r.group && !r.group->isValid()) return false;
    }
    return true;
}

void FilterBuilderWidget::onAnyChange()
{
    Q_EMIT changed();
    const bool v = isValid();
    if (v != m_lastValid) {
        m_lastValid = v;
        Q_EMIT validityChanged(v);
    }
}

}  // namespace Corbomite::Bases
