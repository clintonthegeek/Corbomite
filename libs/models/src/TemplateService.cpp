// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/TemplateService.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultConfig.h"
#include "corbomite/core/MomentFormatter.h"

#include <QDateTime>
#include <QDir>
#include <QJsonObject>
#include <QRegularExpression>

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

QString TemplateService::defaultDateFormat() const
{
    return m_dateFormat;
}

void TemplateService::setDefaultTimeFormat(const QString &format)
{
    m_timeFormat = format;
}

QString TemplateService::defaultTimeFormat() const
{
    return m_timeFormat;
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
                                        const QString &noteTitle,
                                        const QString &folder) const
{
    QString result = templateContent;

    // {{title}} — note title passthrough
    result.replace(QStringLiteral("{{title}}"), noteTitle);

    // {{folder}} — caller-provided folder context
    result.replace(QStringLiteral("{{folder}}"), folder);

    // Note: {{cursor}} is deliberately NOT substituted here. The marker is
    // preserved verbatim; consumers (MainWindow::insertTemplate) locate it
    // post-insertion and position the editor cursor there before stripping
    // the marker from the note body.

    const QDateTime now = QDateTime::currentDateTime();

    // {{date:FORMAT}} — explicit Moment.js-token format
    static const QRegularExpression dateWithFormat(
        QStringLiteral(R"(\{\{date:([^}]+)\}\})"));
    {
        auto it = dateWithFormat.globalMatch(result);
        QString tmp = result;
        while (it.hasNext()) {
            auto match = it.next();
            const QString fmt = match.captured(1);
            const QString formatted = Corbomite::MomentFormatter::format(now, fmt);
            tmp.replace(match.captured(0), formatted);
        }
        result = tmp;
    }

    // {{time:FORMAT}} — explicit Moment.js-token format
    static const QRegularExpression timeWithFormat(
        QStringLiteral(R"(\{\{time:([^}]+)\}\})"));
    {
        auto it = timeWithFormat.globalMatch(result);
        QString tmp = result;
        while (it.hasNext()) {
            auto match = it.next();
            const QString fmt = match.captured(1);
            const QString formatted = Corbomite::MomentFormatter::format(now, fmt);
            tmp.replace(match.captured(0), formatted);
        }
        result = tmp;
    }

    // {{date}} and {{time}} with configured defaults
    result.replace(QStringLiteral("{{date}}"),
                   Corbomite::MomentFormatter::format(now, m_dateFormat));
    result.replace(QStringLiteral("{{time}}"),
                   Corbomite::MomentFormatter::format(now, m_timeFormat));

    return result;
}

QString TemplateService::expandTemplate(const QString &templateContent,
                                        const QString &noteTitle) const
{
    return expandTemplate(templateContent, noteTitle, QString{});
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

void TemplateService::initFromVaultConfig(Corbomite::VaultConfig &config)
{
    const auto json = config.readTemplatesJson();
    if (!json) {
        // No vault override; caller's earlier setters / KConfig defaults stand.
        return;
    }
    const QJsonObject &obj = *json;
    if (obj.contains(QStringLiteral("folder"))) {
        setTemplateFolder(obj.value(QStringLiteral("folder")).toString());
    }
    if (obj.contains(QStringLiteral("date_format"))) {
        setDefaultDateFormat(obj.value(QStringLiteral("date_format")).toString());
    }
    if (obj.contains(QStringLiteral("time_format"))) {
        setDefaultTimeFormat(obj.value(QStringLiteral("time_format")).toString());
    }
}

} // namespace Corbomite
