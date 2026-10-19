// libs/core/include/corbomite/core/FileView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMetaObject>

#include "corbomite/core/ItemView.h"

namespace Corbomite {

class NoteDocument;

class FileView : public ItemView
{
    Q_OBJECT

public:
    explicit FileView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);

    NoteDocument *file() const;
    bool loadFile(NoteDocument *file);

    virtual bool canAcceptExtension(const QString &ext) const;

    QString getDisplayText() const override;
    QJsonObject getState() const override;
    void setState(const QJsonObject &state) override;

    /// Another writer changed this view's file on disk. The host calls this
    /// for every file-backed view when `Vault` reports a modify it did not
    /// write itself (`Vault`'s echo-suppression ledger filters our own saves,
    /// so anything reaching here is genuinely external). `relativePath` is
    /// vault-relative; a view must ignore it unless it is its own file.
    ///
    /// Default: nothing. A view whose on-screen state is not the file's
    /// content has nothing to reconcile. `TextFileView` three-way merges;
    /// `CanvasFileView` adopts the file when it has nothing unsaved and keeps
    /// its edits (telling the user) when it does.
    virtual void onExternalModify(const QString &relativePath);

protected:
    virtual void onLoadFile(NoteDocument *file);
    virtual void onUnloadFile(NoteDocument *file);
    void onOpen() override;
    void onClose() override;

    NoteDocument *m_file = nullptr;
    bool m_navigation = true;
    bool m_allowNoFile = false;

private:
    // Subscription to NoteDocument::pathChanged for the currently loaded
    // file. Bound in loadFile, severed before unload so a stale pointer
    // can't fire across documents.
    QMetaObject::Connection m_pathChangedConn;

    // Subscription to NoteDocument::deleted — fires when Vault dropped the
    // file (programmatic remove/trash or external delete). Severed
    // alongside m_pathChangedConn before unload.
    QMetaObject::Connection m_deletedConn;

    /// Schedule the owning leaf to close on the next event-loop turn.
    /// Used by the deleted/missing-file paths so destruction happens
    /// outside any signal slot that's still iterating Vault state.
    void requestLeafClose();
};

} // namespace Corbomite
