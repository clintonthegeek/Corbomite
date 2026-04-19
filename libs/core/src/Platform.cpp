// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Platform.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#endif

namespace Corbomite::Platform {

bool openWithDefaultApp(const QString &absolutePath)
{
    const QFileInfo info(absolutePath);
    if (!info.exists()) return false;

    const QUrl url = QUrl::fromLocalFile(info.absoluteFilePath());
    return QDesktopServices::openUrl(url);
}

bool showInFolder(const QString &absolutePath)
{
    const QFileInfo info(absolutePath);
    if (!info.exists()) return false;

    const QString abs = info.absoluteFilePath();

#if defined(Q_OS_MACOS)
    return QProcess::startDetached(QStringLiteral("open"),
                                   {QStringLiteral("-R"), abs});
#elif defined(Q_OS_WIN)
    // explorer /select,<path> — note: Windows Explorer parses the
    // comma-joined argument literally, so pass it as a single token.
    const QString nativeAbs = QDir::toNativeSeparators(abs);
    return QProcess::startDetached(
        QStringLiteral("explorer"),
        {QStringLiteral("/select,") + nativeAbs});
#elif defined(Q_OS_LINUX)
    // Preferred: DBus FileManager1 — honoured by every major Linux FM.
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        QDBusInterface fm1(QStringLiteral("org.freedesktop.FileManager1"),
                           QStringLiteral("/org/freedesktop/FileManager1"),
                           QStringLiteral("org.freedesktop.FileManager1"),
                           bus);
        if (fm1.isValid()) {
            const QString uri = QUrl::fromLocalFile(abs).toString();
            QDBusReply<void> reply = fm1.call(
                QStringLiteral("ShowItems"),
                QStringList{uri},
                QStringLiteral("corbomite"));
            if (reply.isValid()) return true;
        }
    }
    // Fallback: open parent folder via xdg-open (selection is lost).
    const QString parent = info.absolutePath();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(parent));
#else
    // Unknown platform — fall back to opening the parent folder.
    const QString parent = info.absolutePath();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(parent));
#endif
}

}  // namespace Corbomite::Platform
