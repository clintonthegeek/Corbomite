// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QByteArray>
#include <QHash>
#include <QJsonValue>
#include <QMetaObject>
#include <QSet>
#include <QString>
#include <QUuid>
#include <QVector>

namespace Corbomite {

class Vault;
class TAbstractFile;
class TFile;
class TFolder;

/// Permission-gated plugin-facing Vault facade. Methods return empty /
/// false / nullptr / null QUuid when the caller lacks the required
/// permission token. Tokens: `vault.read`, `vault.write`, `vault.events`.
class VaultProxy
{
public:
    VaultProxy(Vault *vault, const QSet<QString> &granted, QString pluginId);
    ~VaultProxy();

    VaultProxy(const VaultProxy &) = delete;
    VaultProxy &operator=(const VaultProxy &) = delete;

    // ---- Read (gated by vault.read) ----
    QByteArray     read(TFile *f) const;
    QByteArray     cachedRead(TFile *f) const;
    QByteArray     readBinary(TFile *f) const;
    bool           exists(const QString &path) const;
    TFile         *getFileByPath(const QString &path) const;
    TFolder       *getFolderByPath(const QString &path) const;
    TAbstractFile *getAbstractFileByPath(const QString &path) const;
    QVector<TFile *> getMarkdownFiles() const;
    QVector<TFile *> getFiles() const;
    TFolder       *getRoot() const;
    QString        getName() const;

    // ---- Mutation (gated by vault.write) ----
    bool     modify(TFile *f, const QByteArray &body);
    bool     modifyBinary(TFile *f, const QByteArray &body);
    bool     append(TFile *f, const QByteArray &body);
    bool     process(TFile *f,
                     std::function<QByteArray(const QByteArray &)> mutator);
    TFile   *create(const QString &path, const QByteArray &body);
    TFolder *createFolder(const QString &path);
    bool     rename(TAbstractFile *f, const QString &newPath);
    bool     trash(TAbstractFile *f, bool useSystem);
    bool     remove(TAbstractFile *f);

    // ---- Events (gated by vault.events) ----
    using EventFn = std::function<void(TAbstractFile *)>;
    /// Subscribe to `create` / `modify` / `delete` / `rename`.
    /// Returns a null QUuid if the permission is missing or the event name
    /// is unknown.
    QUuid on(const QString &event, EventFn fn);
    void  off(const QUuid &token);

    // ---- Config JSON (read gated by vault.read; write by vault.write) ----
    QJsonValue readConfigJson(const QString &name) const;
    bool       writeConfigJson(const QString &name, const QJsonValue &v);
    bool       deleteConfigJson(const QString &name);

private:
    Vault                                 *m_vault;
    QSet<QString>                          m_granted;
    QString                                m_pluginId;
    QHash<QUuid, QMetaObject::Connection>  m_subscriptions;

    bool canRead() const
    {
        return m_granted.contains(QStringLiteral("vault.read"));
    }
    bool canWrite() const
    {
        return m_granted.contains(QStringLiteral("vault.write"));
    }
    bool canEvents() const
    {
        return m_granted.contains(QStringLiteral("vault.events"));
    }

    void logDenied(const char *method, const char *requiredToken) const;
};

}  // namespace Corbomite
