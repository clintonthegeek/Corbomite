// SPDX-License-Identifier: GPL-3.0-or-later
//
// `eState.scroll` is not ours to write in our own shape.
//
// The bug: contract-v2 made `MarkdownFileView::scrollPositionVisualLine()` return a
// 0.0-1.0 viewport FRACTION, and `EphemeralState::toJson()` wrote that fraction
// straight into Obsidian's `eState.scroll`. But Obsidian's field is a LINE
// NUMBER (`docs/obsidian-audit/domains/views.md:159` — "scroll?: number; //
// line for scroll-to-match"). The punch-list item filed this as "Corbomite-
// internal JSON shape today, so no interop break" — true when filed, and
// invalidated by Cluster L Phase L2, which made `workspace.json` a
// full-fidelity Obsidian-schema file. So by the time anyone opened a shared
// vault in Obsidian, Corbomite had written 0.42 where Obsidian reads a line,
// and Obsidian restored the note to line 0.
//
// The fix follows the Cluster L compat doctrine (`specs/2026-08-17-workspace-
// compat-boundary.md`): schema-at-rest fidelity to Obsidian is the only
// contract, in-memory shape is free to be idiomatic. Our fraction moves to a
// `corbomite.`-prefixed key inside the same object; Obsidian's `scroll` is left
// alone. Converting the two would need the viewport height, which is not
// available at serialize time and is not stable across window sizes.
//
// The round-trip half matters as much as the write half: an Obsidian-authored
// `scroll` must survive untouched, via the extraKeys passthrough, and must not
// be misread as a fraction on load.

#include "corbomite/storage/EphemeralState.h"

#include <QJsonObject>
#include <QTest>

using Corbomite::EphemeralState;

class TestEphemeralScrollInterop : public QObject
{
    Q_OBJECT

private slots:
    void toJson_doesNotWriteObsidianScrollKey();
    void toJson_writesPrivateFractionKey();
    void fromJson_readsPrivateFractionKey();
    void obsidianAuthoredScroll_roundTripsVerbatim();
    void obsidianScroll_isNotMisreadAsAFraction();
    void fraction_survivesCorbomiteRoundTrip();
};

void TestEphemeralScrollInterop::toJson_doesNotWriteObsidianScrollKey()
{
    EphemeralState s;
    s.scroll = 0.42f;

    const QJsonObject out = s.toJson();

    QVERIFY2(!out.contains(QStringLiteral("scroll")),
             "Corbomite wrote its viewport fraction into Obsidian's "
             "`eState.scroll`, whose value is a LINE NUMBER. Obsidian would "
             "restore the note to line 0.");
}

void TestEphemeralScrollInterop::toJson_writesPrivateFractionKey()
{
    EphemeralState s;
    s.scroll = 0.42f;

    const QJsonObject out = s.toJson();

    QVERIFY2(out.contains(QStringLiteral("corbomite.scrollFraction")),
             "the fraction must still be persisted, under a Corbomite-private "
             "key, or scroll restore regresses");
    // Fuzzy, not QCOMPARE: the field is a float, and 0.42f widens to
    // 0.41999998688697815 as a double.
    QVERIFY(qAbs(out.value(QStringLiteral("corbomite.scrollFraction")).toDouble()
                 - 0.42) < 1e-5);
}

void TestEphemeralScrollInterop::fromJson_readsPrivateFractionKey()
{
    QJsonObject in;
    in.insert(QStringLiteral("corbomite.scrollFraction"), 0.75);

    const EphemeralState s = EphemeralState::fromJson(in);

    QVERIFY(qAbs(s.scroll - 0.75f) < 1e-5);
}

// An Obsidian-authored workspace.json must not lose Obsidian's own field.
void TestEphemeralScrollInterop::obsidianAuthoredScroll_roundTripsVerbatim()
{
    QJsonObject in;
    in.insert(QStringLiteral("scroll"), 128);   // a line number, as Obsidian writes

    const EphemeralState first = EphemeralState::fromJson(in);
    const QJsonObject out = first.toJson();

    QVERIFY2(out.contains(QStringLiteral("scroll")),
             "an Obsidian-authored `scroll` was dropped — schema-at-rest "
             "fidelity means Corbomite must round-trip keys it does not model");
    QCOMPARE(out.value(QStringLiteral("scroll")).toInt(), 128);
}

// The asymmetry that matters: a line number must not be loaded as a fraction.
// 128 clamped into a 0-1 float would restore a nonsense scroll position.
void TestEphemeralScrollInterop::obsidianScroll_isNotMisreadAsAFraction()
{
    QJsonObject in;
    in.insert(QStringLiteral("scroll"), 128);

    const EphemeralState s = EphemeralState::fromJson(in);

    QVERIFY2(s.scroll <= 1.0f,
             qPrintable(QStringLiteral("Obsidian's line-number scroll was read "
                                       "as our fraction: %1")
                            .arg(s.scroll)));
}

void TestEphemeralScrollInterop::fraction_survivesCorbomiteRoundTrip()
{
    EphemeralState s;
    s.scroll = 0.33f;

    const EphemeralState back = EphemeralState::fromJson(s.toJson());

    QVERIFY2(qAbs(back.scroll - 0.33f) < 1e-5,
             qPrintable(QStringLiteral("Corbomite's own scroll restore broke: "
                                       "%1 -> %2")
                            .arg(s.scroll).arg(back.scroll)));
}

QTEST_MAIN(TestEphemeralScrollInterop)
#include "tst_ephemeral_scroll_interop.moc"