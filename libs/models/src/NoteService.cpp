// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/NoteService.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/SQLiteIndex.h"

namespace Corbomite {

NoteService::NoteService(VaultModel *vault, QObject *parent)
    : QObject(parent)
    , m_vault(vault)
{
}

NoteDocument *NoteService::openNote(const QString &relativePath)
{
    return m_vault->openDocument(relativePath);
}

NoteDocument *NoteService::createNote(const QString &name, const QString &folderPath)
{
    QString relPath = resolveUniquePath(name, folderPath);

    FileSystemAdapter fs;
    QString absPath = m_vault->path() + QLatin1Char('/') + relPath;
    if (!fs.writeFile(absPath, QString())) {
        return nullptr;
    }

    m_vault->addNote(relPath);
    return m_vault->openDocument(relPath);
}

bool NoteService::saveNote(NoteDocument *doc)
{
    if (!doc) return false;

    FileSystemAdapter fs;
    if (!fs.writeFile(doc->filePath(), doc->markdown())) {
        return false;
    }

    doc->setModified(false);
    Q_EMIT doc->saved();
    m_vault->updateNoteMeta(doc->relativePath());
    return true;
}

bool NoteService::renameNote(const QString &oldRelPath, const QString &newRelPath)
{
    FileSystemAdapter fs;
    QString oldAbs = m_vault->path() + QLatin1Char('/') + oldRelPath;
    QString newAbs = m_vault->path() + QLatin1Char('/') + newRelPath;

    if (!fs.rename(oldAbs, newAbs)) {
        return false;
    }

    // Repair links in other notes that reference the old path
    if (m_searchIndex) {
        m_searchIndex->repairLinks(oldRelPath, newRelPath, m_vault->path());
    }

    m_vault->renameNote(oldRelPath, newRelPath);
    return true;
}

void NoteService::setSearchIndex(SQLiteIndex *index)
{
    m_searchIndex = index;
}

bool NoteService::deleteNote(const QString &relativePath)
{
    FileSystemAdapter fs;
    QString absPath = m_vault->path() + QLatin1Char('/') + relativePath;

    if (!fs.remove(absPath)) {
        return false;
    }

    m_vault->removeNote(relativePath);
    return true;
}

QString NoteService::resolveUniquePath(const QString &baseName, const QString &folder) const
{
    QString prefix = folder.isEmpty() ? QString() : folder + QLatin1Char('/');
    QString candidate = prefix + baseName + QStringLiteral(".md");

    if (!m_vault->noteExists(candidate)) {
        return candidate;
    }

    for (int i = 1; i < 1000; ++i) {
        candidate = prefix + baseName + QStringLiteral(" %1.md").arg(i);
        if (!m_vault->noteExists(candidate)) {
            return candidate;
        }
    }

    return candidate; // Give up, will collide
}

} // namespace Corbomite
