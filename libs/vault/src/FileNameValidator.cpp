// SPDX-License-Identifier: GPL-3.0-or-later
#include "FileNameValidator.h"

#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/vault/Vault.h"

#include <KLocalizedString>

#include <QRegularExpression>
#include <QSet>

namespace Corbomite {

namespace {

const QSet<QString> &windowsReservedBasenames()
{
    static const QSet<QString> reserved = {
        QStringLiteral("CON"), QStringLiteral("PRN"),
        QStringLiteral("AUX"), QStringLiteral("NUL"),
        QStringLiteral("COM1"), QStringLiteral("COM2"), QStringLiteral("COM3"),
        QStringLiteral("COM4"), QStringLiteral("COM5"), QStringLiteral("COM6"),
        QStringLiteral("COM7"), QStringLiteral("COM8"), QStringLiteral("COM9"),
        QStringLiteral("LPT1"), QStringLiteral("LPT2"), QStringLiteral("LPT3"),
        QStringLiteral("LPT4"), QStringLiteral("LPT5"), QStringLiteral("LPT6"),
        QStringLiteral("LPT7"), QStringLiteral("LPT8"), QStringLiteral("LPT9"),
    };
    return reserved;
}

}  // namespace

QString validateFileName(const QString &newName,
                         const TAbstractFile *sourceFile,
                         const Vault *vault,
                         bool isFinal)
{
    if (newName.isEmpty())
        return i18n("A file name is required.");

    // Bad characters.
    static const QRegularExpression bad(QStringLiteral(R"([\\/:*?"<>|])"));
    if (bad.match(newName).hasMatch()) {
        return i18n(
            "File name cannot contain any of the following characters: "
            "\\ / : * ? \" < > |");
    }

    // Reserved Windows basenames (basename-before-extension check).
    const QString basename = newName.section(QLatin1Char('.'), 0, 0).toUpper();
    if (windowsReservedBasenames().contains(basename)) {
#ifdef Q_OS_WIN
        return i18n("That file name is reserved. Please use a different name.");
#else
        if (isFinal) {
            return i18n(
                "That file name is reserved on Windows. Please use a "
                "different name.");
        }
#endif
    }

    // Collision detection.
    if (vault) {
        // Resolve the parent-prefix: for a detached file (no parent yet),
        // treat it as vault-root. TFolder::getParentPrefix() returns "" for
        // the root folder and "<path>/" for non-root folders.
        QString parentPrefix;
        if (sourceFile && sourceFile->parent)
            parentPrefix = sourceFile->parent->getParentPrefix();

        const QString candidatePath = parentPrefix + newName;
        const TAbstractFile *existing =
            vault->getAbstractFileByPath(candidatePath);
        if (existing && existing != sourceFile)
            return i18n("A file with this name already exists.");
    }

    return QString();
}

}  // namespace Corbomite
