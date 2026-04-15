// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/DailyNoteService.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/models/NoteService.h"
#include "corbomite/models/TemplateService.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/MomentFormatter.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultConfig.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>

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

void DailyNoteService::initFromVaultConfig(Corbomite::VaultConfig &config)
{
    const auto json = config.readDailyNotesJson();
    if (!json) {
        // No vault override; caller's earlier setters / KConfig defaults stand.
        return;
    }
    const QJsonObject &obj = *json;
    if (obj.contains(QStringLiteral("format"))) {
        setDateFormat(obj.value(QStringLiteral("format")).toString());
    }
    if (obj.contains(QStringLiteral("folder"))) {
        setFolder(obj.value(QStringLiteral("folder")).toString());
    }
    if (obj.contains(QStringLiteral("template"))) {
        setTemplateName(obj.value(QStringLiteral("template")).toString());
    }
    // The `autorun` key is documented but not consumed here; MainWindow
    // can read it separately if boot-time auto-open is wanted. Phase 4.1
    // follow-up: write-back on settings change.
}

QString DailyNoteService::todayNotePath() const
{
    const QString pathBase = MomentFormatter::format(
        QDateTime::currentDateTime(), m_dateFormat);
    if (m_folder.isEmpty()) {
        return pathBase + QStringLiteral(".md");
    }
    return m_folder + QLatin1Char('/') + pathBase + QStringLiteral(".md");
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

    const QString relPath = todayNotePath();

    // If exists, just open
    if (todayNoteExists()) {
        return m_noteService->openNote(relPath);
    }

    // Ensure parent directories exist — required when m_dateFormat contains
    // `/` separators (nested-folder formats like `YYYY/MMMM/YYYY-MM-DD`).
    // FileSystemAdapter::writeBinary also does mkpath, but we guard here
    // so the folder/filename split passed to createNote is well-defined.
    {
        const QString absPath = m_vault->path() + QLatin1Char('/') + relPath;
        const QFileInfo fi(absPath);
        const QDir parent = fi.dir();
        if (!parent.exists()) {
            parent.mkpath(QStringLiteral("."));
        }
    }

    // Split relPath into (folderPath, baseName) for createNote.
    // relPath = "Daily/2026/April/2026-04-15.md"
    //   → folderPath = "Daily/2026/April", baseName = "2026-04-15"
    const int lastSlash = relPath.lastIndexOf(QLatin1Char('/'));
    const int dotMd = relPath.lastIndexOf(QStringLiteral(".md"));
    QString folderPath = (lastSlash >= 0)
        ? relPath.left(lastSlash)
        : QString();
    QString baseName = relPath.mid(lastSlash + 1,
                                   dotMd - (lastSlash + 1));

    auto *doc = m_noteService->createNote(baseName, folderPath);
    if (!doc) return nullptr;

    if (!m_templateName.isEmpty() && m_templateService) {
        QString content = m_templateService->loadAndExpand(m_templateName, baseName);
        if (!content.isEmpty()) {
            doc->setMarkdown(content);
            m_noteService->saveNote(doc);
        }
    }

    return doc;
}

} // namespace Corbomite
