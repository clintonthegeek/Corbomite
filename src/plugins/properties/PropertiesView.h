// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/models/PropertyType.h"

#include <QJsonObject>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;
class QVBoxLayout;

namespace Markoff { class YamlValue; }

namespace Corbomite {

class FileManagerProxy;
class MetadataCacheReader;
class PropertyRow;
class VaultProxy;
class WorkspaceController;

/// Plugin-side properties panel — frontmatter editor for the active note.
///
/// Reads the current frontmatter from MetadataCacheReader::frontmatterFor;
/// writes back wholesale via FileManagerProxy::setFrontMatter on a 500ms
/// debounce. Suppresses external-edit refresh while a user edit is pending.
/// Reactive to MetadataCacheReader::cacheChanged for the active file +
/// WorkspaceController::activeFileChanged for note switches.
class PropertiesView : public QWidget
{
    Q_OBJECT
public:
    PropertiesView(MetadataCacheReader *metadata,
                   VaultProxy *vault,
                   FileManagerProxy *fileManager,
                   WorkspaceController *workspace,
                   QWidget *parent = nullptr);
    ~PropertiesView() override;

    int rowCount() const;
    void flushPendingWrite();

    // Interaction API (UI wrappers + tests call these).
    void addProperty(const QString &name, PropertyType type);
    bool renameProperty(const QString &oldKey, const QString &newKey);
    void deleteProperty(const QString &key);
    void moveProperty(int from, int to);

    // Test seams.
    void setActiveFileForTest(const QString &path) { onActiveFileChanged(path); }
    void setRowValueForTest(const QString &key, const QString &text);

private Q_SLOTS:
    void onActiveFileChanged(const QString &path);
    void onCacheChanged(const QString &path);
    void onAddPropertyClicked();

private:
    void refresh();
    void rebuildFromFrontmatter(const QJsonObject &fm);
    void appendRow(const QString &key, PropertyType type,
                   const Markoff::YamlValue &value, bool editable);
    void connectRow(PropertyRow *row);
    void clearRows();
    void scheduleWrite();
    void flushWrite();
    int  indexOfKey(const QString &key) const;
    bool keyExists(const QString &key) const;  // case-insensitive

    MetadataCacheReader *m_metadata = nullptr;
    VaultProxy *m_vaultProxy = nullptr;
    FileManagerProxy *m_fmProxy = nullptr;
    WorkspaceController *m_workspace = nullptr;

    QLabel *m_headerLabel;
    QLabel *m_emptyLabel;
    QWidget *m_rowsContainer = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    QPushButton *m_addPropertyButton;
    QTimer *m_writeDebounce;

    QString m_currentPath;

    QVector<PropertyRow *> m_rows;
};

} // namespace Corbomite
