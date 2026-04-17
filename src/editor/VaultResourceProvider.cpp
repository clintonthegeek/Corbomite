// SPDX-License-Identifier: GPL-3.0-or-later
#include "VaultResourceProvider.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>

namespace Corbomite {

VaultResourceProvider::VaultResourceProvider(Vault *vault, const QString &noteRelativePath)
    : m_vault(vault)
    , m_vaultPath(vault ? vault->basePath() : QString())
{
    int lastSlash = noteRelativePath.lastIndexOf(QLatin1Char('/'));
    m_noteDir = lastSlash > 0 ? noteRelativePath.left(lastSlash) : QString();
}

QString VaultResourceProvider::resolveTarget(const QString &target) const
{
    if (target.isEmpty()) return {};

    QString withExt = target;
    if (!withExt.endsWith(QStringLiteral(".md")) && !withExt.endsWith(QStringLiteral(".canvas"))) {
        withExt += QStringLiteral(".md");
    }

    // Try relative to current note first
    if (!m_noteDir.isEmpty()) {
        QString relative = m_noteDir + QLatin1Char('/') + withExt;
        if (m_vault && m_vault->getAbstractFileByPath(relative)) {
            return relative;
        }
    }

    // Try as-is (relative to vault root)
    if (m_vault && m_vault->getAbstractFileByPath(withExt)) {
        return withExt;
    }

    // Shortest-path match: search all notes for matching filename
    if (m_vault) {
        QString filename = withExt.mid(withExt.lastIndexOf(QLatin1Char('/')) + 1);
        for (TFile *f : m_vault->getMarkdownFiles()) {
            if (f->path.endsWith(QLatin1Char('/') + filename) || f->path == filename) {
                return f->path;
            }
        }
    }

    return withExt;
}

QUrl VaultResourceProvider::resolveImage(const QString &name) const
{
    if (m_vaultPath.isEmpty()) return {};

    if (!m_noteDir.isEmpty()) {
        QString path = m_vaultPath + QLatin1Char('/') + m_noteDir + QLatin1Char('/') + name;
        if (QFileInfo::exists(path)) {
            return QUrl::fromLocalFile(path);
        }
    }

    QString path = m_vaultPath + QLatin1Char('/') + name;
    if (QFileInfo::exists(path)) {
        return QUrl::fromLocalFile(path);
    }

    return {};
}

std::optional<QString> VaultResourceProvider::resolveEmbed(const QString &name) const
{
    QString resolved = resolveTarget(name);
    if (m_vault) {
        if (TFile *tf = m_vault->getFileByPath(resolved)) {
            return QString::fromUtf8(m_vault->cachedRead(tf));
        }
        QString absPath = m_vaultPath + QLatin1Char('/') + resolved;
        QFile file(absPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString::fromUtf8(file.readAll());
        }
    }
    return std::nullopt;
}

QUrl VaultResourceProvider::resolveLink(const QString &target) const
{
    QString resolved = resolveTarget(target);
    if (m_vaultPath.isEmpty()) return {};
    return QUrl::fromLocalFile(m_vaultPath + QLatin1Char('/') + resolved);
}

bool VaultResourceProvider::linkExists(const QString &target) const
{
    if (!m_vault) return false;

    // resolveTarget finds the best match; verify it actually exists
    QString resolved = resolveTarget(target);
    return m_vault->getAbstractFileByPath(resolved) != nullptr;
}

} // namespace Corbomite
