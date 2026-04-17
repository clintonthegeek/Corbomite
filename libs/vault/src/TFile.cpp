// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/TFile.h"

#include <QFileInfo>

namespace Corbomite {

namespace {
void deriveBasenameExt(const QString &name, QString &basename, QString &ext)
{
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0) {
        basename = name;
        ext.clear();
    } else {
        basename = name.left(dot);
        ext = name.mid(dot + 1).toLower();
    }
}
}

TFile::TFile(Vault *v, const QString &p)
    : TAbstractFile(v, p)
{
    deriveBasenameExt(name, basename, extension);
}

void TFile::setPath(const QString &newPath)
{
    TAbstractFile::setPath(newPath);
    deriveBasenameExt(name, basename, extension);
}

QString TFile::getShortName() const
{
    return extension == QStringLiteral("md") ? basename : name;
}

} // namespace Corbomite
