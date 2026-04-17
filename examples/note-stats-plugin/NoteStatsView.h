// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>

class QLabel;

namespace Corbomite {
class VaultProxy;
class SearchProxy;
class MetadataCacheReader;
}

namespace NoteStats {

class NoteStatsView : public QWidget
{
    Q_OBJECT
public:
    NoteStatsView(Corbomite::VaultProxy *vault,
                  Corbomite::SearchProxy *search,
                  Corbomite::MetadataCacheReader *metadata,
                  QWidget *parent = nullptr);

private Q_SLOTS:
    void refresh();

private:
    Corbomite::VaultProxy          *m_vault;
    Corbomite::SearchProxy         *m_search;
    Corbomite::MetadataCacheReader *m_metadata;
    QLabel *m_noteCount;
    QLabel *m_wordCount;
    QLabel *m_tagCount;
    QLabel *m_linkCount;
};

} // namespace NoteStats
