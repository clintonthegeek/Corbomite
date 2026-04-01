// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/DailyNoteService.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/models/NoteService.h"
#include "corbomite/models/TemplateService.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"

#include <QDate>

namespace Corbomite {

DailyNoteService::DailyNoteService(VaultModel *vault, NoteService *noteService,
                                     TemplateService *templateService, QObject *parent)
    : QObject(parent)
    , m_vault(vault)
    , m_noteService(noteService)
    , m_templateService(templateService)
{
}

void DailyNoteService::setDateFormat(const QString &format)
{
    m_dateFormat = format;
}

void DailyNoteService::setFolder(const QString &folder)
{
    m_folder = folder;
}

void DailyNoteService::setTemplateName(const QString &name)
{
    m_templateName = name;
}

QString DailyNoteService::todayNotePath() const
{
    QString filename = QDate::currentDate().toString(m_dateFormat);
    if (m_folder.isEmpty()) {
        return filename + QStringLiteral(".md");
    }
    return m_folder + QLatin1Char('/') + filename + QStringLiteral(".md");
}

bool DailyNoteService::todayNoteExists() const
{
    if (!m_vault) return false;
    QString absPath = m_vault->path() + QLatin1Char('/') + todayNotePath();
    return QFileInfo::exists(absPath);
}

NoteDocument *DailyNoteService::openOrCreateToday()
{
    if (!m_vault || !m_noteService) return nullptr;

    QString relPath = todayNotePath();

    // If exists, just open
    if (todayNoteExists()) {
        return m_noteService->openNote(relPath);
    }

    // Create with template if configured
    QString filename = QDate::currentDate().toString(m_dateFormat);
    auto *doc = m_noteService->createNote(filename, m_folder);
    if (!doc) return nullptr;

    if (!m_templateName.isEmpty() && m_templateService) {
        QString content = m_templateService->loadAndExpand(m_templateName, filename);
        if (!content.isEmpty()) {
            doc->setMarkdown(content);
            m_noteService->saveNote(doc);
        }
    }

    return doc;
}

} // namespace Corbomite
