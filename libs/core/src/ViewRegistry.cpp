// libs/core/src/ViewRegistry.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/ViewRegistry.h"

#include <stdexcept>

namespace Corbomite {

ViewRegistry::ViewRegistry(QObject *parent)
    : QObject(parent)
{
}

void ViewRegistry::registerView(const QString &type, ViewFactory factory)
{
    if (m_viewByType.contains(type))
        throw std::runtime_error(
            QStringLiteral("ViewRegistry: duplicate type '%1'").arg(type).toStdString());
    m_viewByType.insert(type, std::move(factory));
    Q_EMIT viewRegistered(type);
}

void ViewRegistry::unregisterView(const QString &type)
{
    if (m_viewByType.remove(type))
        Q_EMIT viewUnregistered(type);
}

void ViewRegistry::registerExtensions(const QStringList &exts, const QString &type)
{
    for (const auto &ext : exts) {
        if (m_typeByExtension.contains(ext))
            throw std::runtime_error(
                QStringLiteral("ViewRegistry: extension '%1' already registered")
                    .arg(ext).toStdString());
    }
    for (const auto &ext : exts)
        m_typeByExtension.insert(ext, type);
    Q_EMIT extensionsUpdated();
}

void ViewRegistry::unregisterExtensions(const QStringList &exts)
{
    for (const auto &ext : exts)
        m_typeByExtension.remove(ext);
    Q_EMIT extensionsUpdated();
}

void ViewRegistry::registerViewWithExtensions(const QStringList &exts, const QString &type,
                                              ViewFactory factory)
{
    registerView(type, std::move(factory));
    registerExtensions(exts, type);
}

ViewRegistry::ViewFactory ViewRegistry::getViewCreatorByType(const QString &type) const
{
    return m_viewByType.value(type, nullptr);
}

QString ViewRegistry::getTypeByExtension(const QString &ext) const
{
    return m_typeByExtension.value(ext);
}

bool ViewRegistry::isExtensionRegistered(const QString &ext) const
{
    return m_typeByExtension.contains(ext);
}

void ViewRegistry::setFileResolver(FileResolver resolver)
{
    m_fileResolver = std::move(resolver);
}

NoteDocument *ViewRegistry::resolveFile(const QString &relativePath) const
{
    if (m_fileResolver)
        return m_fileResolver(relativePath);
    return nullptr;
}

} // namespace Corbomite
