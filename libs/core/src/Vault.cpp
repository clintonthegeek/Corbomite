// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Vault.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace Corbomite {

Vault::Vault(const QString &rootPath) : m_root(QDir::cleanPath(rootPath)) {}

QString Vault::absolutePath(const QString &relative) const
{
    if (m_root.isEmpty()) return {};
    if (QFileInfo(relative).isAbsolute()) return {};

    const QString candidate = QDir::cleanPath(m_root + QLatin1Char('/') + relative);
    if (candidate == m_root) return candidate;
    if (!candidate.startsWith(m_root + QLatin1Char('/'))) return {};
    return candidate;
}

QByteArray Vault::read(const QString &relativePath) const
{
    const QString abs = absolutePath(relativePath);
    if (abs.isEmpty()) return {};
    QFile f(abs);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

bool Vault::exists(const QString &relativePath) const
{
    const QString abs = absolutePath(relativePath);
    if (abs.isEmpty()) return false;
    return QFileInfo::exists(abs);
}

QStringList Vault::list(const QString &subdir) const
{
    const QString abs = absolutePath(subdir);
    if (abs.isEmpty()) return {};
    QDir d(abs);
    if (!d.exists()) return {};
    return d.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                       QDir::Name);
}

bool Vault::create(const QString &relativePath, const QByteArray &body)
{
    const QString abs = absolutePath(relativePath);
    if (abs.isEmpty()) return false;
    if (QFileInfo::exists(abs)) return false;
    if (!QDir().mkpath(QFileInfo(abs).absolutePath())) return false;

    QSaveFile f(abs);
    if (!f.open(QIODevice::WriteOnly)) return false;
    if (f.write(body) != body.size()) {
        f.cancelWriting();
        return false;
    }
    return f.commit();
}

bool Vault::write(const QString &relativePath, const QByteArray &body)
{
    const QString abs = absolutePath(relativePath);
    if (abs.isEmpty()) return false;
    if (!QDir().mkpath(QFileInfo(abs).absolutePath())) return false;

    QSaveFile f(abs);
    if (!f.open(QIODevice::WriteOnly)) return false;
    if (f.write(body) != body.size()) {
        f.cancelWriting();
        return false;
    }
    return f.commit();
}

bool Vault::rename(const QString &oldPath, const QString &newPath)
{
    const QString from = absolutePath(oldPath);
    const QString to   = absolutePath(newPath);
    if (from.isEmpty() || to.isEmpty()) return false;
    if (!QDir().mkpath(QFileInfo(to).absolutePath())) return false;
    return QFile::rename(from, to);
}

bool Vault::remove(const QString &relativePath)
{
    const QString abs = absolutePath(relativePath);
    if (abs.isEmpty()) return false;
    return QFile::remove(abs);
}

} // namespace Corbomite
