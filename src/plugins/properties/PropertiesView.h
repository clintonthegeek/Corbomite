// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QVector>
#include <QWidget>

class QFormLayout;
class QLabel;
class QPushButton;
class QTimer;

namespace Corbomite {

class FileManagerProxy;
class MetadataCacheReader;
class PropertyEditorWidget;
class VaultProxy;
class WorkspaceController;

/// Plugin-side properties panel — frontmatter editor for the active note.
///
/// Reads the current frontmatter from MetadataCacheReader::frontmatterFor;
/// writes back atomically via FileManagerProxy::processFrontMatter on a
/// 500ms debounce. Suppresses external-edit refresh while a user edit is
/// pending. Reactive to MetadataCacheReader::cacheChanged for the active
/// file + WorkspaceController::activeFileChanged for note switches.
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
    void addPropertyNamed(const QString &name);

private Q_SLOTS:
    void onActiveFileChanged(const QString &path);
    void onCacheChanged(const QString &path);
    void onEditorValueChanged();
    void onAddPropertyClicked();

private:
    void refresh();
    void clearEditors();
    void scheduleWrite();
    void flushWrite();

    MetadataCacheReader *m_metadata = nullptr;
    VaultProxy *m_vaultProxy = nullptr;
    FileManagerProxy *m_fmProxy = nullptr;
    WorkspaceController *m_workspace = nullptr;

    QLabel *m_headerLabel;
    QLabel *m_emptyLabel;
    QFormLayout *m_form;
    QWidget *m_formContainer;
    QPushButton *m_addPropertyButton;
    QTimer *m_writeDebounce;

    QString m_currentPath;

    struct EditorRow {
        QString key;
        PropertyEditorWidget *editor = nullptr;
    };
    QVector<EditorRow> m_rows;
};

} // namespace Corbomite
