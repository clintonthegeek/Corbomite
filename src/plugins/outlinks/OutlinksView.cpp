// SPDX-License-Identifier: GPL-3.0-or-later
#include "OutlinksView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/vault/proxies/FileManagerProxy.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KLocalizedString>
#include <QColor>
#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

namespace Corbomite {

namespace {

QString stripMd(QString s)
{
    if (s.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive)) s.chop(3);
    return s;
}

QString resolveTargetPath(const QString &raw)
{
    QString s = raw;
    const int hash = s.indexOf(QLatin1Char('#'));
    if (hash >= 0) s.truncate(hash);
    if (s.isEmpty()) return s;
    if (!s.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive)
        && !s.contains(QLatin1Char('.'))) {
        s += QStringLiteral(".md");
    }
    return s;
}

} // namespace

OutlinksView::OutlinksView(MetadataCacheReader *metadata,
                            VaultProxy *vault,
                            FileManagerProxy *fileManager,
                            WorkspaceController *workspace,
                            QWidget *parent)
    : QWidget(parent)
    , m_metadata(metadata)
    , m_vaultProxy(vault)
    , m_fmProxy(fileManager)
    , m_workspace(workspace)
    , m_headerLabel(new QLabel(i18n("Outgoing Links"), this))
    , m_list(new QListWidget(this))
    , m_emptyLabel(new QLabel(i18n("No outgoing links"), this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    QFont headerFont = m_headerLabel->font();
    headerFont.setBold(true);
    m_headerLabel->setFont(headerFont);
    layout->addWidget(m_headerLabel);

    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_list);

    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(m_emptyLabel);
    m_emptyLabel->setVisible(true);
    m_list->setVisible(false);

    connect(m_list, &QListWidget::itemClicked, this, &OutlinksView::onItemClicked);

    if (m_metadata) {
        connect(m_metadata, &MetadataCacheReader::cacheChanged, this,
                [this](const QString &p) { if (p == m_currentPath) refresh(); });
        connect(m_metadata, &MetadataCacheReader::cacheDeleted, this,
                [this](const QString &p) { if (p == m_currentPath) refresh(); });
        connect(m_metadata, &MetadataCacheReader::linksResolvedFor, this,
                [this](const QString &p) { if (p == m_currentPath) refresh(); });
    }
    if (m_workspace) {
        connect(m_workspace, &WorkspaceController::activeFileChanged, this,
                &OutlinksView::onActiveFileChanged);
        m_currentPath = m_workspace->activeFilePath();
    }
    refresh();
}

void OutlinksView::onActiveFileChanged(const QString &path)
{
    if (m_currentPath == path) return;
    m_currentPath = path;
    refresh();
}

void OutlinksView::refresh()
{
    m_list->clear();

    if (!m_metadata || m_currentPath.isEmpty()) {
        m_headerLabel->setText(i18n("Outgoing Links"));
        m_emptyLabel->setVisible(true);
        m_list->setVisible(false);
        return;
    }

    const QStringList outlinks = m_metadata->outlinksFor(m_currentPath);
    m_headerLabel->setText(i18n("Outgoing Links (%1)", outlinks.size()));

    if (outlinks.isEmpty()) {
        m_emptyLabel->setVisible(true);
        m_list->setVisible(false);
        return;
    }
    m_emptyLabel->setVisible(false);
    m_list->setVisible(true);

    for (const QString &raw : outlinks) {
        const QString target = resolveTargetPath(raw);
        if (target.isEmpty()) continue;

        QString name = stripMd(target);
        name = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);

        auto *item = new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, target);
        item->setToolTip(target);

        const bool exists = m_vaultProxy
            && m_vaultProxy->getAbstractFileByPath(target) != nullptr;
        if (exists) {
            item->setText(name);
        } else {
            item->setText(name + i18n(" (create)"));
            QFont font = item->font();
            font.setItalic(true);
            item->setFont(font);
            item->setForeground(QColor(128, 128, 128));
        }
    }
}

void OutlinksView::onItemClicked(QListWidgetItem *item)
{
    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;

    const bool exists = m_vaultProxy
        && m_vaultProxy->getAbstractFileByPath(path) != nullptr;
    if (exists) {
        if (m_workspace) m_workspace->openFile(path);
        return;
    }

    // Create a new markdown note. For MVP, drop it at the vault root —
    // matches the legacy MainWindow path which used createMarkdownNote
    // with an empty folder. A richer rule (current note's folder) would
    // need VaultProxy::getFileByPath(currentPath)->parent().
    if (!m_fmProxy) return;
    const QString name = stripMd(path);
    auto *created = m_fmProxy->createNewMarkdownFile(nullptr, name);
    if (created && m_workspace) m_workspace->openFile(created->path);
}

} // namespace Corbomite
