// SPDX-License-Identifier: GPL-3.0-or-later
#include "BacklinksPanel.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/SQLiteIndex.h"

#include <KLocalizedString>
#include <QVBoxLayout>
#include <QFont>

namespace Corbomite {

BacklinksPanel::BacklinksPanel(QWidget *parent)
    : QWidget(parent)
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

    connect(m_list, &QListWidget::itemClicked, this, &BacklinksPanel::onItemClicked);
}

void BacklinksPanel::setIndex(SQLiteIndex *index)
{
    m_index = index;
    refresh();
}

void BacklinksPanel::setCurrentNote(NoteDocument *doc)
{
    m_currentDoc = doc;
    refresh();
}

void BacklinksPanel::refresh()
{
    m_list->clear();

    if (!m_index || !m_currentDoc) {
        m_headerLabel->setText(i18n("Backlinks"));
        m_emptyLabel->setVisible(true);
        m_list->setVisible(false);
        return;
    }

    auto backlinks = m_index->backlinksFor(m_currentDoc->relativePath());

    m_headerLabel->setText(i18n("Backlinks (%1)", backlinks.size()));

    if (backlinks.isEmpty()) {
        m_emptyLabel->setVisible(true);
        m_list->setVisible(false);
        return;
    }

    m_emptyLabel->setVisible(false);
    m_list->setVisible(true);

    for (const auto &link : backlinks) {
        // Extract note name from path
        QString name = link.sourcePath;
        name = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);

        // Extract folder for disambiguation
        QString folder;
        int lastSlash = link.sourcePath.lastIndexOf(QLatin1Char('/'));
        if (lastSlash > 0) {
            folder = link.sourcePath.left(lastSlash);
        }

        auto *item = new QListWidgetItem(m_list);
        if (folder.isEmpty()) {
            item->setText(name);
        } else {
            item->setText(name + QStringLiteral("  — ") + folder);
        }
        item->setData(Qt::UserRole, link.sourcePath);
        item->setToolTip(link.sourcePath);

        // Future: Add context snippets showing the paragraph containing the link.
        // Options to explore for caching:
        // - Cache context at index time (adds ~100 chars per link to DB, fast retrieval)
        // - Lazy-load context on panel expand (slower but no storage cost)
        // - Store line numbers in links table, read only the relevant line from disk
    }
}

void BacklinksPanel::onItemClicked(QListWidgetItem *item)
{
    QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        Q_EMIT noteActivated(path);
    }
}

} // namespace Corbomite
