// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/PluginPermissionGrantDialog.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Corbomite {

PluginPermissionGrantDialog::PluginPermissionGrantDialog(
        const QString &pluginName,
        const QString &pluginDescription,
        const QStringList &requestedPermissions,
        QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18nc("@title:window", "Plugin Permissions"));

    auto *layout = new QVBoxLayout(this);

    auto *header = new QLabel(this);
    header->setTextFormat(Qt::PlainText);
    header->setWordWrap(true);
    header->setText(i18nc("@info plugin permission dialog header",
                          "The plugin “%1” declares it needs the following "
                          "permissions. Granting a permission allows the "
                          "plugin to use the corresponding capability.",
                          pluginName));
    layout->addWidget(header);

    if (!pluginDescription.isEmpty()) {
        auto *desc = new QLabel(pluginDescription, this);
        desc->setTextFormat(Qt::PlainText);
        desc->setWordWrap(true);
        desc->setStyleSheet(QStringLiteral("color: gray;"));
        layout->addWidget(desc);
    }

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    for (const QString &token : requestedPermissions) {
        auto *box = new QCheckBox(this);
        box->setText(QStringLiteral("%1 — %2").arg(token, describe(token)));
        box->setChecked(true);
        m_boxes.insert(token, box);
        layout->addWidget(box);
    }

    auto *buttons = new QDialogButtonBox(this);
    auto *grantBtn = buttons->addButton(i18nc("@action:button", "Grant"),
                                         QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    grantBtn->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted,
            this, &PluginPermissionGrantDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &PluginPermissionGrantDialog::onRejected);
}

QSet<QString> PluginPermissionGrantDialog::grantedIfAccepted() const
{
    if (m_cancelled) return {};
    QSet<QString> out;
    for (auto it = m_boxes.cbegin(); it != m_boxes.cend(); ++it) {
        if (it.value()->isChecked()) out.insert(it.key());
    }
    return out;
}

void PluginPermissionGrantDialog::setCheckedForTest(const QString &token, bool checked)
{
    if (auto *box = m_boxes.value(token, nullptr)) {
        box->setChecked(checked);
    }
}

void PluginPermissionGrantDialog::cancelForTest()
{
    m_cancelled = true;
}

void PluginPermissionGrantDialog::onAccepted()
{
    m_cancelled = false;
    accept();
}

void PluginPermissionGrantDialog::onRejected()
{
    m_cancelled = true;
    reject();
}

QString PluginPermissionGrantDialog::describe(const QString &t)
{
    static const QHash<QString, QString> descs{
        {QStringLiteral("vault.read"),
         i18n("Read note contents from your vault")},
        {QStringLiteral("vault.write"),
         i18n("Create, modify, delete, or rename notes")},
        {QStringLiteral("metadata.read"),
         i18n("Read note metadata (frontmatter, links, tags, headings)")},
        {QStringLiteral("ui.commands"),
         i18n("Add commands and keyboard shortcuts")},
        {QStringLiteral("ui.views"),
         i18n("Add sidebar panels and main-area views")},
        {QStringLiteral("ui.menus"),
         i18n("Inject items into context menus")},
        {QStringLiteral("workspace"),
         i18n("Open files, split panes, manage tabs and windows")},
        {QStringLiteral("network"),
         i18n("Connect to external network services")},
        {QStringLiteral("secrets"),
         i18n("Store and retrieve credentials in the system keyring")},
        {QStringLiteral("process"),
         i18n("Run external programs")},
        {QStringLiteral("config"),
         i18n("Read and write application settings")},
        {QStringLiteral("render"),
         i18n("Extend note rendering (mermaid, math, syntax, embeds)")},
    };
    return descs.value(t, t);
}

} // namespace Corbomite
