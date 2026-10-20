// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QList>

#include "corbomite/core/RecoveryStore.h"

class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace Corbomite {

class VersionHistoryService;

/// Version History modal: a per-file snapshot list on the
/// left (newest first, save-failure entries marked) with a read-only preview
/// and a "Restore this version" button. The dialog is UI-only; restore
/// semantics (including the pre-restore safety copy) live in
/// VersionHistoryService.
class VersionHistoryDialog : public QDialog
{
    Q_OBJECT
public:
    VersionHistoryDialog(VersionHistoryService *service,
                         QString relativePath,
                         QWidget *parent = nullptr);

    /// True when the user restored a version (the file was overwritten).
    bool restored() const { return m_restored; }

private:
    void refresh();
    void updateSelection();

    VersionHistoryService *m_service = nullptr;
    QString m_relativePath;
    QList<RecoverySnapshot> m_snapshots;
    QListWidget *m_list = nullptr;
    QPlainTextEdit *m_preview = nullptr;
    QPushButton *m_restore = nullptr;
    bool m_restored = false;
};

}  // namespace Corbomite
