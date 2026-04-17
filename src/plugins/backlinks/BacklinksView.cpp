// SPDX-License-Identifier: GPL-3.0-or-later
#include "BacklinksView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"

#include <KLocalizedString>
#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

namespace Corbomite {

BacklinksView::BacklinksView(MetadataCacheReader *metadata,
                              WorkspaceController *workspace,
                              QWidget *parent)
    : QWidget(parent)
    , m_metadata(metadata)
    , m_workspace(workspace)
    , m_headerLabel(new QLabel(i18n("Backlinks"), this))
    , m_list(new QListWidget(this))
    , m_emptyLabel(new QLabel(i18n("No backlinks"), this))
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

    connect(m_list, &QListWidget::itemClicked, this,
            &BacklinksView::onItemClicked);

    if (m_metadata) {
        connect(m_metadata, &MetadataCacheReader::cacheChanged, this,
                &BacklinksView::onCacheTouched);
        connect(m_metadata, &MetadataCacheReader::cacheDeleted, this,
                &BacklinksView::onCacheTouched);
        connect(m_metadata, &MetadataCacheReader::linksResolvedFor, this,
                &BacklinksView::onCacheTouched);
    }
    if (m_workspace) {
        connect(m_workspace, &WorkspaceController::activeFileChanged, this,
                &BacklinksView::onActiveFileChanged);
        m_currentPath = m_workspace->activeFilePath();
    }
    refresh();
}

void BacklinksView::onActiveFileChanged(const QString &path)
{
    if (m_currentPath == path) return;
    m_currentPath = path;
    refresh();
}

void BacklinksView::onCacheTouched(const QString &)
{
    // Any cache change can affect our backlinks — backlinks are sourced
    // from arbitrary other notes that might link to m_currentPath.
    refresh();
}

void BacklinksView::refresh()
{
    m_list->clear();

    if (!m_metadata || m_currentPath.isEmpty()) {
        m_headerLabel->setText(i18n("Backlinks"));
        m_emptyLabel->setVisible(true);
        m_list->setVisible(false);
        return;
    }

    QString target = m_currentPath;
    if (target.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive))
        target.chop(3);
    const QStringList sources = m_metadata->backlinksFor(target);
    m_headerLabel->setText(i18n("Backlinks (%1)", sources.size()));

    if (sources.isEmpty()) {
        m_emptyLabel->setVisible(true);
        m_list->setVisible(false);
        return;
    }
    m_emptyLabel->setVisible(false);
    m_list->setVisible(true);

    for (const QString &src : sources) {
        QString name = src.mid(src.lastIndexOf(QLatin1Char('/')) + 1);
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
        QString folder;
        const int lastSlash = src.lastIndexOf(QLatin1Char('/'));
        if (lastSlash > 0) folder = src.left(lastSlash);

        auto *item = new QListWidgetItem(m_list);
        item->setText(folder.isEmpty() ? name
                                       : name + QStringLiteral("  — ") + folder);
        item->setData(Qt::UserRole, src);
        item->setToolTip(src);
    }
}

void BacklinksView::onItemClicked(QListWidgetItem *item)
{
    if (!m_workspace) return;
    const QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) m_workspace->openFile(path);
}

} // namespace Corbomite
