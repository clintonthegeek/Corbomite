// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/proxies/FileManagerProxy.h"

#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"

#include <QLoggingCategory>

namespace Corbomite {

namespace {
Q_LOGGING_CATEGORY(lcPluginFileManager, "corbomite.plugin.filemanager")
}

FileManagerProxy::FileManagerProxy(FileManager *fm,
                                   const QSet<QString> &granted,
                                   QString pluginId)
    : m_fm(fm), m_granted(granted), m_pluginId(std::move(pluginId))
{
}

void FileManagerProxy::logDenied(const char *method, const char *req) const
{
    qCWarning(lcPluginFileManager) << "plugin" << m_pluginId << "denied"
                                   << method << "— missing" << req;
}

bool FileManagerProxy::renameFile(TAbstractFile *f, const QString &newPath)
{
    if (!canWrite()) {
        logDenied("renameFile", "vault.write");
        return false;
    }
    return m_fm && m_fm->renameFile(f, newPath);
}

bool FileManagerProxy::processFrontMatter(TFile *f,
                                          FileManager::FrontMatterMutator mut)
{
    if (!canWrite()) {
        logDenied("processFrontMatter", "vault.write");
        return false;
    }
    return m_fm && m_fm->processFrontMatter(f, std::move(mut));
}

bool FileManagerProxy::setFrontMatter(TFile *f,
                                      const QList<FileManager::FrontMatterEntry> &ordered)
{
    if (!canWrite()) {
        logDenied("setFrontMatter", "vault.write");
        return false;
    }
    return m_fm && m_fm->setFrontMatter(f, ordered);
}

TFile *FileManagerProxy::createNewMarkdownFile(TFolder *parent,
                                               const QString &name,
                                               const QByteArray &content)
{
    if (!canWrite()) {
        logDenied("createNewMarkdownFile", "vault.write");
        return nullptr;
    }
    return m_fm ? m_fm->createNewMarkdownFile(parent, name, content) : nullptr;
}

TFile *FileManagerProxy::createNewFile(TFolder *parent, const QString &name,
                                       const QString &ext, const QByteArray &content)
{
    if (!canWrite()) {
        logDenied("createNewFile", "vault.write");
        return nullptr;
    }
    return m_fm ? m_fm->createNewFile(parent, name, ext, content) : nullptr;
}

TFolder *FileManagerProxy::createNewFolder(TFolder *parent)
{
    if (!canWrite()) {
        logDenied("createNewFolder", "vault.write");
        return nullptr;
    }
    return m_fm ? m_fm->createNewFolder(parent) : nullptr;
}

bool FileManagerProxy::insertIntoFile(TFile *f, const QByteArray &content,
                                      FileManager::InsertMode mode)
{
    if (!canWrite()) {
        logDenied("insertIntoFile", "vault.write");
        return false;
    }
    return m_fm && m_fm->insertIntoFile(f, content, mode);
}

bool FileManagerProxy::trashFile(TAbstractFile *f)
{
    if (!canWrite()) {
        logDenied("trashFile", "vault.write");
        return false;
    }
    return m_fm && m_fm->trashFile(f);
}

QString FileManagerProxy::promptForFileRename(TAbstractFile *f, QWidget *parent)
{
    if (!canWrite()) {
        logDenied("promptForFileRename", "vault.write");
        return {};
    }
    return m_fm ? m_fm->promptForFileRename(f, parent) : QString{};
}

bool FileManagerProxy::promptForDeletion(TAbstractFile *f, QWidget *parent)
{
    if (!canWrite()) {
        logDenied("promptForDeletion", "vault.write");
        return false;
    }
    return m_fm && m_fm->promptForDeletion(f, parent);
}

TFolder *FileManagerProxy::getNewFileParent(const QString &hintPath,
                                            const QString &filename) const
{
    if (!canRead()) {
        logDenied("getNewFileParent", "vault.read");
        return nullptr;
    }
    return m_fm ? m_fm->getNewFileParent(hintPath, filename) : nullptr;
}

QString FileManagerProxy::getAvailablePathForAttachment(
    const QString &linktext, const QString &sourcePathHint) const
{
    if (!canRead()) {
        logDenied("getAvailablePathForAttachment", "vault.read");
        return {};
    }
    return m_fm ? m_fm->getAvailablePathForAttachment(linktext, sourcePathHint)
                : QString{};
}

QString FileManagerProxy::generateMarkdownLink(TFile *target,
                                               const QString &sourcePath,
                                               const QString &subpath,
                                               const QString &displayText) const
{
    if (!canMetadataRead()) {
        logDenied("generateMarkdownLink", "metadata.read");
        return {};
    }
    return m_fm ? m_fm->generateMarkdownLink(target, sourcePath, subpath,
                                             displayText)
                : QString{};
}

}  // namespace Corbomite
