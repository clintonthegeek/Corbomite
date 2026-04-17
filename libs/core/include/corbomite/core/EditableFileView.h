// libs/core/include/corbomite/core/EditableFileView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/FileView.h"

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
};

} // namespace Corbomite
