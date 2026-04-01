// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/TemplateService.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/storage/FileSystemAdapter.h"

#include <QDate>
#include <QDir>
#include <QRegularExpression>
#include <QTime>

namespace Corbomite {

TemplateService::TemplateService(VaultModel *vault, QObject *parent)
    : QObject(parent)
    , m_vault(vault)
{
}

void TemplateService::setTemplateFolder(const QString &folder)
{
    m_templateFolder = folder;
}

QString TemplateService::templateFolder() const
{
    return m_templateFolder;
}

void TemplateService::setDefaultDateFormat(const QString &format)
{
    m_dateFormat = format;
}

void TemplateService::setDefaultTimeFormat(const QString &format)
{
    m_timeFormat = format;
}

QStringList TemplateService::availableTemplates() const
{
    if (!m_vault) return {};

    QString absPath = m_vault->path() + QLatin1Char('/') + m_templateFolder;
    QDir dir(absPath);
    if (!dir.exists()) return {};

    QStringList result;
    auto entries = dir.entryInfoList({QStringLiteral("*.md")}, QDir::Files, QDir::Name);
    for (const auto &fi : entries) {
        result.append(fi.completeBaseName());
    }
    return result;
}

QString TemplateService::expandTemplate(const QString &templateContent,
                                         const QString &noteTitle) const
{
    QString result = templateContent;
    QDate today = QDate::currentDate();
    QTime now = QTime::currentTime();

    // {{title}}
    result.replace(QStringLiteral("{{title}}"), noteTitle);

    // {{date}} — default format
    result.replace(QStringLiteral("{{date}}"), today.toString(m_dateFormat));

    // {{time}} — default format
    result.replace(QStringLiteral("{{time}}"), now.toString(m_timeFormat));

    // {{date:FORMAT}} — custom date format
    static const QRegularExpression datePattern(QStringLiteral(R"(\{\{date:([^}]+)\}\})"));
    auto dateIt = datePattern.globalMatch(result);
    while (dateIt.hasNext()) {
        auto match = dateIt.next();
        result.replace(match.captured(0), today.toString(match.captured(1)));
    }

    // {{time:FORMAT}} — custom time format
    static const QRegularExpression timePattern(QStringLiteral(R"(\{\{time:([^}]+)\}\})"));
    auto timeIt = timePattern.globalMatch(result);
    while (timeIt.hasNext()) {
        auto match = timeIt.next();
        result.replace(match.captured(0), now.toString(match.captured(1)));
    }

    return result;
}

QString TemplateService::loadAndExpand(const QString &templateName,
                                       const QString &noteTitle) const
{
    if (!m_vault) return {};

    QString path = m_vault->path() + QLatin1Char('/')
                 + m_templateFolder + QLatin1Char('/')
                 + templateName + QStringLiteral(".md");

    FileSystemAdapter fs;
    auto content = fs.readFile(path);
    if (!content.has_value()) return {};

    return expandTemplate(content.value(), noteTitle);
}

} // namespace Corbomite
