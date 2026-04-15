// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::Hotkey + hotkeys.json round-trip (Cluster C Phase 3).
// Spec: docs/obsidian-audit/VAULT-FORMAT.md §3 .obsidian/hotkeys.json
//
// Obsidian schema:
//   { [commandId]: Hotkey[] }   // value [] means explicitly unbound
//   Hotkey = { modifiers: ('Mod'|'Ctrl'|'Meta'|'Shift'|'Alt')[], key: string }
//
// Invariants we must preserve on round-trip:
//   - Unknown command IDs (not registered yet) are preserved.
//   - Empty-array values (explicit unbind) are preserved.
//   - Keys in insertion order (Object.keys order).
//   - "Mod" modifier stays "Mod" in the file (platform-resolution
//     happens at runtime, not at serialisation).

#include <QTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "corbomite/core/Hotkey.h"

using Corbomite::Hotkey;
using Corbomite::HotkeyModifier;
using Corbomite::HotkeyFile;

class TestHotkey : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // ---- Hotkey struct basics --------------------------------------------

    void testHotkeyEquality()
    {
        Hotkey a{{HotkeyModifier::Mod}, QStringLiteral("b")};
        Hotkey b{{HotkeyModifier::Mod}, QStringLiteral("b")};
        QVERIFY(a == b);
        Hotkey c{{HotkeyModifier::Ctrl}, QStringLiteral("b")};
        QVERIFY(!(a == c));
    }

    // ---- Parse ------------------------------------------------------------

    void testParseBasic()
    {
        const QByteArray json =
            R"({"app:toggle":[{"modifiers":["Mod"],"key":"b"}]})";
        HotkeyFile f = HotkeyFile::parse(json);
        QCOMPARE(f.bindings.size(), 1);
        QVERIFY(f.bindings.contains(QStringLiteral("app:toggle")));
        const auto &list = f.bindings[QStringLiteral("app:toggle")];
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().key, QStringLiteral("b"));
        QCOMPARE(list.first().modifiers.size(), 1);
        QCOMPARE(list.first().modifiers.first(), HotkeyModifier::Mod);
    }

    void testParseMultipleModifiers()
    {
        const QByteArray json =
            R"({"app:do":[{"modifiers":["Ctrl","Shift","Alt"],"key":"F12"}]})";
        auto f = HotkeyFile::parse(json);
        const auto &h = f.bindings[QStringLiteral("app:do")].first();
        QCOMPARE(h.modifiers.size(), 3);
        QVERIFY(h.modifiers.contains(HotkeyModifier::Ctrl));
        QVERIFY(h.modifiers.contains(HotkeyModifier::Shift));
        QVERIFY(h.modifiers.contains(HotkeyModifier::Alt));
    }

    void testParseEmptyArrayMeansExplicitUnbind()
    {
        const QByteArray json = R"({"app:unbound":[]})";
        auto f = HotkeyFile::parse(json);
        QVERIFY(f.bindings.contains(QStringLiteral("app:unbound")));
        QCOMPARE(f.bindings[QStringLiteral("app:unbound")].size(), 0);
    }

    void testParseEmptyObject()
    {
        auto f = HotkeyFile::parse(QByteArray{"{}"});
        QCOMPARE(f.bindings.size(), 0);
    }

    void testParseIgnoresMalformedEntries()
    {
        const QByteArray json = R"({
            "ok":[{"modifiers":["Mod"],"key":"a"}],
            "bad": "not-an-array"
        })";
        auto f = HotkeyFile::parse(json);
        // Only the valid entry survives.
        QCOMPARE(f.bindings.size(), 1);
        QVERIFY(f.bindings.contains(QStringLiteral("ok")));
    }

    // ---- Serialise --------------------------------------------------------

    void testSerialiseRoundTrip()
    {
        HotkeyFile f;
        f.bindings.insert(QStringLiteral("app:a"),
            {Hotkey{{HotkeyModifier::Mod}, QStringLiteral("b")}});
        f.bindings.insert(QStringLiteral("app:b"),
            {}); // explicit unbind

        const auto bytes = f.serialise();
        auto parsed = HotkeyFile::parse(bytes);

        QCOMPARE(parsed.bindings.size(), 2);
        QCOMPARE(parsed.bindings[QStringLiteral("app:a")].size(), 1);
        QCOMPARE(parsed.bindings[QStringLiteral("app:a")].first().key,
                 QStringLiteral("b"));
        QVERIFY(parsed.bindings.contains(QStringLiteral("app:b")));
        QCOMPARE(parsed.bindings[QStringLiteral("app:b")].size(), 0);
    }

    void testSerialisePreservesInsertionOrder()
    {
        HotkeyFile f;
        f.order = {QStringLiteral("z"), QStringLiteral("a"),
                   QStringLiteral("m")};
        for (const auto &id : f.order) {
            f.bindings.insert(id,
                {Hotkey{{}, QStringLiteral("x")}});
        }
        const auto bytes = f.serialise();
        // Ensure the JSON keys appear in the declared order.
        const int zPos = bytes.indexOf("\"z\"");
        const int aPos = bytes.indexOf("\"a\"");
        const int mPos = bytes.indexOf("\"m\"");
        QVERIFY(zPos >= 0 && aPos > zPos && mPos > aPos);
    }

    void testModRemainsModNotPlatformResolved()
    {
        // Obsidian spec: the FILE stores "Mod" unchanged. Platform
        // resolution (Mod→Meta on macOS / Ctrl elsewhere) is a runtime
        // concern via Keymap.compileModifiers.
        HotkeyFile f;
        f.bindings.insert(QStringLiteral("app:a"),
            {Hotkey{{HotkeyModifier::Mod}, QStringLiteral("s")}});
        const auto bytes = f.serialise();
        QVERIFY(bytes.contains("\"Mod\""));
        QVERIFY(!bytes.contains("\"Meta\""));
        QVERIFY(!bytes.contains("\"Ctrl\""));
    }

    // ---- Unknown-command preservation -----------------------------------

    void testUnknownCommandIdsSurvive()
    {
        // Spec: an ID we don't recognise (plugin not yet registered)
        // must survive a load→save round-trip untouched.
        const QByteArray json =
            R"({"unknown-plugin:foo":[{"modifiers":["Mod"],"key":"q"}]})";
        auto f = HotkeyFile::parse(json);
        const auto bytes = f.serialise();
        auto parsed = HotkeyFile::parse(bytes);
        QVERIFY(parsed.bindings.contains(QStringLiteral("unknown-plugin:foo")));
        QCOMPARE(parsed.bindings[QStringLiteral("unknown-plugin:foo")].first().key,
                 QStringLiteral("q"));
    }

    void testSerialiseUsesTwoSpaceIndent()
    {
        // Obsidian writes JSON.stringify(obj, undefined, 2). Match it.
        HotkeyFile f;
        f.bindings.insert(QStringLiteral("app:a"),
            {Hotkey{{HotkeyModifier::Mod}, QStringLiteral("b")}});
        const auto bytes = f.serialise();
        // A 2-space indent appears after the first "{".
        QVERIFY(bytes.contains("\n  \"app:a\""));
    }
};

QTEST_MAIN(TestHotkey)
#include "tst_hotkey.moc"
