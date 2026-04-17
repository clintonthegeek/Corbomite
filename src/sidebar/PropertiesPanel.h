// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtWidgets/QWidget>

class QFormLayout;
class QLabel;
class QPushButton;
class QTimer;

namespace Corbomite {

class FileManager;
class MetadataCache;
class NoteDocument;
class PropertyEditorWidget;
class Vault;

/// Sidebar panel that shows the current note's YAML frontmatter as an
/// editable key-value form. Edits are debounced 500ms and written back
/// atomically via FileManager::processFrontMatter. Subscribes to
/// MetadataCache::cacheChanged for reactive refresh on external edits,
/// but suppresses refresh while the user is actively editing (debounce
/// timer active).
class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget *parent = nullptr);
    ~PropertiesPanel() override;

    void setMetadataCache(MetadataCache *cache);
    /// Q.0 P6 — pair of (Vault, FileManager) drives frontmatter writeback.
    /// Both must be non-null for writes to flush.
    void setVault(Vault *vault);
    void setFileManager(FileManager *fm);
    void setCurrentNote(NoteDocument *doc);

    /// Programmatic helper for tests — add a property with a default
    /// Text editor. Schedules a write.
    void addPropertyNamed(const QString &name);

    /// Accessor for tests — the count of currently-shown editor rows.
    int rowCount() const;

    /// Accessor for tests — flush any pending debounced write immediately.
    void flushPendingWrite();

Q_SIGNALS:
    /// Emitted after a successful file write via FrontMatterWriter.
    void propertiesWritten(const QString &filePath);

private Q_SLOTS:
    void onMetadataCacheChanged(const QString &path);
    void onEditorValueChanged();
    void onAddPropertyClicked();

private:
    void refresh();
    void clearEditors();
    void scheduleWrite();
    void flushWrite();

    QLabel *m_headerLabel;
    QLabel *m_emptyLabel;
    QFormLayout *m_form;
    QWidget *m_formContainer;
    QPushButton *m_addPropertyButton;
    QTimer *m_writeDebounce;

    QPointer<MetadataCache> m_cache;
    QPointer<NoteDocument> m_currentDoc;
    Vault *m_vault = nullptr;
    FileManager *m_fileManager = nullptr;

    struct EditorRow {
        QString key;
        PropertyEditorWidget *editor = nullptr;
    };
    QVector<EditorRow> m_rows;
};

}  // namespace Corbomite
