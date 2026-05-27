// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaOps.h"

namespace Corbomite::Bases::FormulaOps {

bool add(QHash<QString, Formula> &map, QStringList &order,
         const QString &name, const QString &source)
{
    if (name.isEmpty() || map.contains(name)) return false;
    map.insert(name, Formula(source));
    order.append(name);
    return true;
}

bool rename(QHash<QString, Formula> &map, QStringList &order,
            const QString &oldName, const QString &newName)
{
    if (oldName == newName) return true;
    if (newName.isEmpty() || !map.contains(oldName) || map.contains(newName))
        return false;
    // The map/order invariant (every key appears once in `order`) must hold
    // before we touch the map — bail with no mutation if it is somehow broken,
    // rather than silently appending and corrupting insertion order.
    const int idx = order.indexOf(oldName);
    if (idx < 0) return false;
    map.insert(newName, map.take(oldName));
    order[idx] = newName;
    return true;
}

bool setSource(QHash<QString, Formula> &map,
               const QString &name, const QString &source)
{
    if (!map.contains(name)) return false;
    map.insert(name, Formula(source));
    return true;
}

bool remove(QHash<QString, Formula> &map, QStringList &order,
            const QString &name)
{
    if (!map.contains(name)) return false;
    map.remove(name);
    order.removeAll(name);
    return true;
}

}  // namespace Corbomite::Bases::FormulaOps
