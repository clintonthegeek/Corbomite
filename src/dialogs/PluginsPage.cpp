// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginsPage.h"

#include "corbomite/core/PluginApi.h"
#include "corbomite/vault/PluginManager.h"
#include "corbomite/vault/PluginPermissionGrantDialog.h"

#include <KAboutData>
#include <KLocalizedString>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSplitter>
#include <QVBoxLayout>

namespace Corbomite {

namespace {
constexpr int kPluginIdRole = Qt::UserRole + 1;
}

PluginsPage::PluginsPage(PluginManager *mgr, QWidget *parent)
    : QWidget(parent), m_mgr(mgr)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    outer->addWidget(splitter);

    m_list = new QListWidget(splitter);
    splitter->addWidget(m_list);

    m_detail = new QWidget(splitter);
    m_detailLayout = new QVBoxLayout(m_detail);
    m_detailLayout->setContentsMargins(12, 12, 12, 12);
    m_detailHeading = new QLabel(i18n("Select a plugin"), m_detail);
    m_detailHeading->setWordWrap(true);
    m_detailLayout->addWidget(m_detailHeading);
    m_detailLayout->addStretch();
    splitter->addWidget(m_detail);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    rebuild();

    connect(m_list, &QListWidget::currentRowChanged, this,
            &PluginsPage::onSelectionChanged);
    connect(m_list, &QListWidget::itemChanged, this,
            &PluginsPage::onItemChanged);

    if (m_mgr) {
        connect(m_mgr, &PluginManager::pluginEnabled, this,
                [this](const QString &) { rebuild(); });
        connect(m_mgr, &PluginManager::pluginDisabled, this,
                [this](const QString &) { rebuild(); });
    }
}

void PluginsPage::rebuild()
{
    m_list->blockSignals(true);
    m_list->clear();
    if (m_mgr) {
        for (int i = 0; i < m_mgr->pluginCount(); ++i) {
            const auto &info = m_mgr->pluginByIndex(i);
            const QString id = info.metaData.base().pluginId();
            const QString label = QStringLiteral("%1 (%2)")
                .arg(info.metaData.base().name(), id);
            auto *item = new QListWidgetItem(label);
            item->setData(kPluginIdRole, id);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(info.enabled ? Qt::Checked : Qt::Unchecked);

            // MinVersion / ApiLevel-incompatible rows are visible but
            // non-interactive: the checkbox is disabled (ItemIsEnabled
            // stripped) and cannot be toggled.
            const auto state = m_mgr->loadState(id);
            if (state != PluginManager::LoadState::Compatible) {
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            }

            m_list->addItem(item);
        }
    }
    m_list->blockSignals(false);
    if (m_list->count() > 0 && m_list->currentRow() < 0) {
        m_list->setCurrentRow(0);
    } else {
        refreshDetail(m_list->currentRow());
    }
}

void PluginsPage::onSelectionChanged()
{
    refreshDetail(m_list->currentRow());
}

void PluginsPage::onItemChanged(QListWidgetItem *item)
{
    if (m_settingChecked || !m_mgr || !item) return;
    const QString id = item->data(kPluginIdRole).toString();
    const bool checked = item->checkState() == Qt::Checked;
    m_settingChecked = true;
    if (checked) {
        if (!m_mgr->enablePlugin(id)) {
            // Restore checkbox state if the enable was rejected
            // (e.g. permission grant cancelled by the user).
            item->setCheckState(Qt::Unchecked);
        }
    } else {
        m_mgr->disablePlugin(id);
    }
    m_settingChecked = false;
    refreshDetail(m_list->row(item));
}

