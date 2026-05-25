// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/ViewConfigOps.h"

#include "corbomite/bases/BasesQuery.h"

namespace Corbomite::Bases {

namespace {
int indexOfProp(const QVector<PropertyId> &v, const PropertyId &p) {
    for (int i = 0; i < v.size(); ++i) if (v[i] == p) return i;
    return -1;
}
int indexOfSort(const QVector<SortKey> &v, const PropertyId &p) {
    for (int i = 0; i < v.size(); ++i) if (v[i].property == p) return i;
    return -1;
}
int indexOfView(const std::vector<std::unique_ptr<BasesViewConfig>> &views, const QString &name) {
    for (int i = 0; i < int(views.size()); ++i) if (views[i] && views[i]->name == name) return i;
    return -1;
}
}  // namespace

void setColumnVisible(QVector<PropertyId> &order, const PropertyId &pid, bool visible,
                      const QVector<PropertyId> &allProps)
{
    const int at = indexOfProp(order, pid);
    if (visible) {
        if (at >= 0) return;
        const int canonical = indexOfProp(allProps, pid);
        int insertAt = order.size();
        if (canonical >= 0) {
            for (int i = 0; i < order.size(); ++i) {
                if (indexOfProp(allProps, order[i]) > canonical) { insertAt = i; break; }
            }
        }
        order.insert(insertAt, pid);
    } else {
        if (at >= 0) order.remove(at);
    }
}

void moveColumn(QVector<PropertyId> &order, int from, int to)
{
    if (from < 0 || from >= order.size()) return;
    if (to < 0) to = 0;
    if (to >= order.size()) to = order.size() - 1;
    if (from == to) return;
    order.move(from, to);
}

void hideAllColumns(QVector<PropertyId> &order) { order.clear(); }

void addSortKey(QVector<SortKey> &sort, const PropertyId &pid, const QString &dir)
{
    if (indexOfSort(sort, pid) >= 0) return;
    sort.push_back({pid, dir});
}

void setSortDirection(QVector<SortKey> &sort, const PropertyId &pid, const QString &dir)
{
    const int i = indexOfSort(sort, pid);
    if (i >= 0) sort[i].direction = dir;
    else sort.push_back({pid, dir});
}

void removeSortKey(QVector<SortKey> &sort, const PropertyId &pid)
{
    const int i = indexOfSort(sort, pid);
    if (i >= 0) sort.remove(i);
}

void setGroupBy(BasesViewConfig &cfg, const std::optional<PropertyId> &pid, const QString &dir)
{
    if (pid.has_value()) cfg.groupBy = GroupBy{*pid, dir};
    else cfg.groupBy.reset();
}

bool duplicateView(BasesQuery &q, const QString &name, const QString &newName)
{
    if (indexOfView(q.views, newName) >= 0) return false;
    const int src = indexOfView(q.views, name);
    if (src < 0) return false;
    auto copy = std::make_unique<BasesViewConfig>(*q.views[src]);
    copy->name = newName;
    q.views.push_back(std::move(copy));
    return true;
}

bool deleteView(BasesQuery &q, const QString &name)
{
    if (q.views.size() <= 1) return false;
    const int i = indexOfView(q.views, name);
    if (i < 0) return false;
    q.views.erase(q.views.begin() + i);
    return true;
}

bool renameView(BasesQuery &q, const QString &oldName, const QString &newName)
{
    if (indexOfView(q.views, newName) >= 0) return false;
    const int i = indexOfView(q.views, oldName);
    if (i < 0) return false;
    q.views[i]->name = newName;
    return true;
}

bool setDefaultView(BasesQuery &q, const QString &name)
{
    const int i = indexOfView(q.views, name);
    if (i < 0) return false;
    if (i == 0) return true;
    auto v = std::move(q.views[i]);
    q.views.erase(q.views.begin() + i);
    q.views.insert(q.views.begin(), std::move(v));
    return true;
}

}  // namespace Corbomite::Bases
