// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite {

class DiffMatchPatch
{
public:
    static QString threeWayMerge(const QString &base,
                                 const QString &local,
                                 const QString &remote);
};

} // namespace Corbomite
