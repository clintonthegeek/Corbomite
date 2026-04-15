// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace Corbomite {

class VaultModel;
class VaultConfig;

class TemplateService : public QObject {
    Q_OBJECT

public:
    explicit TemplateService(VaultModel *vault, QObject *parent = nullptr);

    void setTemplateFolder(const QString &folder);
    QString templateFolder() const;

    void setDefaultDateFormat(const QString &format);
    QString defaultDateFormat() const;
    void setDefaultTimeFormat(const QString &format);
    QString defaultTimeFormat() const;

    QStringList availableTemplates() const;

    /// Expand `{{title}}`, `{{date[:FMT]}}`, `{{time[:FMT]}}`, `{{folder}}`.
    /// `{{cursor}}` is preserved verbatim for callers (e.g., the editor) to
    /// locate and strip post-insertion.
    QString expandTemplate(const QString &templateContent,
                           const QString &noteTitle,
                           const QString &folder) const;

    /// Delegates to the 3-arg overload with an empty folder.
    QString expandTemplate(const QString &templateContent,
                           const QString &noteTitle) const;

    QString loadAndExpand(const QString &templateName,
                          const QString &noteTitle) const;

    /// Reads `.obsidian/templates.json`. If present, updates folder /
    /// date_format / time_format only for keys that are present in the
    /// JSON; absent keys leave current state intact. If the JSON file is
    /// missing altogether, this is a no-op, so callers can seed defaults
    /// from KConfig first and then let the vault override where specified.
    void initFromVaultConfig(Corbomite::VaultConfig &config);

    /// The literal placeholder preserved by `expandTemplate`; consumers
    /// scan post-insertion for this marker to position the cursor.
    static QString cursorMarker() { return QStringLiteral("{{cursor}}"); }

private:
    VaultModel *m_vault;
    QString m_templateFolder = QStringLiteral("Templates");
    QString m_dateFormat = QStringLiteral("YYYY-MM-DD");
    QString m_timeFormat = QStringLiteral("HH:mm");
};

} // namespace Corbomite