void PluginsPage::refreshDetail(int row)
{
    // Tear down existing detail widgets.
    while (m_detailLayout->count() > 0) {
        QLayoutItem *child = m_detailLayout->takeAt(0);
        if (auto *w = child->widget()) w->deleteLater();
        delete child;
    }
    m_detailHeading = nullptr;
    m_currentEditable = false;

    if (!m_mgr || row < 0 || row >= m_mgr->pluginCount()) {
        m_detailHeading = new QLabel(i18n("Select a plugin"), m_detail);
        m_detailLayout->addWidget(m_detailHeading);
        m_detailLayout->addStretch();
        return;
    }

    const auto &info = m_mgr->pluginByIndex(row);
    const auto &meta = info.metaData.base();

    m_detailHeading = new QLabel(meta.name(), m_detail);
    QFont f = m_detailHeading->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 2);
    m_detailHeading->setFont(f);
    m_detailLayout->addWidget(m_detailHeading);

    if (!meta.description().isEmpty()) {
        auto *desc = new QLabel(meta.description(), m_detail);
        desc->setWordWrap(true);
        m_detailLayout->addWidget(desc);
    }

    auto *meta1 = new QLabel(
        i18n("Version %1 — %2",
             meta.version().isEmpty() ? i18n("unknown") : meta.version(),
             meta.authors().isEmpty() ? i18n("unknown author")
                                       : meta.authors().first().name()),
        m_detail);
    meta1->setWordWrap(true);
    m_detailLayout->addWidget(meta1);

    auto *trustedLabel = new QLabel(
        info.metaData.trusted() ? i18n("Trusted (system) plugin")
                                 : i18n("Untrusted (user) plugin"),
        m_detail);
    m_detailLayout->addWidget(trustedLabel);

    // Compat gate: surface a prominent "Requires …" label for plugins
    // the host has refused to load. The enable checkbox in the list is
    // already disabled; this label tells the user what to upgrade.
    const auto state = m_mgr->loadState(info.metaData.base().pluginId());
    if (state == PluginManager::LoadState::IncompatibleVersion) {
        auto *warn = new QLabel(
            i18n("Requires Corbomite %1 or newer — this plugin cannot be loaded.",
                 info.metaData.minAppVersion().toString()),
            m_detail);
        warn->setWordWrap(true);
        QFont wf = warn->font();
        wf.setBold(true);
        warn->setFont(wf);
        m_detailLayout->addWidget(warn);
    } else if (state == PluginManager::LoadState::IncompatibleApiLevel) {
        auto *warn = new QLabel(
            i18n("Requires plugin API level %1 or newer (host supports %2) "
                 "— this plugin cannot be loaded.",
                 info.metaData.apiLevel(),
                 CORBOMITE_PLUGIN_API_LEVEL),
            m_detail);
        warn->setWordWrap(true);
        QFont wf = warn->font();
        wf.setBold(true);
        warn->setFont(wf);
        m_detailLayout->addWidget(warn);
    }

    auto *line = new QFrame(m_detail);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_detailLayout->addWidget(line);

    m_detailLayout->addWidget(new QLabel(i18n("Permissions:"), m_detail));

    const QStringList perms = info.metaData.permissions();
    m_currentEditable = !info.metaData.trusted();
    for (const QString &perm : perms) {
        auto *cb = new QCheckBox(
            QStringLiteral("%1 — %2").arg(
                perm, PluginPermissionGrantDialog::describe(perm)),
            m_detail);
        cb->setChecked(true);
        cb->setEnabled(m_currentEditable);
        m_detailLayout->addWidget(cb);
    }
    if (perms.isEmpty()) {
        auto *none = new QLabel(i18n("(none)"), m_detail);
        m_detailLayout->addWidget(none);
    }

    m_detailLayout->addStretch();
}

int PluginsPage::rowCount() const { return m_list->count(); }

void PluginsPage::selectRow(int index) { m_list->setCurrentRow(index); }

bool PluginsPage::isPluginChecked(const QString &pluginId) const
{
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(kPluginIdRole).toString() == pluginId) {
            return m_list->item(i)->checkState() == Qt::Checked;
        }
    }
    return false;
}

void PluginsPage::setPluginChecked(const QString &pluginId, bool checked)
{
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(kPluginIdRole).toString() == pluginId) {
            m_list->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
            return;
        }
    }
}

bool PluginsPage::detailPermissionsEditable() const { return m_currentEditable; }

} // namespace Corbomite
