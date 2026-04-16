// libs/core/include/corbomite/core/ViewRegistry.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QHash>
#include <QStringList>
#include <functional>

namespace Corbomite {

class NoteDocument;
class View;
class WorkspaceLeaf;

class ViewRegistry : public QObject
{
    Q_OBJECT

public:
    using ViewFactory = std::function<View *(WorkspaceLeaf *)>;
    using FileResolver = std::function<NoteDocument *(const QString &relativePath)>;

    explicit ViewRegistry(QObject *parent = nullptr);

    void setFileResolver(FileResolver resolver);
    NoteDocument *resolveFile(const QString &relativePath) const;

    void registerView(const QString &type, ViewFactory factory);
    void unregisterView(const QString &type);

    void registerExtensions(const QStringList &exts, const QString &type);
    void unregisterExtensions(const QStringList &exts);

    void registerViewWithExtensions(const QStringList &exts, const QString &type,
                                    ViewFactory factory);

    ViewFactory getViewCreatorByType(const QString &type) const;
    QString getTypeByExtension(const QString &ext) const;
    bool isExtensionRegistered(const QString &ext) const;

Q_SIGNALS:
    void viewRegistered(const QString &type);
    void viewUnregistered(const QString &type);
    void extensionsUpdated();

private:
    QHash<QString, ViewFactory> m_viewByType;
    QHash<QString, QString> m_typeByExtension;
    FileResolver m_fileResolver;
};

} // namespace Corbomite
