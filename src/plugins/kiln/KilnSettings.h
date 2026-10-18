// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace Corbomite {

/// One publication target, in the shape the engine reads.
///
/// Field names match the engine's config keys rather than being translated on the
/// way out. A second vocabulary would be a second thing to keep in step, and the
/// panel is not the only writer: the same JSON is what a shell run of `kiln` reads,
/// so a mismatch would be a mismatch between two doors into one feature.
struct KilnProjection
{
    QString name;
    QStringList folders;      ///< cohort: notes under these top-level folders
    QStringList deny;         ///< note paths or bare filenames held back
    QString orderFrom;        ///< a note whose link order is the running order
    QString section;
    QString shape = QStringLiteral("bundle");        ///< bundle | leaf
    QString frontMatter = QStringLiteral("yaml");    ///< yaml | toml
    QStringList fieldOrder;
    QString linkBase;         ///< empty means "derive it from the section"

    // -- the site shell
    /// Extra Hugo taxonomies: front-matter key -> taxonomy name.
    QMap<QString, QString> taxonomies;
    bool related = true;      ///< configure `.Related` indices
    bool fragments = true;    ///< write Kiln's own config fragments
    bool scaffold = false;    ///< write the site shell as well

    QJsonObject toJson() const;
    static KilnProjection fromJson(const QJsonObject &object);

    /// Why this projection cannot be run, or an empty string when it can.
    QString problem() const;
};

/// Everything the engine needs, and everything the panel edits.
struct KilnSettings
{
    /// Not read by the engine — it is what the panel spawns. Kept here so one file
    /// holds all of it.
    QString enginePath = QStringLiteral("kiln");
    QString output;
    QList<KilnProjection> projections;

    static KilnSettings fromJson(const QJsonObject &object);

    /// The engine's config, exactly as `kiln --config -` wants it on stdin.
    /// `vaultPath` is passed in rather than stored: the vault's location is a fact
    /// the host knows and a saved file can only remember wrongly.
    QJsonObject toJson(const QString &vaultPath) const;

    /// The first thing that would stop a run, or an empty string. Checked before
    /// spawning anything, so a half-configured projection is a sentence in the panel
    /// rather than a process that fails somewhere else.
    QString problem() const;
};

/// The engine's report as text a person reads. Logic rather than widget code, so it
/// can be asserted without a panel — and the panel's whole job is to show this.
QString describeReport(const QJsonObject &report, int exitCode, const QString &standardError);

} // namespace Corbomite
