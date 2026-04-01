// SPDX-License-Identifier: GPL-3.0-or-later
#include "SessionManager.h"
#include "editor/EditorViewSpace.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QSplitter>

namespace Corbomite {

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(2000);
    connect(&m_saveTimer, &QTimer::timeout, this, &SessionManager::doSave);
}

void SessionManager::setSessionPath(const QString &path)
{
    m_sessionPath = path;
}

void SessionManager::saveWindowGeometry(const QByteArray &geometry, const QByteArray &state)
{
    m_data[QStringLiteral("windowGeometry")] = QString::fromLatin1(geometry.toBase64());
    m_data[QStringLiteral("windowState")] = QString::fromLatin1(state.toBase64());
    scheduleSave();
}

void SessionManager::saveSidebarState(bool leftVisible, int leftWidth, bool rightVisible, int rightWidth,
                                       const QString &activePanel)
{
    QJsonObject sidebar;
    sidebar[QStringLiteral("leftVisible")] = leftVisible;
    sidebar[QStringLiteral("leftWidth")] = leftWidth;
    sidebar[QStringLiteral("rightVisible")] = rightVisible;
    sidebar[QStringLiteral("rightWidth")] = rightWidth;
    if (!activePanel.isEmpty()) {
        sidebar[QStringLiteral("activePanel")] = activePanel;
    }
    m_data[QStringLiteral("sidebar")] = sidebar;
    scheduleSave();
}

void SessionManager::saveExpandedFolders(const QStringList &folders)
{
    m_data[QStringLiteral("expandedFolders")] = QJsonArray::fromStringList(folders);
    scheduleSave();
}

void SessionManager::saveEditorState(const QJsonObject &editorState)
{
    m_data[QStringLiteral("editor")] = editorState;
    scheduleSave();
}

void SessionManager::scheduleSave()
{
    if (m_saveBlockCount > 0) return;
    m_saveTimer.start();
}

void SessionManager::saveNow()
{
    m_saveTimer.stop();
    doSave();
}

QJsonObject SessionManager::load() const
{
    QFile file(m_sessionPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    auto doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject();
}

QJsonObject SessionManager::editorState() const
{
    return m_data[QStringLiteral("editor")].toObject();
}

QJsonObject SessionManager::sidebarState() const
{
    return m_data[QStringLiteral("sidebar")].toObject();
}

QStringList SessionManager::expandedFolders() const
{
    QStringList result;
    auto arr = m_data[QStringLiteral("expandedFolders")].toArray();
    for (const auto &v : arr) {
        result.append(v.toString());
    }
    return result;
}

QByteArray SessionManager::windowGeometry() const
{
    return QByteArray::fromBase64(
        m_data[QStringLiteral("windowGeometry")].toString().toLatin1());
}

QByteArray SessionManager::windowState() const
{
    return QByteArray::fromBase64(
        m_data[QStringLiteral("windowState")].toString().toLatin1());
}

void SessionManager::blockSaving()
{
    ++m_saveBlockCount;
    m_saveTimer.stop();
}

void SessionManager::unblockSaving()
{
    if (m_saveBlockCount > 0) {
        --m_saveBlockCount;
    }
}

QJsonObject SessionManager::buildSplitLayoutJson(QSplitter *splitter, const QVector<EditorViewSpace *> &viewSpaces)
{
    QJsonObject result;
    result[QStringLiteral("splitLayout")] = encodeSplitterNode(splitter, viewSpaces);
    return result;
}

QJsonValue SessionManager::encodeSplitterNode(QSplitter *splitter, const QVector<EditorViewSpace *> &viewSpaces)
{
    if (!splitter) return QJsonValue();

    if (splitter->count() == 1) {
        if (auto *space = qobject_cast<EditorViewSpace *>(splitter->widget(0))) {
            int idx = viewSpaces.indexOf(space);
            return QStringLiteral("pane:%1").arg(idx);
        }
        if (auto *childSplitter = qobject_cast<QSplitter *>(splitter->widget(0))) {
            return encodeSplitterNode(childSplitter, viewSpaces);
        }
    }

    QJsonObject node;
    node[QStringLiteral("orientation")] =
        splitter->orientation() == Qt::Horizontal
            ? QStringLiteral("horizontal")
            : QStringLiteral("vertical");

    QJsonArray sizes;
    for (int s : splitter->sizes()) {
        sizes.append(s);
    }
    node[QStringLiteral("sizes")] = sizes;

    QJsonArray children;
    for (int i = 0; i < splitter->count(); ++i) {
        QWidget *child = splitter->widget(i);
        if (auto *space = qobject_cast<EditorViewSpace *>(child)) {
            int idx = viewSpaces.indexOf(space);
            children.append(QStringLiteral("pane:%1").arg(idx));
        } else if (auto *childSplitter = qobject_cast<QSplitter *>(child)) {
            children.append(encodeSplitterNode(childSplitter, viewSpaces));
        }
    }
    node[QStringLiteral("children")] = children;

    return node;
}

void SessionManager::doSave()
{
    if (m_sessionPath.isEmpty()) return;

    QDir().mkpath(QFileInfo(m_sessionPath).absolutePath());
    QFile file(m_sessionPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(m_data).toJson(QJsonDocument::Indented));
    }
}

} // namespace Corbomite
