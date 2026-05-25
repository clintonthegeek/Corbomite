// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/ViewsMenuPanel.h"

#include "corbomite/bases/BasesQuery.h"
#include "corbomite/bases/BasesViewConfig.h"
#include "corbomite/bases/ViewConfigOps.h"

#include <KLocalizedString>

#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace Corbomite::Bases {

ViewsMenuPanel::ViewsMenuPanel(QWidget *parent) : QFrame(parent)
{
    setWindowFlags(Qt::Popup);
    setFrameShape(QFrame::StyledPanel);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    m_list = new QListWidget(this);
    root->addWidget(m_list);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this]() {
        const QString n = selectedName();
        if (!n.isEmpty() && m_onActivate) m_onActivate(n);
    });

    auto *rename = new QPushButton(i18n("Rename…"), this);
    auto *dup    = new QPushButton(i18n("Duplicate"), this);
    auto *del    = new QPushButton(i18n("Delete"), this);
    auto *def    = new QPushButton(i18n("Set as default"), this);
    root->addWidget(rename);
    root->addWidget(dup);
    root->addWidget(del);
    root->addWidget(def);

    connect(rename, &QPushButton::clicked, this, [this]() {
        const QString cur = selectedName();
        if (cur.isEmpty() || !m_query) return;
        bool ok = false;
        const QString next = QInputDialog::getText(this, i18n("Rename view"),
            i18n("New name:"), QLineEdit::Normal, cur, &ok);
        if (ok && !next.isEmpty() && renameView(*m_query, cur, next)) {
            m_activeName = next; rebuild(); if (m_onChanged) m_onChanged();
        }
    });
    connect(dup, &QPushButton::clicked, this, [this]() {
        const QString cur = selectedName();
        if (cur.isEmpty() || !m_query) return;
        if (duplicateView(*m_query, cur, i18n("%1 copy", cur))) {
            rebuild(); if (m_onChanged) m_onChanged();
        }
    });
    connect(del, &QPushButton::clicked, this, [this]() {
        const QString cur = selectedName();
        if (cur.isEmpty() || !m_query) return;
        if (deleteView(*m_query, cur)) { rebuild(); if (m_onChanged) m_onChanged(); }
    });
    connect(def, &QPushButton::clicked, this, [this]() {
        const QString cur = selectedName();
        if (cur.isEmpty() || !m_query) return;
        if (setDefaultView(*m_query, cur)) { rebuild(); if (m_onChanged) m_onChanged(); }
    });
}

void ViewsMenuPanel::setState(BasesQuery *query, const QString &activeName)
{
    m_query = query;
    m_activeName = activeName;
    rebuild();
}

void ViewsMenuPanel::rebuild()
{
    m_list->clear();
    if (!m_query) return;
    for (const auto &v : m_query->views) {
        if (!v) continue;
        const QString label = (v->name == m_activeName)
            ? i18n("%1 (active)", v->name) : v->name;
        auto *it = new QListWidgetItem(label, m_list);
        it->setData(Qt::UserRole, v->name);
        if (v->name == m_activeName) m_list->setCurrentItem(it);
    }
}

QString ViewsMenuPanel::selectedName() const
{
    auto *it = m_list->currentItem();
    return it ? it->data(Qt::UserRole).toString() : QString{};
}

}  // namespace Corbomite::Bases
