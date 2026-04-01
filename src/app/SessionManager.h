// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QTimer>

class QSplitter;

namespace Corbomite {
class EditorViewSpace;

class SessionManager : public QObject {
    Q_OBJECT

public:
    explicit SessionManager(QObject *parent = nullptr);

    void setSessionPath(const QString &path);

    // Save
    void saveWindowGeometry(const QByteArray &geometry, const QByteArray &state);
    void saveSidebarState(bool leftVisible, int leftWidth, bool rightVisible, int rightWidth,
                          const QString &activePanel = QString());
    void saveExpandedFolders(const QStringList &folders);
    void saveEditorState(const QJsonObject &editorState);
    void scheduleSave();
    void saveNow();

    // Load
    QJsonObject load() const;

    // Convenience accessors for loaded data
    QJsonObject editorState() const;
    QJsonObject sidebarState() const;
    QStringList expandedFolders() const;
    QByteArray windowGeometry() const;
    QByteArray windowState() const;

    // Block saves during vault transitions
    void blockSaving();
    void unblockSaving();

    // Static helpers for building editor state JSON
    static QJsonObject buildSplitLayoutJson(QSplitter *splitter, const QVector<EditorViewSpace *> &viewSpaces);
    static QJsonValue encodeSplitterNode(QSplitter *splitter, const QVector<EditorViewSpace *> &viewSpaces);

private:

    void doSave();

    QString m_sessionPath;
    QJsonObject m_data;
    QTimer m_saveTimer;
    int m_saveBlockCount = 0;
};

} // namespace Corbomite
