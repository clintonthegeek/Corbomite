// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QListWidget;
class QDialogButtonBox;

namespace Corbomite {

class Vault;
class TAbstractFile;

/// Modal folder-picker used by `FileManager::promptForMove`.
///
/// Shows a flat list of every folder in the vault (the root is displayed
/// as "/"), with a live filter edit for substring narrowing. The source
/// file's current parent is excluded from the list so users can't "move"
/// to the folder the file already lives in. On activation the chosen
/// folder path is stored and the dialog accepts.
class MoveFileDialog : public QDialog
{
    Q_OBJECT
public:
    MoveFileDialog(const TAbstractFile *file,
                   const Vault *vault,
                   QWidget *parent = nullptr);

    /// Test seam: folder paths currently eligible for selection (pre-filter).
    QStringList availableFolderPaths() const;

    /// Test seam: set the filter edit programmatically.
    void setFilterText(const QString &filter);

    /// The chosen folder path ("/" for the vault root). Meaningful only
    /// when the dialog result is Accepted; empty otherwise.
    QString selectedFolderPath() const;

private Q_SLOTS:
    void onFilterChanged(const QString &text);
    void onItemActivated();
    void onAccepted();

private:
    void populateFolderList();

    const TAbstractFile *m_file;
    const Vault *m_vault;
    QLineEdit *m_filterEdit = nullptr;
    QListWidget *m_listWidget = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
    QStringList m_allPaths;
    QString m_selection;
};

}  // namespace Corbomite
