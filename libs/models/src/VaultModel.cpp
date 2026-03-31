// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/VaultModel.h"
#include <QDir>
#include <QFileInfo>

namespace Corbomite {

VaultModel::VaultModel(QObject *parent)
    : QObject(parent)
{
}

VaultModel::~VaultModel()
{
    close();
}

void VaultModel::open(const QString &vaultPath)
{
    close();
    m_vaultPath = vaultPath;

    // Ensure .corbomite dir exists
    m_fs.mkpath(configPath());

    // Scan vault
    const auto notes = m_scanner.scan(m_vaultPath);
    for (const auto &meta : notes) {
        m_notes.insert(meta.relativePath, meta);
    }

    Q_EMIT vaultScanned();
}

void VaultModel::close()
{
    qDeleteAll(m_docs);
    m_docs.clear();
    m_notes.clear();
    m_vaultPath.clear();
}

bool VaultModel::isOpen() const
{
    return !m_vaultPath.isEmpty();
}

QString VaultModel::path() const
{
    return m_vaultPath;
}

QString VaultModel::name() const
{
    return QDir(m_vaultPath).dirName();
}

QString VaultModel::configPath() const
{
    return m_vaultPath + QStringLiteral("/.corbomite");
}

QVector<NoteMeta> VaultModel::allNotes() const
{
    return QVector<NoteMeta>(m_notes.cbegin(), m_notes.cend());
}

NoteMeta VaultModel::noteMeta(const QString &relativePath) const
{
    return m_notes.value(relativePath);
}

bool VaultModel::noteExists(const QString &relativePath) const
{
    return m_notes.contains(relativePath);
}

NoteDocument *VaultModel::openDocument(const QString &relativePath)
{
    if (auto *doc = m_docs.value(relativePath)) {
        return doc;
    }

    auto content = m_fs.readFile(m_vaultPath + QLatin1Char('/') + relativePath);
    auto *doc = new NoteDocument(m_vaultPath, relativePath, this);
    if (content.has_value()) {
        doc->setMarkdown(content.value());
        doc->setModified(false);
    }
    m_docs.insert(relativePath, doc);
    return doc;
}

NoteDocument *VaultModel::cachedDocument(const QString &relativePath) const
{
    return m_docs.value(relativePath);
}

void VaultModel::addNote(const QString &relativePath)
{
    QString absPath = m_vaultPath + QLatin1Char('/') + relativePath;
    QFileInfo fi(absPath);
    if (fi.exists()) {
        m_notes.insert(relativePath, NoteMeta::fromFileInfo(fi, m_vaultPath));
    } else {
        m_notes.insert(relativePath, NoteMeta::fromRelativePath(relativePath));
    }
    Q_EMIT noteAdded(relativePath);
}

void VaultModel::removeNote(const QString &relativePath)
{
    m_notes.remove(relativePath);
    if (auto *doc = m_docs.take(relativePath)) {
        doc->deleteLater();
    }
    Q_EMIT noteRemoved(relativePath);
}

void VaultModel::renameNote(const QString &oldPath, const QString &newPath)
{
    auto meta = m_notes.take(oldPath);
    meta.relativePath = newPath;
    m_notes.insert(newPath, meta);

    if (auto *doc = m_docs.take(oldPath)) {
        m_docs.insert(newPath, doc);
    }

    Q_EMIT noteRenamed(oldPath, newPath);
}

void VaultModel::updateNoteMeta(const QString &relativePath)
{
    QString absPath = m_vaultPath + QLatin1Char('/') + relativePath;
    QFileInfo fi(absPath);
    if (fi.exists() && m_notes.contains(relativePath)) {
        m_notes[relativePath] = NoteMeta::fromFileInfo(fi, m_vaultPath);
        Q_EMIT noteModified(relativePath);
    }
}

} // namespace Corbomite
