// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace Corbomite {

class VaultModel;
class NoteService;
class NoteDocument;
class TemplateService;

class DailyNoteService : public QObject {
    Q_OBJECT

public:
    explicit DailyNoteService(VaultModel *vault, NoteService *noteService,
                               TemplateService *templateService, QObject *parent = nullptr);

    void setDateFormat(const QString &format);
    void setFolder(const QString &folder);
    void setTemplateName(const QString &name);

    QString todayNotePath() const;
    bool todayNoteExists() const;

    NoteDocument *openOrCreateToday();

private:
    VaultModel *m_vault;
    NoteService *m_noteService;
    TemplateService *m_templateService;
    QString m_dateFormat = QStringLiteral("yyyy-MM-dd");
    QString m_folder = QStringLiteral("Daily Notes");
    QString m_templateName;
};

} // namespace Corbomite
