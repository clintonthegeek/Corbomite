// SPDX-License-Identifier: GPL-3.0-or-later
#include "PathNormalization.h"

namespace Corbomite::VaultPaths {

QString normalize(const QString &input)
{
    QString s = input;
    s.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (s.contains(QStringLiteral("//"))) {
        s.replace(QStringLiteral("//"), QStringLiteral("/"));
    }
    if (s.length() > 1 && s.endsWith(QLatin1Char('/'))) {
        s.chop(1);
    }
    if (s.startsWith(QStringLiteral("./"))) s.remove(0, 2);
    return s.normalized(QString::NormalizationForm_C);
}

} // namespace Corbomite::VaultPaths
