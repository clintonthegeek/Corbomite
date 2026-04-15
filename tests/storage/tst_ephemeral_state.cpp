// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster E Phase 1 — unit tests for Corbomite::EphemeralState round-trip.
//
// Covers:
//   - Default-constructed struct serializes to the expected minimal JSON.
//   - Round-trip preserves every typed field.
//   - Unknown-key preservation (Cluster B WorkspaceState invariant).
//   - Fixture round-trip against a hand-rolled Obsidian-shape `eState` blob.
//
// No `workspace.json` fixture was left behind by Cluster B containing a full
// `eState` sub-object (grep across tests/ confirms only the tst_workspacestate
// inline blob, which carries `state.state.mode` but not a full `eState`). We
// build the fixture inline from the audit's example in domains/workspace.md §3.

#include "corbomite/storage/EphemeralState.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QTest>

using Corbomite::EphemeralState;

class EphemeralStateTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void defaultSerializesToMinimalJson()
    {
        const EphemeralState s;
        const auto json = s.toJson();
        QCOMPARE(json.value(QStringLiteral("scroll")).toDouble(), 0.0);
        QCOMPARE(json.value(QStringLiteral("mode")).toString(), QStringLiteral("source"));
        QCOMPARE(json.value(QStringLiteral("source")).toBool(), false);

        const auto cursor = json.value(QStringLiteral("cursor")).toObject();
        QCOMPARE(cursor.value(QStringLiteral("line")).toInt(), 0);
        QCOMPARE(cursor.value(QStringLiteral("column")).toInt(), 0);

        const auto folds = json.value(QStringLiteral("foldedHeadings")).toArray();
        QCOMPARE(folds.size(), 0);
    }

    void typedFieldsRoundTrip()
    {
        EphemeralState s;
        s.scroll = 42.73f;
        s.cursor.line = 17;
        s.cursor.column = 4;
        s.modeRaw = QStringLiteral("source");
        s.sourceFlag = true;
        s.foldedHeadings = {3, 9, 21};

        const auto json = s.toJson();
        const auto back = EphemeralState::fromJson(json);

        QCOMPARE(back.scroll, 42.73f);
        QCOMPARE(back.cursor.line, 17);
        QCOMPARE(back.cursor.column, 4);
        QCOMPARE(back.modeRaw, QStringLiteral("source"));
        QCOMPARE(back.sourceFlag, true);
        QCOMPARE(back.foldedHeadings, QVector<int>({3, 9, 21}));
    }

    void previewModeRoundTrip()
    {
        EphemeralState s;
        s.modeRaw = QStringLiteral("preview");
        s.sourceFlag = false; // ignored for preview but still round-trip stable
        s.scroll = 3.5f;

        const auto json = s.toJson();
        QCOMPARE(json.value(QStringLiteral("mode")).toString(), QStringLiteral("preview"));

        const auto back = EphemeralState::fromJson(json);
        QCOMPARE(back.modeRaw, QStringLiteral("preview"));
        QCOMPARE(back.scroll, 3.5f);
    }

    // --- Cluster B invariant: preserve unknown keys. ---
    void unknownKeysSurviveRoundTrip()
    {
        const QByteArray rawIn = R"({
            "scroll": 5.5,
            "cursor": {"line": 2, "column": 6},
            "mode": "source",
            "source": false,
            "foldedHeadings": [1, 4],
            "_plugin_ks": {"foo": "bar"},
            "_futureKey": 42
        })";
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(rawIn, &err);
        QCOMPARE(err.error, QJsonParseError::NoError);

        const auto s = EphemeralState::fromJson(doc.object());
        const auto out = s.toJson();

        // Typed fields parsed correctly.
        QCOMPARE(s.scroll, 5.5f);
        QCOMPARE(s.cursor.line, 2);
        QCOMPARE(s.cursor.column, 6);
        QCOMPARE(s.foldedHeadings, QVector<int>({1, 4}));

        // Unknown keys still present after round-trip.
        QVERIFY(out.contains(QStringLiteral("_plugin_ks")));
        const auto plugin = out.value(QStringLiteral("_plugin_ks")).toObject();
        QCOMPARE(plugin.value(QStringLiteral("foo")).toString(), QStringLiteral("bar"));
        QCOMPARE(out.value(QStringLiteral("_futureKey")).toInt(), 42);
    }

    // --- Absent optional fields (`source` or `cursor`) use defaults. ---
    void absentSourceFieldDefaultsToFalse()
    {
        QJsonObject in;
        in.insert(QStringLiteral("mode"), QStringLiteral("source"));
        // no "source" key
        const auto s = EphemeralState::fromJson(in);
        QCOMPARE(s.modeRaw, QStringLiteral("source"));
        QCOMPARE(s.sourceFlag, false);
    }

    void absentCursorYieldsZeroZero()
    {
        QJsonObject in;
        in.insert(QStringLiteral("mode"), QStringLiteral("source"));
        const auto s = EphemeralState::fromJson(in);
        QCOMPARE(s.cursor.line, 0);
        QCOMPARE(s.cursor.column, 0);
    }

    // --- Fixture round-trip — construct an Obsidian-shape eState inline
    //     (no pre-existing fixture in tests/ carried a full eState). ---
    void obsidianShapeEStateRoundTrip()
    {
        // Shape from docs/obsidian-audit/domains/workspace.md §3: a leaf's
        // `eState` for a markdown view carries scroll (float), cursor
        // (line,column), mode + source, optional plugin keys.
        const QByteArray wire = R"({
            "scroll": 128.42,
            "cursor": {"line": 15, "column": 0},
            "mode": "source",
            "source": true,
            "foldedHeadings": [5, 12, 48],
            "_obsidian_internal": {"lastActive": 1739999999}
        })";
        QJsonParseError err;
        const auto in = QJsonDocument::fromJson(wire, &err).object();
        QCOMPARE(err.error, QJsonParseError::NoError);

        const auto s = EphemeralState::fromJson(in);
        QCOMPARE(s.scroll, 128.42f);
        QCOMPARE(s.cursor.line, 15);
        QCOMPARE(s.cursor.column, 0);
        QCOMPARE(s.modeRaw, QStringLiteral("source"));
        QCOMPARE(s.sourceFlag, true);
        QCOMPARE(s.foldedHeadings, QVector<int>({5, 12, 48}));

        const auto out = s.toJson();
        // Typed fields byte-equivalent at the field level.
        QCOMPARE(out.value(QStringLiteral("scroll")).toDouble(),
                 static_cast<double>(s.scroll));
        QCOMPARE(out.value(QStringLiteral("cursor")).toObject(),
                 in.value(QStringLiteral("cursor")).toObject());
        QCOMPARE(out.value(QStringLiteral("mode")).toString(), QStringLiteral("source"));
        QCOMPARE(out.value(QStringLiteral("source")).toBool(), true);
        QCOMPARE(out.value(QStringLiteral("foldedHeadings")).toArray(),
                 in.value(QStringLiteral("foldedHeadings")).toArray());
        // Unknown keys preserved.
        QCOMPARE(out.value(QStringLiteral("_obsidian_internal")).toObject(),
                 in.value(QStringLiteral("_obsidian_internal")).toObject());
    }
};

QTEST_APPLESS_MAIN(EphemeralStateTest)
#include "tst_ephemeral_state.moc"
