// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/SortGroupMenuPanel.h"

#include "corbomite/bases/ViewConfigOps.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariant>

namespace Corbomite::Bases {

namespace {
QVariant propVar(const PropertyId &p) { return QStringList{QString::number(int(p.kind)), p.name}; }
PropertyId propFrom(const QVariant &v) {
    const QStringList s = v.toStringList();
    return s.size() == 2 ? PropertyId{PropertyKind(s[0].toInt()), s[1]} : PropertyId{};
}
}  // namespace

SortGroupMenuPanel::SortGroupMenuPanel(QWidget *parent) : QFrame(parent)
{
    setWindowFlags(Qt::Popup);
    setFrameShape(QFrame::StyledPanel);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    root->addWidget(new QLabel(i18n("Sort"), this));
    m_sortRows = new QVBoxLayout();
    root->addLayout(m_sortRows);

    auto *addSort = new QPushButton(i18n("Add sort"), this);
    root->addWidget(addSort);
    connect(addSort, &QPushButton::clicked, this, [this]() {
        if (!m_cfg) return;
        for (const auto &p : m_allProps) {
            bool used = false;
            for (const auto &k : m_cfg->sort) if (k.property == p) { used = true; break; }
            if (!used) { addSortKey(m_cfg->sort, p, QStringLiteral("ASC")); break; }
        }
        rebuild();
        changed();
    });

    root->addWidget(new QLabel(i18n("Group by"), this));
    auto *groupRow = new QHBoxLayout();
    m_groupCombo = new QComboBox(this);
    m_groupDir = new QComboBox(this);
    m_groupDir->addItems({QStringLiteral("ASC"), QStringLiteral("DESC")});
    groupRow->addWidget(m_groupCombo, 1);
    groupRow->addWidget(m_groupDir);
    root->addLayout(groupRow);

    connect(m_groupCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_updating || !m_cfg) return;
        const QVariant v = m_groupCombo->currentData();
        if (v.isValid() && !v.toStringList().isEmpty())
            setGroupBy(*m_cfg, propFrom(v), m_groupDir->currentText());
        else
            setGroupBy(*m_cfg, std::nullopt, QString{});
        changed();
    });
    connect(m_groupDir, &QComboBox::currentTextChanged, this, [this](const QString &d) {
        if (m_updating || !m_cfg || !m_cfg->groupBy.has_value()) return;
        m_cfg->groupBy->direction = d;
        changed();
    });
}

QComboBox *SortGroupMenuPanel::makePropertyCombo(const PropertyId &selected, bool withNone)
{
    auto *c = new QComboBox(this);
    if (withNone) c->addItem(i18n("(none)"), QVariant{});
    int sel = withNone ? 0 : -1;
    for (const auto &p : m_allProps) {
        c->addItem(m_displayName ? m_displayName(p) : p.name, propVar(p));
        if (p == selected) sel = c->count() - 1;
    }
    if (sel >= 0) c->setCurrentIndex(sel);
    return c;
}

void SortGroupMenuPanel::setState(BasesViewConfig *cfg, const QVector<PropertyId> &allProps,
                                  std::function<QString(const PropertyId &)> displayName)
{
    m_cfg = cfg;
    m_allProps = allProps;
    m_displayName = std::move(displayName);
    rebuild();
}

void SortGroupMenuPanel::rebuild()
{
    if (!m_cfg) return;
    m_updating = true;

    while (QLayoutItem *item = m_sortRows->takeAt(0)) {
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }
    for (int i = 0; i < m_cfg->sort.size(); ++i) {
        const SortKey key = m_cfg->sort[i];
        auto *rowWidget = new QWidget(this);
        auto *row = new QHBoxLayout(rowWidget);
        row->setContentsMargins(0, 0, 0, 0);
        QComboBox *prop = makePropertyCombo(key.property, false);
        auto *dir = new QComboBox(rowWidget);
        dir->addItems({QStringLiteral("ASC"), QStringLiteral("DESC")});
        dir->setCurrentText(key.direction);
        auto *remove = new QPushButton(i18n("✕"), rowWidget);
        remove->setFixedWidth(28);
        row->addWidget(prop, 1);
        row->addWidget(dir);
        row->addWidget(remove);
        m_sortRows->addWidget(rowWidget);

        const PropertyId original = key.property;
        connect(prop, &QComboBox::currentIndexChanged, this, [this, original, prop]() {
            if (m_updating || !m_cfg) return;
            const PropertyId next = propFrom(prop->currentData());
            int idx = -1;
            for (int j = 0; j < m_cfg->sort.size(); ++j)
                if (m_cfg->sort[j].property == original) { idx = j; break; }
            if (idx >= 0 && !next.name.isEmpty()) { m_cfg->sort[idx].property = next; changed(); }
        });
        connect(dir, &QComboBox::currentTextChanged, this, [this, original](const QString &d) {
            if (m_updating || !m_cfg) return;
            setSortDirection(m_cfg->sort, original, d);
            changed();
        });
        connect(remove, &QPushButton::clicked, this, [this, original]() {
            if (!m_cfg) return;
            removeSortKey(m_cfg->sort, original);
            rebuild();
            changed();
        });
    }

    m_groupCombo->clear();
    m_groupCombo->addItem(i18n("(none)"), QVariant{});
    int gsel = 0;
    for (const auto &p : m_allProps) {
        m_groupCombo->addItem(m_displayName ? m_displayName(p) : p.name, propVar(p));
        if (m_cfg->groupBy.has_value() && m_cfg->groupBy->property == p)
            gsel = m_groupCombo->count() - 1;
    }
    m_groupCombo->setCurrentIndex(gsel);
    if (m_cfg->groupBy.has_value()) m_groupDir->setCurrentText(m_cfg->groupBy->direction);

    m_updating = false;
}

void SortGroupMenuPanel::changed() { if (m_onChanged) m_onChanged(); }

}  // namespace Corbomite::Bases
