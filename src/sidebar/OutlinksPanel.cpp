// SPDX-License-Identifier: GPL-3.0-or-later
#include "OutlinksPanel.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/models/VaultModel.h"

#include <KLocalizedString>
#include <QVBoxLayout>
#include <QFont>

namespace Corbomite {

OutlinksPanel::OutlinksPanel(QWidget *parent)
    : QWidget(parent)
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

    connect(m_list, &QListWidget::itemClicked, this, &OutlinksPanel::onItemClicked);
}

void OutlinksPanel::setIndex(SQLiteIndex *index)
{
    m_index = index;
    refresh();
}

void OutlinksPanel::setMetadataCache(MetadataCache *cache)
{
    if (m_cache) {
        disconnect(m_cache, nullptr, this, nullptr);
    }
    m_cache = cache;
    if (m_cache) {
        // Refresh when the current note's own links resolve, or when it's
        // mutated/deleted — its outlinks may have changed.
        connect(m_cache, &MetadataCache::linksResolvedFor,
                this, [this](const QString &path) {
            if (m_currentDoc && path == m_currentDoc->relativePath()) refresh();
        });
        connect(m_cache, &MetadataCache::cacheChanged,
                this, [this](const QString &path, const QString &, const CachedMetadata &) {
            if (m_currentDoc && path == m_currentDoc->relativePath()) refresh();
        });
        connect(m_cache, &MetadataCache::cacheDeleted,
                this, [this](const QString &path, const CachedMetadata &) {
            if (m_currentDoc && path == m_currentDoc->relativePath()) refresh();
        });
    }
}

void OutlinksPanel::setVaultModel(VaultModel *vault)
{
    m_vault = vault;
}

void OutlinksPanel::setCurrentNote(NoteDocument *doc)
{
    m_currentDoc = doc;
    refresh();
}

void OutlinksPanel::refresh()
{
    m_list->clear();

    if (!m_index || !m_currentDoc) {
        m_headerLabel->setText(i18n("Outgoing Links"));
        m_emptyLabel->setVisible(true);
        m_list->setVisible(false);
        return;
    }

    auto outlinks = m_index->outlinksFor(m_currentDoc->relativePath());

    m_headerLabel->setText(i18n("Outgoing Links (%1)", outlinks.size()));

    if (outlinks.isEmpty()) {
        m_emptyLabel->setVisible(true);
        m_list->setVisible(false);
        return;
    }

    m_emptyLabel->setVisible(false);
    m_list->setVisible(true);

    for (const auto &link : outlinks) {
        // Extract note name from target path
        QString name = link.targetPath;
        name = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);

        auto *item = new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, link.targetPath);
        item->setToolTip(link.targetPath);

        // Check if target exists
        bool exists = m_vault && m_vault->noteExists(link.targetPath);

        if (exists) {
            item->setText(name);
        } else {
            // Orphan link — show in muted italic
            item->setText(name + i18n(" (create)"));
            QFont font = item->font();
            font.setItalic(true);
            item->setFont(font);
            item->setForeground(QColor(128, 128, 128));
        }

        // Show link type as icon
        if (link.linkType == QStringLiteral("embed")) {
            item->setIcon(QIcon::fromTheme(QStringLiteral("insert-image")));
        } else if (link.linkType == QStringLiteral("markdown")) {
            item->setIcon(QIcon::fromTheme(QStringLiteral("text-html")));
        }
        // wiki links get no special icon (default)
    }
}

void OutlinksPanel::onItemClicked(QListWidgetItem *item)
{
    QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;

    bool exists = m_vault && m_vault->noteExists(path);
    if (exists) {
        Q_EMIT noteActivated(path);
    } else {
        // Extract name for creation (strip .md)
        QString name = path;
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
        Q_EMIT createNoteRequested(name);
    }
}

} // namespace Corbomite
