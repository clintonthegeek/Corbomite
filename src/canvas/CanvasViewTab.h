// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QWidget>

#include <functional>

namespace Corbomite::Canvas {
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

    /// The canonical Obsidian-style bytes for the document as it stands now —
    /// exactly what `save()` would write.
    QByteArray serializedBytes() const;

    /// The file's current bytes, or a **null** array when it cannot be read
    /// (distinguishable from a legitimately empty file).
    QByteArray diskBytes() const;

    /// Replace the in-memory document with the file's current bytes and
    /// repopulate the scene from it. Safe only when this tab has nothing
    /// unsaved to lose — that decision belongs to the caller (see
    /// `classifyExternalEdit`). The camera and the read-only lock are
    /// deliberately left alone: a reload is not a reopen.
    bool reloadFromDisk();

    /// What to do about an external edit to an open canvas.
    enum class ExternalEdit {
        NoChange,          ///< nothing readable, or disk already matches us
        ReloadFromDisk,    ///< nothing unsaved: adopt the file
        KeepLocalAndWarn,  ///< unsaved edits here: keep them and tell the user
    };

    /// The policy, pure so it can be pinned without a view. `ours` is
    /// `serializedBytes()`, `hasUnsavedEdits` is `isModified()`, and
    /// `lastSeenDisk` is the revision the caller already reacted to (empty on
    /// first sight, so repeated events for one revision warn once).
    static ExternalEdit classifyExternalEdit(const QByteArray &ours,
                                             const QByteArray &disk,
                                             bool hasUnsavedEdits,
                                             const QByteArray &lastSeenDisk);

    /// Access to the underlying CanvasScene so CanvasFileView can drive its
    /// render-to-image / render-to-svg export pipeline.
    Corbomite::Canvas::CanvasScene *canvasScene() const;

    /// Supply the vault's file list to the file picker.
    ///
    /// A seam (`std::function`) rather than a `Vault*` on purpose: libs/canvas
    /// must not depend on libs/vault, and this mirrors the existing
    /// `setVaultPathResolver` idiom in the same constructor. Call it before
    /// the tab opens; the picker falls back to a filesystem walk if it is
    /// never set, so nothing breaks for callers that don't supply one.
    void setFileLister(std::function<QStringList()> lister);

    std::function<QStringList()> m_fileLister;

    /// Access to the underlying CanvasView so CanvasFileView can dispatch
    /// the polymorphic View::zoomIn/Out/Reset virtuals onto the real
    /// viewport transform. (The canvas toolbar wants
    /// this same accessor, so it lives here.)
    Corbomite::Canvas::CanvasView *canvasView() const;

    /// Machine-local per-file store seam for the read-only lock.
    /// Same shape and same rationale as setFileLister() above: the
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

    /// The document this tab owns. The host registers it with
    /// `Corbomite::Canvas::CanvasDocumentRegistry` (keyed on `canvasRelativePath()`) so
    /// the rename rewriter mutates the open instance instead of a second
    /// copy on disk. Never null once the tab is constructed.
    Corbomite::Canvas::CanvasDocument *canvasDocument() const;

    /// Re-render file cards whose target appeared (vault create) or
    /// changed, so a dangling card resolves without reopening the canvas.
    /// Empty `vaultRelativePath` refreshes every file card.
    void refreshFileCards(const QString &vaultRelativePath = QString());

    /// The two commands, called by CanvasViewActions'
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

    /// Override the name prompt and the markdown writer the scene uses
    /// for convertTextCardToFile(). Both default to the real modal prompt and
    /// a FileManager write resolved through the window; tests supply their own
    /// so neither a nested event loop nor a MainWindow is required.
    void setFileNameRequestor(std::function<QString()> requestor);
    void setMarkdownFileCreator(
        std::function<QString(const QString &name, const QString &content)> creator);

Q_SIGNALS:
    void modificationChanged(bool modified);

private:
    /// Wire the scene's "Convert to File…" name prompt and markdown
    /// writer to the real modal and to FileManager.
    void installConvertToFileSeams();

    void applyLocalData(const QJsonObject &data);

    Corbomite::Canvas::CanvasDocument *m_document;
    Corbomite::Canvas::CanvasView *m_view;
    QString m_filePath;
    QString m_vaultRoot;

    /// Debounced autosave. Every mutation of the document arms it with the
    /// `[Editor] AutoSaveDelayMs` delay (the same setting TextFileView uses),
    /// so canvas edits reach disk without waiting for the tab to close or for
    /// Ctrl+S — Obsidian's canvas does the same through `View.requestSave()`
    /// (`app/own/2018-k7.js:477-490`). Packaged as a member because it also
    /// has to be stopped before a disk reload.
    QTimer *m_autosaveTimer = nullptr;
    LocalDataReader m_localDataReader;
    LocalDataWriter m_localDataWriter;
    std::function<QString()> m_fileNameRequestor;
    std::function<QString(const QString &, const QString &)> m_markdownFileCreator;
};

} // namespace Corbomite
