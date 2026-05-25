// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/SortCycle.h"

namespace Corbomite::Bases {

namespace {
// ASC -> DESC -> (empty = remove)
QString nextDir(const QString &d) {
    return d == QLatin1String("ASC") ? QStringLiteral("DESC") : QString{};
}
int indexOf(const QVector<SortKey> &s, const PropertyId &p) {
    for (int i = 0; i < s.size(); ++i) if (s[i].property == p) return i;
    return -1;
}
}  // namespace

void cycleHeaderSort(QVector<SortKey> &sort, const PropertyId &clicked, bool shiftHeld)
{
    if (shiftHeld) {
        const int i = indexOf(sort, clicked);
        if (i < 0) { sort.push_back({clicked, QStringLiteral("ASC")}); return; }
        const QString nd = nextDir(sort[i].direction);
        if (nd.isEmpty()) sort.remove(i);
        else sort[i].direction = nd;
        return;
    }
    // Plain click.
    if (!sort.isEmpty() && sort.front().property == clicked) {
        const QString nd = nextDir(sort.front().direction);
        sort.clear();
        if (!nd.isEmpty()) sort.push_back({clicked, nd});
        return;
    }
    sort.clear();
    sort.push_back({clicked, QStringLiteral("ASC")});
}

}  // namespace Corbomite::Bases
