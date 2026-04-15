// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/MenuEventEmitter.h"

namespace Corbomite {

MenuEventEmitter::MenuEventEmitter(QObject *parent)
    : QObject(parent)
{
}

void MenuEventEmitter::emitFileMenu(QMenu *menu, const QString &filePath)
{
    Q_EMIT fileMenu(menu, filePath);
}

void MenuEventEmitter::emitUrlMenu(QMenu *menu, const QString &url)
{
    Q_EMIT urlMenu(menu, url);
}

void MenuEventEmitter::emitEditorMenu(QMenu *menu, QObject *editor)
{
    Q_EMIT editorMenu(menu, editor);
}

void MenuEventEmitter::emitFilesMenu(QMenu *menu, const QStringList &filePaths)
{
    Q_EMIT filesMenu(menu, filePaths);
}

void MenuEventEmitter::emitLeafMenu(QMenu *menu, QObject *leaf)
{
    Q_EMIT leafMenu(menu, leaf);
}

void MenuEventEmitter::emitTabGroupMenu(QMenu *menu, QObject *tabGroup)
{
    Q_EMIT tabGroupMenu(menu, tabGroup);
}

void MenuEventEmitter::emitMarkdownViewportMenu(QMenu *menu, QObject *viewport)
{
    Q_EMIT markdownViewportMenu(menu, viewport);
}

} // namespace Corbomite
