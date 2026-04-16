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
