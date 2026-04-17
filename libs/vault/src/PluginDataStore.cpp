// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/PluginDataStore.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

namespace Corbomite {

PluginDataStore::PluginDataStore(QString pluginDir)
    : m_pluginDir(std::move(pluginDir)) {}

QString PluginDataStore::dataFilePath() const
{
    return m_pluginDir + QStringLiteral("/data.json");
}

QJsonObject PluginDataStore::load() const
{
    QFile f(dataFilePath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray bytes = f.readAll();
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object();
}

bool PluginDataStore::save(const QJsonObject &obj)
{
    QDir().mkpath(m_pluginDir);
    QSaveFile f(dataFilePath());
    if (!f.open(QIODevice::WriteOnly)) return false;
    const auto bytes = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if (f.write(bytes) != bytes.size()) { f.cancelWriting(); return false; }
    return f.commit();
}

} // namespace Corbomite
