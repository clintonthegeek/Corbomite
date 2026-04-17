// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Value.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <functional>
#include <memory>

namespace Corbomite {
class TFile;
class Vault;
class MetadataCache;
}  // namespace Corbomite

namespace Corbomite::Bases {

/// Singleton "no value". `NullValue::instance()` is the only way to
/// obtain one — constructor is private.
class NullValue : public Value
{
public:
    static ValuePtr instance();

    QString type() const override { return QStringLiteral("Null"); }
    bool isTruthy() const override { return false; }
    bool isEmpty() const override { return true; }
    QString toString() const override { return {}; }

private:
    NullValue() = default;
    friend struct NullValueAccess;
};

}  // namespace Corbomite::Bases
