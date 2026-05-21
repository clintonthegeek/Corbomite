// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace Markoff { class MarkoffDocument; class FindController; }

namespace Corbomite {

class NoteDocument : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool modified READ isModified NOTIFY modificationChanged)

public:
    // TODO(port-foundation-exploration): pool parameter formerly carried a
    // Markoff::ParsePool *; the new MarkoffDocument has no parse pool
    // (incremental parser retired with D4). Parameter dropped from signature.
    explicit NoteDocument(const QString &vaultRoot, const QString &relativePath,
                          QObject *parent = nullptr);
    ~NoteDocument() override;

    QString filePath() const;
    QString relativePath() const;
    QString name() const;

    /// Update the document's vault-relative path. Used by `Vault::rename`
    /// to keep an open NoteDocument in sync with on-disk renames so views
    /// holding the document can refresh their title/tab caption. Emits
    /// `pathChanged(oldRelativePath)` when the path actually changes.
    void setRelativePath(const QString &relativePath);

    /// Notify view subscribers that the underlying file has been removed
    /// (programmatic delete/trash or external deletion). Emits `deleted()`
    /// so a `FileView` can null its file pointer and request its leaf to
    /// close instead of orphaning the tab. Called by `Vault` immediately
    /// before it `deleteLater()`s the doc.
    void markDeleted();

    QString markdown() const;
    void    setMarkdown(const QString &text);

    bool isModified() const;
    void setModified(bool modified);

    int wordCount() const;
    int characterCount() const;

    // New: leaves bind via note->markoff().
    Markoff::MarkoffDocument       *markoff();
    const Markoff::MarkoffDocument *markoff() const;

    /// Lazy: constructs one FindController per NoteDocument on first call.
    /// Bound to markoff(); owned by NoteDocument via QObject parent.
    Markoff::FindController *findController();

Q_SIGNALS:
    void textChanged();
    void modificationChanged(bool modified);
    void saved();

    /// Emitted when Vault::saveDocument aborts the write (e.g. canonical
    /// buffer contains invalid bytes). The file on disk is unchanged.
    void saveFailed();

    /// Emitted after `setRelativePath` mutates the cached path. Carries
    /// the previous relative path so listeners (typically FileView
    /// subclasses) can refresh title/header chrome and any per-path UI
    /// state. Mirrors Obsidian's `vault.on("rename")` propagation that
    /// FileView.onload subscribes to.
    void pathChanged(const QString &oldRelativePath);

    /// Emitted by `markDeleted` to let view subscribers null their cached
    /// pointer and close the leaf. Mirrors Obsidian's per-FileView
    /// subscription to `vault.on("delete")`.
    void deleted();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace Corbomite
