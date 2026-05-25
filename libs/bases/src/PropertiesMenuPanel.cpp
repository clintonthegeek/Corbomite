// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/PropertiesMenuPanel.h"

#include "corbomite/bases/ViewConfigOps.h"

#include <KLocalizedString>

#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariant>

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

    connect(m_list, &QListWidget::itemChanged, this, &PropertiesMenuPanel::onItemChanged);
    connect(m_list->model(), &QAbstractItemModel::rowsMoved,
            this, &PropertiesMenuPanel::onRowsMoved);
    connect(hideAll, &QPushButton::clicked, this, [this]() {
        if (!m_order) return;
        hideAllColumns(*m_order);
        rebuild();
        if (m_onChanged) m_onChanged();
    });
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
        auto *it = new QListWidgetItem(m_displayName ? m_displayName(p) : p.name, m_list);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable
                     | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
        it->setCheckState(m_order->contains(p) ? Qt::Checked : Qt::Unchecked);
        it->setData(PropRole, toVariant(p));
    }
    m_updating = false;
}

void PropertiesMenuPanel::onItemChanged()
{
    if (m_updating || !m_order) return;
    QVector<PropertyId> next;
    for (int i = 0; i < m_list->count(); ++i) {
        auto *it = m_list->item(i);
        if (it->checkState() == Qt::Checked) next.push_back(fromVariant(it->data(PropRole)));
    }
    *m_order = next;
    if (m_onChanged) m_onChanged();
}

void PropertiesMenuPanel::onRowsMoved()
{
    if (m_updating || !m_order) return;
    QVector<PropertyId> next;
    for (int i = 0; i < m_list->count(); ++i) {
        auto *it = m_list->item(i);
        if (it->checkState() == Qt::Checked) next.push_back(fromVariant(it->data(PropRole)));
    }
    *m_order = next;
    if (m_onChanged) m_onChanged();
}

}  // namespace Corbomite::Bases
