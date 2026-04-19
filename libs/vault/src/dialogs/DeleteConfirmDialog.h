// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QLabel;
class QDialogButtonBox;

namespace Corbomite {

class Vault;
class TAbstractFile;

/// Warning-styled delete confirmation modal used by
/// `FileManager::promptForDeletion`.
///
/// Body text is trash-option-aware (system trash / vault trash / permanent
/// delete) and pulled from the `[Files]` group of `KSharedConfig::openConfig()`
/// — the same KConfigXT-backed store that `SettingsDialog` writes to.
///
/// "Don't ask again" is file-only (folder deletions always prompt). When
/// checked on Accept, flips the `[Files]/PromptDelete` key to `false`.
///
/// Cancel is the default button (destructive-action convention).
class DeleteConfirmDialog : public QDialog
{
    Q_OBJECT
public:
    DeleteConfirmDialog(const TAbstractFile *file,
                        Vault *vault,
                        QWidget *parent = nullptr);

    /// Composed body text (for test inspection).
    QString bodyText() const;

    /// Test seam: toggle the "Don't ask again" checkbox programmatically.
    void setDontAskAgain(bool on);

public Q_SLOTS:
    void accept() override;

private:
    const TAbstractFile *m_file;
    Vault *m_vault;
    QLabel *m_bodyLabel = nullptr;
    QCheckBox *m_dontAsk = nullptr;  // null for folder deletions
    QDialogButtonBox *m_buttonBox = nullptr;
    QString m_body;
};

}  // namespace Corbomite
