// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace Corbomite {

class Vault;
class VaultModel;
class NoteDocument;
class TemplateService;
class VaultConfig;

class DailyNoteService : public QObject {
    Q_OBJECT

public:
    explicit DailyNoteService(Vault *vault, VaultModel *vaultModel,
                               TemplateService *templateService, QObject *parent = nullptr);

    void setDateFormat(const QString &format);
    void setFolder(const QString &folder);
    void setTemplateName(const QString &name);

    /// Apply overrides from `.obsidian/daily-notes.json` if present.
    /// Missing file or missing keys leave the current (KConfig/default)
    /// state intact. Call AFTER setDateFormat/setFolder/setTemplateName
    /// so vault-local values win. See
    /// `docs/obsidian-audit/addenda/2026-04-15-daily-notes-templates-schemas.md`.
    void initFromVaultConfig(Corbomite::VaultConfig &config);

    QString todayNotePath() const;
    bool todayNoteExists() const;

    NoteDocument *openOrCreateToday();

private:
    Vault *m_vault;
    VaultModel *m_vaultModel;
    TemplateService *m_templateService;
    QString m_dateFormat = QStringLiteral("YYYY-MM-DD");
    QString m_folder = QStringLiteral("Daily Notes");
    QString m_templateName;
};

} // namespace Corbomite
