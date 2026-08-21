// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterPropertyInfo.h"

#include "corbomite/bases/Value.h"

#include <algorithm>

namespace Corbomite::Bases {

namespace {
constexpr int kMaxSampleRows = 500;  // cap the scan; a type match usually lands in the first few rows
}

QVector<FilterPropertyInfo> buildFilterPropertyInfos(
    const QVector<PropertyId> &props,
    const QVector<std::shared_ptr<BasesEntry>> &sampleRows,
    const std::function<QString(const PropertyId &)> &displayNameFor)
{
    QVector<FilterPropertyInfo> out;
    out.reserve(props.size());
    const qsizetype rowCap = std::min<qsizetype>(sampleRows.size(), kMaxSampleRows);

    for (const PropertyId &id : props) {
        FilterPropertyInfo info;
        info.id = id;
        info.displayName = displayNameFor ? displayNameFor(id) : id.toString();
        info.valueType = QStringLiteral("String");  // default: widest operator set

        for (qsizetype i = 0; i < rowCap; ++i) {
            const auto &row = sampleRows.at(i);
            if (!row) continue;
            const ValuePtr v = row->getValue(id);
            if (v && !v->isEmpty()) {
                info.valueType = v->type();
                break;
            }
        }
        out.push_back(info);
    }
    return out;
}

}  // namespace Corbomite::Bases
