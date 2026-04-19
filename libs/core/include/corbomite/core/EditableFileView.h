// libs/core/include/corbomite/core/EditableFileView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/FileView.h"

#include <functional>

class QLineEdit;

namespace Corbomite {

class EditableFileView : public FileView
{
    Q_OBJECT

public:
    explicit EditableFileView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);

    /// Move the cursor to `line` (1-based). Default returns false — subclasses
    /// that host an editable editor (Markdown source/live-preview) override.
    /// Used by WorkspaceController::goToLine to drive Outline-style
    /// scroll-to-heading from plugins.
    virtual bool setCursorLine(int line);

    // ---- Host-bound file-menu callbacks (Cluster R Phase 2) ----
    //
    // `libs/core` cannot depend on `libs/vault` (the reverse is already the
    // case), so the universal file-menu items in `onMoreOptionsMenu` talk
    // to `FileManager` / `CommandRegistry` / `Vault` via opaque callbacks
    // injected by the host (MainWindow). Each callback receives the view's
    // current `NoteDocument *` so the wiring is stateless — no raw vault
    // pointers leak into core.
    //
    // Every callback may be null (unset); when null, the corresponding
    // menu item is still shown but triggers a no-op. Under typical hosting
    // (MainWindow wiring), all five land populated before the view is
    // opened for the first time.
    using NoteDocumentCallback = std::function<void(NoteDocument *, QWidget *parent)>;
    using CommandDispatch = std::function<void(const QString &commandId)>;

    void setRenameCallback(NoteDocumentCallback cb);
    void setMoveCallback(NoteDocumentCallback cb);
    void setDeleteCallback(NoteDocumentCallback cb);
    void setVaultAbsolutePathResolver(std::function<QString(NoteDocument *)> resolver);
    void setVaultNameResolver(std::function<QString()> resolver);
    void setCommandDispatcher(CommandDispatch dispatcher);

    void onMoreOptionsMenu(MenuSectionHelper &helper) override;

protected:
    void onOpen() override;
    void onPaneMenu(QMenu *menu) override;

private:
    void startRename();
    void finishRename();
    void cancelRename();

    QLineEdit *m_titleEdit = nullptr;
    QString m_originalName;
    bool m_renaming = false;

    NoteDocumentCallback m_renameCallback;
    NoteDocumentCallback m_moveCallback;
    NoteDocumentCallback m_deleteCallback;
    std::function<QString(NoteDocument *)> m_absolutePathResolver;
    std::function<QString()> m_vaultNameResolver;
    CommandDispatch m_commandDispatcher;
};

} // namespace Corbomite
