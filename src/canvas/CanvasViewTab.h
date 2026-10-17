// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QWidget>

#include <functional>

namespace Canvas {
class CanvasDocument;
class CanvasScene;
class CanvasView;
}

namespace Corbomite {

class MarkdownRenderEngine;

class CanvasViewTab : public QWidget {
    Q_OBJECT

public:
    /// @param filePath Absolute path of the .canvas file being opened.
    /// @param vaultRoot Absolute path of the vault root. File-card paths
    ///        embedded in the canvas are resolved/saved relative to this
    ///        root (per Obsidian's `.canvas` spec), not the canvas file's
    ///        own directory. Falls back to the canvas file's directory
    ///        when left empty (e.g. ad-hoc/out-of-vault usage).
    explicit CanvasViewTab(const QString &filePath, const QString &vaultRoot = QString(),
                           QWidget *parent = nullptr);
    ~CanvasViewTab() override;

    void setRenderEngine(MarkdownRenderEngine *engine);

    QString filePath() const;
    bool save();
    bool isModified() const;

    /// Cluster R Task 3.6 — access to the underlying CanvasScene so
    /// CanvasFileView can drive its render-to-image / render-to-svg
    /// export pipeline.
    Canvas::CanvasScene *canvasScene() const;

    /// Cluster M leftover — supply the vault's file list to the file picker.
    ///
    /// A seam (`std::function`) rather than a `Vault*` on purpose: libs/canvas
    /// must not depend on libs/vault, and this mirrors the existing
    /// `setVaultPathResolver` idiom in the same constructor. Call it before
    /// the tab opens; the picker falls back to a filesystem walk if it is
    /// never set, so nothing breaks for callers that don't supply one.
    void setFileLister(std::function<QStringList()> lister);

    std::function<QStringList()> m_fileLister;

    /// Cluster O Phase O1.T3 — access to the underlying CanvasView so
    /// CanvasFileView can dispatch the polymorphic View::zoomIn/Out/Reset
    /// virtuals onto the real viewport transform. (O4.T1 will want this
    /// same accessor for the canvas toolbar; landing it here is the
    /// minimal plumbing O1.T3 needs, not scope creep into O4.)
    Canvas::CanvasView *canvasView() const;

    /// Cluster M M5.4 — machine-local per-file store seam for the read-only
    /// lock. Same shape and same rationale as setFileLister() above: the
    /// backing store is app state (SessionManager's tier 3) that libs/canvas
    /// and this widget must not own.
    ///
    /// Keys are Obsidian's own localStorage key format, `canvas-<path>`
    /// (`app/own/2028-n9.js:565`), and `path` is vault-relative. The one
    /// payload key is `readonly`. Calling this restores the flag on the scene
    /// and keeps it persisted on every subsequent toggle; calling it with no
    /// reader/writer leaves the lock in memory only.
    using LocalDataReader = std::function<QJsonObject(const QString &key)>;
    using LocalDataWriter = std::function<void(const QString &key, const QJsonObject &value)>;
    void setLocalDataStore(LocalDataReader reader, LocalDataWriter writer);

    /// The `canvas-<path>` key this tab persists under, for tests and for the
    /// CanvasFileView seam. Vault-relative when a vault root is known,
    /// otherwise the absolute file path.
    QString localDataKey() const;

    /// Path half of `localDataKey()`, without the `canvas-` prefix.
    /// Vault-relative when the file is inside the vault, otherwise the
    /// absolute file path. This is the canvas identity passed to Markoff.
    QString canvasRelativePath() const;

    /// Cluster M M5.5 — the two commands, called by CanvasViewActions'
    /// KAction layer. Both are no-ops without a canvas loaded.
    ///
    /// jumpToGroup() is Obsidian's `canvas:jump-to-group`
    /// (`app/own/2028-n9.js:667-677`): pick a group from a fuzzy list, move
    /// the camera onto it.
    void jumpToGroup();

    /// convertTextCardToFile() is Obsidian's `canvas:convert-to-file`
    /// (`app/own/2028-n9.js:678-692`) minus the selection test, which the
    /// caller does as the action's Tier-B gate: prompt for a note name, then
    /// hand the write to the scene (which owns the node swap).
    void convertTextCardToFile(const QString &nodeId);

    /// M5.5 — override the name prompt and the markdown writer the scene uses
    /// for convertTextCardToFile(). Both default to the real modal prompt and
    /// a FileManager write resolved through the window; tests supply their own
    /// so neither a nested event loop nor a MainWindow is required.
    void setFileNameRequestor(std::function<QString()> requestor);
    void setMarkdownFileCreator(
        std::function<QString(const QString &name, const QString &content)> creator);

Q_SIGNALS:
    void modificationChanged(bool modified);

private:
    /// M5.5 — wire the scene's "Convert to File…" name prompt and markdown
    /// writer to the real modal and to FileManager.
    void installConvertToFileSeams();

    void applyLocalData(const QJsonObject &data);

    Canvas::CanvasDocument *m_document;
    Canvas::CanvasView *m_view;
    QString m_filePath;
    QString m_vaultRoot;
    LocalDataReader m_localDataReader;
    LocalDataWriter m_localDataWriter;
    std::function<QString()> m_fileNameRequestor;
    std::function<QString(const QString &, const QString &)> m_markdownFileCreator;
};

} // namespace Corbomite
