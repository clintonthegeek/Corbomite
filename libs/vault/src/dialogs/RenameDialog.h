// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QLabel;
class QDialogButtonBox;

namespace Corbomite {

class Vault;
class TAbstractFile;

/// Modal rename dialog used by `FileManager::promptForFileRename`.
///
/// Pre-selects the basename (portion before the final `.`) so typing
/// immediately replaces the descriptive part without clobbering the
/// extension. Live-validates every keystroke through
/// `Corbomite::validateFileName`; the Save button is disabled while the
/// input is invalid and an inline error is shown beneath the entry.
///
/// The dialog is side-effect-free: it only reports the user's chosen name
/// via `proposedNewName()`. `FileManager::promptForFileRename` performs the
/// actual rename through `renameFile` (link-rewrite aware).
class RenameDialog : public QDialog
{
    Q_OBJECT
public:
    RenameDialog(const TAbstractFile *file,
                 const Vault *vault,
                 QWidget *parent = nullptr);

    /// Test seam: programmatic Save-button state introspection.
    bool isSaveEnabled() const;

    /// The user-entered name. Meaningful only when `result() == Accepted`.
    QString proposedNewName() const;

private Q_SLOTS:
    void onTextChanged(const QString &newText);

private:
    const TAbstractFile *m_file;
    const Vault *m_vault;
    QLineEdit *m_edit = nullptr;
    QLabel *m_errorLabel = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

}  // namespace Corbomite
