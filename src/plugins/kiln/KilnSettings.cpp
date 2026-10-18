// SPDX-License-Identifier: GPL-3.0-or-later
#include "KilnSettings.h"

#include <KLocalizedString>

#include <QJsonArray>
#include <QSet>

namespace Corbomite {
namespace {

QStringList stringList(const QJsonValue &value)
{
    QStringList out;
    if (value.isString()) {
        out.append(value.toString());
        return out;
    }
    for (const QJsonValue &entry : value.toArray()) {
        if (entry.isString())
            out.append(entry.toString());
    }
    return out;
}

QJsonArray arrayOf(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values) {
        if (!value.trimmed().isEmpty())
            array.append(value.trimmed());
    }
    return array;
}

/// A comma-separated line into a list, dropping empties. The panel edits lists as
/// text, because a list editor for four different lists is more widget than feature.
QStringList splitList(const QString &text)
{
    QStringList out;
    for (const QString &part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty())
            out.append(trimmed);
    }
    return out;
}

QString joinList(const QStringList &values)
{
    return values.join(QStringLiteral(", "));
}

/// Renders one section of the engine's report, skipping it when it is empty.
QString section(const QJsonObject &report, const QString &key, const QString &heading)
{
    const QJsonArray rows = report.value(key).toArray();
    if (rows.isEmpty())
        return QString();

    QString out = QStringLiteral("\n%1 (%2)\n").arg(heading).arg(rows.size());
    for (const QJsonValue &value : rows) {
        const QJsonObject row = value.toObject();
        const QString note = row.value(QStringLiteral("note")).toString();
        const QString detail = row.value(QStringLiteral("detail")).toString();
        out += QStringLiteral("  %1  %2\n").arg(note, detail);
    }
    return out;
}

bool hasFindings(const QJsonObject &report)
{
    for (const QString &key : {QStringLiteral("unresolvedLinks"), QStringLiteral("unselectedLinks"),
                               QStringLiteral("brokenProperties"), QStringLiteral("warnings")}) {
        if (!report.value(key).toArray().isEmpty())
            return true;
    }
    return false;
}

} // namespace

QJsonObject KilnProjection::toJson() const
{
    QJsonObject select;
    if (!folders.isEmpty())
        select.insert(QStringLiteral("folders"), arrayOf(folders));
    if (!deny.isEmpty())
        select.insert(QStringLiteral("deny"), arrayOf(deny));
    if (!orderFrom.trimmed().isEmpty())
        select.insert(QStringLiteral("orderFrom"), orderFrom.trimmed());

    QJsonObject object;
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("select"), select);
    // Left out when empty, like `linkBase` and for the same reason: empty is the
    // instruction, not a value. Empty means each note's own folder is its section, which
    // is the engine's own default too, so writing `""` would say nothing.
    if (!section.trimmed().isEmpty())
        object.insert(QStringLiteral("section"), section.trimmed());
    object.insert(QStringLiteral("shape"), shape);
    object.insert(QStringLiteral("frontMatter"), frontMatter);
    if (!fieldOrder.isEmpty())
        object.insert(QStringLiteral("fieldOrder"), arrayOf(fieldOrder));
    // Left out when empty, because empty is the instruction: derive the base from
    // the section. Writing `""` would be a value the engine would then have to
    // treat as unset anyway.
    if (!linkBase.trimmed().isEmpty())
        object.insert(QStringLiteral("linkBase"), linkBase.trimmed());

    if (!taxonomies.isEmpty()) {
        QJsonObject map;
        for (auto it = taxonomies.constBegin(); it != taxonomies.constEnd(); ++it)
            map.insert(it.key(), it.value());
        object.insert(QStringLiteral("taxonomies"), map);
    }
    // Booleans are written only when they differ from the engine's own default, so the
    // stored config says what the author decided and nothing else.
    if (!related)
        object.insert(QStringLiteral("related"), false);
    if (!fragments)
        object.insert(QStringLiteral("fragments"), false);
    if (scaffold)
        object.insert(QStringLiteral("scaffold"), true);
    return object;
}

KilnProjection KilnProjection::fromJson(const QJsonObject &object)
{
    KilnProjection projection;
    projection.name = object.value(QStringLiteral("name")).toString();

    const QJsonObject select = object.value(QStringLiteral("select")).toObject();
    projection.folders = stringList(select.value(QStringLiteral("folders")));
    projection.deny = stringList(select.value(QStringLiteral("deny")));
    projection.orderFrom = select.value(QStringLiteral("orderFrom")).toString();

    projection.section = object.value(QStringLiteral("section")).toString(projection.section);
    projection.shape = object.value(QStringLiteral("shape")).toString(projection.shape);
    projection.frontMatter =
        object.value(QStringLiteral("frontMatter")).toString(projection.frontMatter);
    projection.fieldOrder = stringList(object.value(QStringLiteral("fieldOrder")));
    projection.linkBase = object.value(QStringLiteral("linkBase")).toString();

    const QJsonObject taxonomies = object.value(QStringLiteral("taxonomies")).toObject();
    for (auto it = taxonomies.constBegin(); it != taxonomies.constEnd(); ++it)
        projection.taxonomies.insert(it.key(), it.value().toString());
    projection.related = object.value(QStringLiteral("related")).toBool(true);
    projection.fragments = object.value(QStringLiteral("fragments")).toBool(true);
    projection.scaffold = object.value(QStringLiteral("scaffold")).toBool(false);
    return projection;
}

