// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QByteArray>
#include <QHash>
#include <QJsonValue>
#include <QMetaObject>
#include <QObject>
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
///
/// A `QObject` so plugin widgets can `QObject::connect` directly to the
/// forwarded `created` / `modified` / `deletedFile` / `renamed` signals.
/// The underlying `Vault` signals are wired in the ctor only when the
/// owning plugin holds the `vault.events` permission; without that
/// permission the signals are defined but never fire.
class VaultProxy : public QObject
{
    Q_OBJECT
public:
    VaultProxy(Vault *vault, const QSet<QString> &granted, QString pluginId,
               QObject *parent = nullptr);
    ~VaultProxy() override;

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

Q_SIGNALS:
    /// Forwarded from `Vault::created`. Only fires when the owning plugin
    /// holds the `vault.events` permission (checked once at ctor time).
    void created(Corbomite::TAbstractFile *f);
    /// Forwarded from `Vault::modified`.
    void modified(Corbomite::TFile *f);
    /// Forwarded from `Vault::deletedFile`. Named to avoid the `QObject`
    /// signal `destroyed`-adjacent confusion and to mirror the underlying
    /// signal name.
    void deletedFile(Corbomite::TAbstractFile *f);
    /// Forwarded from `Vault::renamed`. The second arg is the old relative
    /// path at the time of emission.
    void renamed(Corbomite::TAbstractFile *f, const QString &oldPath);

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
