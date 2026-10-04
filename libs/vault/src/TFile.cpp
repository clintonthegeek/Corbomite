// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/TFile.h"

#include <QFileInfo>

namespace Corbomite {

namespace {
// `Fl`/`Ll` (app/own/0365-Fl.js:2, 0363-Ll.js:2): split on the last `.`,
// but a leading dot, a trailing dot, or no dot at all yields extension `""`
// and basename `== the whole name`. So `.gitignore` → basename
// `".gitignore"`, extension `""`; `Note.` → basename `"Note."`, extension
// `""`; `Note.md.bak` → basename `"Note.md"`, extension `"bak"`.
void deriveBasenameExt(const QString &name, QString &basename, QString &ext)
{
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0 || dot == name.length() - 1) {
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
