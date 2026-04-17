// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/proxies/VaultProxy.h"

#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/vault/Vault.h"

#include <QLoggingCategory>

namespace Corbomite {

namespace {
Q_LOGGING_CATEGORY(lcPluginVault, "corbomite.plugin.vault")
}

VaultProxy::VaultProxy(Vault *vault, const QSet<QString> &granted,
                       QString pluginId, QObject *parent)
    : QObject(parent),
      m_vault(vault),
      m_granted(granted),
      m_pluginId(std::move(pluginId))
{
    if (m_vault && canEvents()) {
        connect(m_vault, &Vault::created, this, &VaultProxy::created);
        connect(m_vault, &Vault::modified, this, &VaultProxy::modified);
        connect(m_vault, &Vault::deletedFile, this, &VaultProxy::deletedFile);
        connect(m_vault, &Vault::renamed, this, &VaultProxy::renamed);
    }
}

VaultProxy::~VaultProxy()
{
    for (const auto &c : std::as_const(m_subscriptions))
        QObject::disconnect(c);
}

void VaultProxy::logDenied(const char *method, const char *req) const
{
    qCWarning(lcPluginVault) << "plugin" << m_pluginId << "denied" << method
                             << "— missing" << req;
}

QByteArray VaultProxy::read(TFile *f) const
{
    if (!canRead()) {
        logDenied("read", "vault.read");
        return {};
    }
    return m_vault ? m_vault->read(f) : QByteArray{};
}

QByteArray VaultProxy::cachedRead(TFile *f) const
{
    if (!canRead()) {
        logDenied("cachedRead", "vault.read");
        return {};
    }
    return m_vault ? m_vault->cachedRead(f) : QByteArray{};
}

QByteArray VaultProxy::readBinary(TFile *f) const
{
    if (!canRead()) {
        logDenied("readBinary", "vault.read");
        return {};
    }
    return m_vault ? m_vault->readBinary(f) : QByteArray{};
}

bool VaultProxy::exists(const QString &path) const
{
    if (!canRead()) {
        logDenied("exists", "vault.read");
        return false;
    }
    return m_vault && m_vault->getAbstractFileByPath(path) != nullptr;
}

TFile *VaultProxy::getFileByPath(const QString &path) const
{
    if (!canRead()) {
        logDenied("getFileByPath", "vault.read");
        return nullptr;
    }
    return m_vault ? m_vault->getFileByPath(path) : nullptr;
}

TFolder *VaultProxy::getFolderByPath(const QString &path) const
{
    if (!canRead()) {
        logDenied("getFolderByPath", "vault.read");
        return nullptr;
    }
    return m_vault ? m_vault->getFolderByPath(path) : nullptr;
}

TAbstractFile *VaultProxy::getAbstractFileByPath(const QString &path) const
{
    if (!canRead()) {
        logDenied("getAbstractFileByPath", "vault.read");
        return nullptr;
    }
    return m_vault ? m_vault->getAbstractFileByPath(path) : nullptr;
}

QVector<TFile *> VaultProxy::getMarkdownFiles() const
{
    if (!canRead()) {
        logDenied("getMarkdownFiles", "vault.read");
        return {};
    }
    return m_vault ? m_vault->getMarkdownFiles() : QVector<TFile *>{};
}

QVector<TFile *> VaultProxy::getFiles() const
{
    if (!canRead()) {
        logDenied("getFiles", "vault.read");
        return {};
    }
    return m_vault ? m_vault->getFiles() : QVector<TFile *>{};
}

TFolder *VaultProxy::getRoot() const
{
    if (!canRead()) {
        logDenied("getRoot", "vault.read");
        return nullptr;
    }
    return m_vault ? m_vault->getRoot() : nullptr;
}

QString VaultProxy::getName() const
{
    if (!canRead()) {
        logDenied("getName", "vault.read");
        return {};
    }
    return m_vault ? m_vault->getName() : QString{};
}

QString VaultProxy::basePath() const
{
    if (!canRead()) {
        logDenied("basePath", "vault.read");
        return {};
    }
    return m_vault ? m_vault->basePath() : QString{};
}

bool VaultProxy::modify(TFile *f, const QByteArray &body)
{
    if (!canWrite()) {
        logDenied("modify", "vault.write");
        return false;
    }
    return m_vault && m_vault->modify(f, body);
}

bool VaultProxy::modifyBinary(TFile *f, const QByteArray &body)
{
    if (!canWrite()) {
        logDenied("modifyBinary", "vault.write");
        return false;
    }
    return m_vault && m_vault->modifyBinary(f, body);
}

bool VaultProxy::append(TFile *f, const QByteArray &body)
{
    if (!canWrite()) {
        logDenied("append", "vault.write");
        return false;
    }
    return m_vault && m_vault->append(f, body);
}

bool VaultProxy::process(TFile *f,
                         std::function<QByteArray(const QByteArray &)> mut)
{
    if (!canWrite()) {
        logDenied("process", "vault.write");
        return false;
    }
    return m_vault && m_vault->process(f, std::move(mut));
}

TFile *VaultProxy::create(const QString &path, const QByteArray &body)
{
    if (!canWrite()) {
        logDenied("create", "vault.write");
        return nullptr;
    }
    return m_vault ? m_vault->create(path, body) : nullptr;
}

TFolder *VaultProxy::createFolder(const QString &path)
{
    if (!canWrite()) {
        logDenied("createFolder", "vault.write");
        return nullptr;
    }
    return m_vault ? m_vault->createFolder(path) : nullptr;
}

bool VaultProxy::rename(TAbstractFile *f, const QString &newPath)
{
    if (!canWrite()) {
        logDenied("rename", "vault.write");
        return false;
    }
    return m_vault && m_vault->rename(f, newPath);
}

bool VaultProxy::trash(TAbstractFile *f, bool useSystem)
{
    if (!canWrite()) {
        logDenied("trash", "vault.write");
        return false;
    }
    return m_vault && m_vault->trash(f, useSystem);
}

bool VaultProxy::remove(TAbstractFile *f)
{
    if (!canWrite()) {
        logDenied("remove", "vault.write");
        return false;
    }
    return m_vault && m_vault->remove(f);
}

QUuid VaultProxy::on(const QString &event, EventFn fn)
{
    if (!canEvents()) {
        logDenied("on", "vault.events");
        return {};
    }
    if (!m_vault) return {};
    QMetaObject::Connection c;
    if (event == QStringLiteral("create")) {
        c = QObject::connect(m_vault, &Vault::created, fn);
    } else if (event == QStringLiteral("modify")) {
        c = QObject::connect(m_vault, &Vault::modified,
                             [fn](TFile *f) { fn(f); });
    } else if (event == QStringLiteral("delete")) {
        c = QObject::connect(m_vault, &Vault::deletedFile, fn);
    } else if (event == QStringLiteral("rename")) {
        c = QObject::connect(m_vault, &Vault::renamed,
                             [fn](TAbstractFile *f, const QString &) {
                                 fn(f);
                             });
    } else {
        return {};
    }
    QUuid token = QUuid::createUuid();
    m_subscriptions.insert(token, c);
    return token;
}

void VaultProxy::off(const QUuid &token)
{
    auto it = m_subscriptions.find(token);
    if (it == m_subscriptions.end()) return;
    QObject::disconnect(it.value());
    m_subscriptions.erase(it);
}

QJsonValue VaultProxy::readConfigJson(const QString &name) const
{
    if (!canRead()) {
        logDenied("readConfigJson", "vault.read");
        return {};
    }
    return m_vault ? m_vault->readConfigJson(name) : QJsonValue{};
}

bool VaultProxy::writeConfigJson(const QString &name, const QJsonValue &v)
{
    if (!canWrite()) {
        logDenied("writeConfigJson", "vault.write");
        return false;
    }
    return m_vault && m_vault->writeConfigJson(name, v);
}

bool VaultProxy::deleteConfigJson(const QString &name)
{
    if (!canWrite()) {
        logDenied("deleteConfigJson", "vault.write");
        return false;
    }
    return m_vault && m_vault->deleteConfigJson(name);
}

}  // namespace Corbomite