QString KilnProjection::problem() const
{
    if (name.trimmed().isEmpty())
        return i18n("A projection has no name.");
    if (folders.isEmpty() && orderFrom.trimmed().isEmpty())
        return i18n("Projection '%1' selects nothing: give it a folder, or a note whose links are the running order.",
                    name);
    if (shape != QLatin1String("bundle") && shape != QLatin1String("leaf"))
        return i18n("Projection '%1' has an unknown shape '%2'.", name, shape);
    if (frontMatter != QLatin1String("yaml") && frontMatter != QLatin1String("toml"))
        return i18n("Projection '%1' has an unknown front matter format '%2'.", name, frontMatter);
    // No section is required. Empty means each note's own folder is its section, which is
    // what lets one projection publish `software/` and `techniques/` into two sections —
    // demanding a section here would force the author to file both under one heading.
    return QString();
}

KilnSettings KilnSettings::fromJson(const QJsonObject &object)
{
    KilnSettings settings;
    settings.enginePath = object.value(QStringLiteral("enginePath")).toString(settings.enginePath);
    settings.output = object.value(QStringLiteral("output")).toString();
    for (const QJsonValue &value : object.value(QStringLiteral("projections")).toArray())
        settings.projections.append(KilnProjection::fromJson(value.toObject()));
    return settings;
}

QJsonObject KilnSettings::toJson(const QString &vaultPath) const
{
    QJsonArray projections;
    for (const KilnProjection &projection : this->projections)
        projections.append(projection.toJson());

    QJsonObject object;
    object.insert(QStringLiteral("vault"), vaultPath);
    object.insert(QStringLiteral("output"), output);
    object.insert(QStringLiteral("projections"), projections);
    // Extra, and the engine ignores it. One file serves both readers: the panel and a
    // shell run of `kiln --config <data.json>`.
    object.insert(QStringLiteral("enginePath"), enginePath);
    return object;
}

QString KilnSettings::problem() const
{
    if (enginePath.trimmed().isEmpty())
        return i18n("No engine path: tell the panel where the kiln binary is.");
    if (output.trimmed().isEmpty())
        return i18n("No site directory: the engine writes a content tree, it does not invent a site.");
    if (projections.isEmpty())
        return i18n("No projections: add one to say which notes belong to a site.");

    QSet<QString> names;
    for (const KilnProjection &projection : this->projections) {
        const QString projectionProblem = projection.problem();
        if (!projectionProblem.isEmpty())
            return projectionProblem;
        const QString key = projection.name.trimmed().toLower();
        if (names.contains(key))
            return i18n("Two projections are called '%1'.", projection.name);
        names.insert(key);
    }
    return QString();
}

QString describeReport(const QJsonObject &report, int exitCode, const QString &standardError)
{
    if (report.isEmpty()) {
        if (exitCode < 0)
            return i18n("The engine did not start. %1", standardError);
        return i18n("The engine exited %1 without a report. %2", exitCode, standardError);
    }

    if (!report.value(QStringLiteral("ok")).toBool()) {
        return i18n("The run did not happen: %1")
                .arg(report.value(QStringLiteral("error")).toString());
    }

    QString out = i18n("Target %1%2\nSelected %3   links rewritten %4\n",
                       report.value(QStringLiteral("targetRoot")).toString(),
                       report.value(QStringLiteral("dryRun")).toBool()
                           ? i18n("   (dry run, nothing written)")
                           : QString(),
                       report.value(QStringLiteral("selected")).toInt(),
                       report.value(QStringLiteral("linksRewritten")).toInt());

    out += section(report, QStringLiteral("denied"), i18n("held back"));
    out += section(report, QStringLiteral("unresolvedLinks"), i18n("links to no such note"));
    out += section(report, QStringLiteral("unselectedLinks"),
                   i18n("links to notes this projection does not publish"));
    out += section(report, QStringLiteral("brokenProperties"),
                   i18n("link properties that did not resolve"));
    out += section(report, QStringLiteral("moved"),
                   i18n("URLs that moved (the old one is now a redirect)"));
    out += section(report, QStringLiteral("gone"),
                   i18n("published last time, not this time"));
    out += section(report, QStringLiteral("removed"), i18n("pruned"));
    out += section(report, QStringLiteral("shell"), i18n("the site shell"));
    out += section(report, QStringLiteral("warnings"), i18n("warnings"));

    out += QLatin1Char('\n');
    if (hasFindings(report))
        out += i18n("Nothing was published that should not have been, but there is something to fix.");
    else
        out += i18n("Clean.");
    return out;
}

} // namespace Corbomite
