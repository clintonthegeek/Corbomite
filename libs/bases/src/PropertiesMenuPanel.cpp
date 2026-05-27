// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/PropertiesMenuPanel.h"

#include "corbomite/bases/ViewConfigOps.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QIcon>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

namespace Corbomite::Bases {

namespace {
constexpr int PropRole = Qt::UserRole + 1;
QVariant toVariant(const PropertyId &p) {
    return QVariant::fromValue(QStringList{QString::number(int(p.kind)), p.name});
}
PropertyId fromVariant(const QVariant &v) {
    const QStringList s = v.toStringList();
    if (s.size() != 2) return {};
    return PropertyId{PropertyKind(s[0].toInt()), s[1]};
}
}  // namespace

PropertiesMenuPanel::PropertiesMenuPanel(QWidget *parent)
    : QFrame(parent)
{
    setWindowFlags(Qt::Popup);
    setFrameShape(QFrame::StyledPanel);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    m_list = new QListWidget(this);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    root->addWidget(m_list);

    auto *hideAll = new QPushButton(i18n("Hide all"), this);
    root->addWidget(hideAll);

    auto *addFormula = new QPushButton(i18n("Add formula"), this);
    addFormula->setObjectName(QStringLiteral("addFormulaButton"));
    root->addWidget(addFormula);

    connect(m_list, &QListWidget::itemChanged, this, &PropertiesMenuPanel::onItemChanged);
    connect(m_list->model(), &QAbstractItemModel::rowsMoved,
            this, &PropertiesMenuPanel::onRowsMoved);
    connect(hideAll, &QPushButton::clicked, this, [this]() {
        if (!m_order) return;
        hideAllColumns(*m_order);
        rebuild();
        if (m_onChanged) m_onChanged();
    });
    connect(addFormula, &QPushButton::clicked, this,
            &PropertiesMenuPanel::addFormulaRequested);
}

void PropertiesMenuPanel::setState(QVector<PropertyId> *order,
                                   const QVector<PropertyId> &allProps,
                                   std::function<QString(const PropertyId &)> displayName)
{
    m_order = order;
    m_allProps = allProps;
    m_displayName = std::move(displayName);
    rebuild();
}

void PropertiesMenuPanel::setSummaryState(
    const QStringList &availableSummaryNames,
    std::function<QString(const PropertyId &)> currentSummary)
{
    m_summaryNames = availableSummaryNames;
    m_currentSummary = std::move(currentSummary);
}

void PropertiesMenuPanel::rebuild()
{
    if (!m_order) return;
    m_updating = true;
    m_list->clear();
    // Visible columns first (in `order`), then hidden ones (from allProps).
    QVector<PropertyId> ordered = *m_order;
    for (const auto &p : m_allProps)
        if (!ordered.contains(p)) ordered.push_back(p);

    for (const auto &p : ordered) {
        auto *it = new QListWidgetItem(m_list);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        it->setData(PropRole, toVariant(p));

        auto *row = new QWidget(m_list);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(2, 0, 2, 0);

        auto *vis = new QCheckBox(m_displayName ? m_displayName(p) : p.name, row);
        vis->setChecked(m_order->contains(p));
        connect(vis, &QCheckBox::toggled, this, [this, p](bool on) {
            if (m_updating || !m_order) return;
            if (on) { if (!m_order->contains(p)) m_order->push_back(p); }
            else m_order->removeAll(p);
            if (m_onChanged) m_onChanged();
        });
        h->addWidget(vis, 1);

        if (p.kind == PropertyKind::Formula) {
            auto *edit = new QToolButton(row);
            edit->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
            edit->setToolTip(i18nc("@action:button", "Edit formula"));
            connect(edit, &QToolButton::clicked, this,
                    [this, p]() { Q_EMIT editFormulaRequested(p.name); });
            h->addWidget(edit);
            auto *del = new QToolButton(row);
            del->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
            del->setToolTip(i18nc("@action:button", "Delete formula"));
            connect(del, &QToolButton::clicked, this,
                    [this, p]() { Q_EMIT deleteFormulaRequested(p.name); });
            h->addWidget(del);
        }

        auto *summary = new QComboBox(row);
        summary->addItem(i18n("None"), QString());
        for (const auto &n : m_summaryNames) summary->addItem(n, n);
        summary->addItem(i18n("Custom…"),
                         QString::fromLatin1(kCustomSummarySentinel));
        const QString cur = m_currentSummary ? m_currentSummary(p) : QString();
        int idx = summary->findData(cur);
        summary->setCurrentIndex(idx >= 0 ? idx : 0);
        connect(summary, QOverload<int>::of(&QComboBox::activated), this,
                [this, p, summary](int) {
                    if (m_updating) return;
                    Q_EMIT summaryChanged(p, summary->currentData().toString());
                });
        h->addWidget(summary);

        m_list->setItemWidget(it, row);
        it->setSizeHint(row->sizeHint());
    }
    m_updating = false;
}

void PropertiesMenuPanel::onItemChanged()
{
    // Visibility is now handled by per-row QCheckBox lambdas; this slot is inert.
}

void PropertiesMenuPanel::onRowsMoved()
{
    if (m_updating || !m_order) return;
    QVector<PropertyId> next;
    for (int i = 0; i < m_list->count(); ++i) {
        const PropertyId p = fromVariant(m_list->item(i)->data(PropRole));
        if (m_order->contains(p)) next.push_back(p);
    }
    *m_order = next;
    if (m_onChanged) m_onChanged();
}

}  // namespace Corbomite::Bases
