// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>

namespace Corbomite {

class VaultModel;

class TemplateService : public QObject {
    Q_OBJECT

public:
    explicit TemplateService(VaultModel *vault, QObject *parent = nullptr);

    void setTemplateFolder(const QString &folder);
    QString templateFolder() const;

    void setDefaultDateFormat(const QString &format);
    void setDefaultTimeFormat(const QString &format);

    QStringList availableTemplates() const;

    QString expandTemplate(const QString &templateContent,
                           const QString &noteTitle) const;

    QString loadAndExpand(const QString &templateName,
                          const QString &noteTitle) const;

private:
    VaultModel *m_vault;
    QString m_templateFolder = QStringLiteral("Templates");
    QString m_dateFormat = QStringLiteral("yyyy-MM-dd");
    QString m_timeFormat = QStringLiteral("HH:mm");
};

} // namespace Corbomite
