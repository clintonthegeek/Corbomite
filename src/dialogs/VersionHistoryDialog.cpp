// SPDX-License-Identifier: GPL-3.0-or-later
#include "VersionHistoryDialog.h"

#include "corbomite/core/VersionHistoryService.h"

#include <KLocalizedString>

#include <QDateTime>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

namespace Corbomite {

VersionHistoryDialog::VersionHistoryDialog(VersionHistoryService *service,
                                           QString relativePath,
                                           QWidget *parent)
    : QDialog(parent)
    , m_service(service)
    , m_relativePath(std::move(relativePath))
{
    setWindowTitle(i18n("Version history: %1", m_relativePath));
    resize(760, 460);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("versionHistoryList"));

    m_preview = new QPlainTextEdit(this);
    m_preview->setReadOnly(true);
    m_preview->setObjectName(QStringLiteral("versionHistoryPreview"));
    m_preview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    auto *split = new QSplitter(this);
    split->addWidget(m_list);
    split->addWidget(m_preview);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_restore = buttons->addButton(i18n("Restore this version"),
                                   QDialogButtonBox::AcceptRole);
    m_restore->setObjectName(QStringLiteral("versionHistoryRestore"));
    m_restore->setEnabled(false);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(split);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_restore, &QPushButton::clicked, this, [this] {
        const int row = m_list->currentRow();
        if (row < 0 || row >= m_snapshots.size())
            return;
        if (m_service->restore(m_relativePath, m_snapshots.at(row))) {
            m_restored = true;
            accept();
        }
    });
    connect(m_list, &QListWidget::currentRowChanged,
            this, &VersionHistoryDialog::updateSelection);

    refresh();
}

void VersionHistoryDialog::refresh()
{
    m_list->clear();
    m_snapshots = m_service->snapshots(m_relativePath);
    for (const RecoverySnapshot &s : m_snapshots) {
        const QString when =
            QDateTime::fromMSecsSinceEpoch(s.timestampMs).toString(
                QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        auto *item = new QListWidgetItem(
            s.failure ? QStringLiteral("\u25CF ") + when : when, m_list);
        if (s.failure)
            item->setForeground(QColor(Qt::red));
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
    else
        m_preview->clear();
    updateSelection();
}

void VersionHistoryDialog::updateSelection()
{
    const int row = m_list->currentRow();
    const bool valid = row >= 0 && row < m_snapshots.size();
    if (valid) {
        const auto content = m_service->preview(m_snapshots.at(row));
        m_preview->setPlainText(content.value_or(QString()));
    } else {
        m_preview->clear();
    }
    m_restore->setEnabled(valid);
}

}  // namespace Corbomite
