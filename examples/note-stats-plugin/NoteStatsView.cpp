// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteStatsView.h"

#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/storage/proxies/SearchProxy.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KLocalizedString>
#include <QFormLayout>
#include <QLabel>

namespace NoteStats {

NoteStatsView::NoteStatsView(Corbomite::VaultProxy *vault,
                             Corbomite::SearchProxy *search,
                             Corbomite::MetadataCacheReader *metadata,
                             QWidget *parent)
    : QWidget(parent),
      m_vault(vault), m_search(search), m_metadata(metadata),
      m_noteCount(new QLabel(this)),
      m_wordCount(new QLabel(this)),
      m_tagCount(new QLabel(this)),
      m_linkCount(new QLabel(this))
{
    auto *form = new QFormLayout(this);
    form->addRow(i18n("Notes"), m_noteCount);
    form->addRow(i18n("Words (approx.)"), m_wordCount);
    form->addRow(i18n("Unique tags"), m_tagCount);
    form->addRow(i18n("Total links"), m_linkCount);

    refresh();

    if (m_vault) {
        connect(m_vault, &Corbomite::VaultProxy::created,
                this, &NoteStatsView::refresh);
        connect(m_vault, &Corbomite::VaultProxy::modified,
                this, &NoteStatsView::refresh);
        connect(m_vault, &Corbomite::VaultProxy::deletedFile,
                this, &NoteStatsView::refresh);
    }
    if (m_metadata) {
        connect(m_metadata, &Corbomite::MetadataCacheReader::indexFinished,
                this, &NoteStatsView::refresh);
    }
}

void NoteStatsView::refresh()
{
    if (!m_vault || !m_search) return;

    const auto files = m_vault->getMarkdownFiles();
    m_noteCount->setText(QString::number(files.size()));

    int words = 0;
    for (auto *f : files) {
        const QByteArray body = m_vault->cachedRead(f);
        // Rough word count: delimiter-counted. Good enough for a stats display.
        int w = 0;
        bool in = false;
        for (char c : body) {
            if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
                if (in) { ++w; in = false; }
            } else {
                in = true;
            }
        }
        if (in) ++w;
        words += w;
    }
    m_wordCount->setText(QString::number(words));

    m_tagCount->setText(QString::number(m_search->allTags().size()));
    m_linkCount->setText(QString::number(m_search->allLinks().size()));
}

} // namespace NoteStats
