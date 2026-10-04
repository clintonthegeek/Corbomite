// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::Hotkey + hotkeys.json round-trip (Cluster C Phase 3).
// Spec: docs/obsidian-audit/VAULT-FORMAT.md §3 .obsidian/hotkeys.json, as
// corrected by docs/obsidian-audit/addenda/2026-10-03-core-command-and-hotkey-contracts.md §D
//
// Obsidian schema:
//   { [commandId]: Hotkey[] }   // value [] means explicitly unbound
//   Hotkey = { modifiers: ('Mod'|'Ctrl'|'Meta'|'Shift'|'Alt')[], key: string, code?: string }
//
// Invariants we must preserve on round-trip:
//   - Unknown command IDs (not registered yet) are preserved.
//   - Empty-array values (explicit unbind) are preserved.
//   - Keys in insertion order (Object.keys order).
//   - "Mod" modifier stays "Mod" in the file (platform-resolution
//     happens at runtime, not at serialisation).
//
// HotkeyManager covers the merge half, which is presence-based rather than
// value-based: an absent id takes the default, an id with [] takes nothing.

#include <QTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "corbomite/core/Hotkey.h"
#include "corbomite/core/HotkeyManager.h"

using Corbomite::Hotkey;
using Corbomite::HotkeyManager;
using Corbomite::HotkeyModifier;
using Corbomite::HotkeyFile;
using Corbomite::obsidianDefaultHotkeys;

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

    // ---- `code` field (Obsidian bake(): key: a.code ? Xw(a.code) : a.key)

    void testParseReadsCodeField()
    {
        const QByteArray json =
            R"({"editor:toggle-italics":[{"modifiers":["Mod"],"key":"^","code":"KeyI"}]})";
        auto f = HotkeyFile::parse(json);
        const auto &h = f.bindings[QStringLiteral("editor:toggle-italics")].first();
        QCOMPARE(h.key, QStringLiteral("^"));
        QCOMPARE(h.code, QStringLiteral("KeyI"));
        // Xw returns charAt(3) verbatim, so the resolved key is the UPPERCASE
        // letter; isMatch() compares case-insensitively (0747-ly.js:130-133).
        QCOMPARE(h.resolvedKey(), QStringLiteral("I"));
    }

    void testCodeWinsOverKeyWhenResolving()
    {
        Hotkey h{{HotkeyModifier::Mod}, QStringLiteral("i")};
        QCOMPARE(h.resolvedKey(), QStringLiteral("i"));
        h.code = QStringLiteral("KeyI");
        QCOMPARE(h.resolvedKey(), QStringLiteral("I"));
        h.key = QStringLiteral("^");
        QCOMPARE(h.resolvedKey(), QStringLiteral("I"));
    }

    void testCodeOnlyStripsFourCharacterKeyPrefix()
    {
        // Xw (app/own/0831-Xw.js:2-4): startsWith("Key") && 4 === length.
        Hotkey h;
        h.code = QStringLiteral("KeyArrowLeft");
        QCOMPARE(h.resolvedKey(), QStringLiteral("KeyArrowLeft"));
        h.code = QStringLiteral("ArrowLeft");
        QCOMPARE(h.resolvedKey(), QStringLiteral("ArrowLeft"));
        h.code = QStringLiteral("Keyboard");
        QCOMPARE(h.resolvedKey(), QStringLiteral("Keyboard"));
        h.code = QStringLiteral("Digit1");
        QCOMPARE(h.resolvedKey(), QStringLiteral("Digit1"));
    }

    void testCodeRoundTripsThroughSerialise()
    {
        HotkeyFile f;
        Hotkey h{{HotkeyModifier::Mod}, QStringLiteral("^"), QStringLiteral("KeyI")};
        f.bindings.insert(QStringLiteral("editor:toggle-italics"), {h});
        const auto reparsed = HotkeyFile::parse(f.serialise());
        QCOMPARE(reparsed.bindings[QStringLiteral("editor:toggle-italics")],
                 QList<Hotkey>{h});
    }

    // ---- HotkeyManager: override-only merge (Obsidian bake(), 0833-$w.js:73-74)

    void testManagerFallsBackToDefaultWhenIdAbsent()
    {
        HotkeyManager m;
        const auto defaults = obsidianDefaultHotkeys(false);
        for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
            m.addDefaultHotkeys(it.key(), it.value());
        }
        QVERIFY(m.load(R"({"editor:toggle-bold":[{"modifiers":["Mod"],"key":"j"}]})"));
        // Untouched id keeps the Obsidian default.
        QCOMPARE(m.effectiveHotkeys(QStringLiteral("editor:toggle-italics")),
                 (QList<Hotkey>{Hotkey{{HotkeyModifier::Mod}, QStringLiteral("I")}}));
        // Overridden id loses it.
        QCOMPARE(m.effectiveHotkeys(QStringLiteral("editor:toggle-bold")),
                 (QList<Hotkey>{Hotkey{{HotkeyModifier::Mod}, QStringLiteral("j")}}));
    }

    void testManagerEmptyArrayUnbindsAndDoesNotRestoreDefault()
    {
        HotkeyManager m;
        const auto defaults = obsidianDefaultHotkeys(false);
        QCOMPARE(defaults.value(QStringLiteral("editor:toggle-bold")).size(), 1);

        QVERIFY(m.load(R"({"editor:toggle-bold":[]})"));
        m.addDefaultHotkeys(QStringLiteral("editor:toggle-bold"),
                            defaults.value(QStringLiteral("editor:toggle-bold")));
        m.addDefaultHotkeys(QStringLiteral("editor:toggle-italics"),
                            defaults.value(QStringLiteral("editor:toggle-italics")));

        // The falsifiable half: an empty override list means ZERO bindings.
        // A reader that treats "[]" as "no opinion" would restore Mod+B here.
        QCOMPARE(m.effectiveHotkeys(QStringLiteral("editor:toggle-bold")),
                 QList<Hotkey>{});
        QVERIFY(!m.effectiveHotkeys(QStringLiteral("editor:toggle-bold")).size());
        QVERIFY(!m.isBound(QStringLiteral("editor:toggle-bold")));
        // An absent id is a different state and still resolves to the default.
        QVERIFY(!m.customHotkeys(QStringLiteral("editor:toggle-italics")).has_value());
        QVERIFY(m.isBound(QStringLiteral("editor:toggle-italics")));
    }

    void testManagerOverrideReplacesRatherThanAugmentsDefaults()
    {
        HotkeyManager m;
        m.addDefaultHotkeys(QStringLiteral("app:go-back"),
                            {Hotkey{{HotkeyModifier::Mod, HotkeyModifier::Alt},
                                    QStringLiteral("ArrowLeft")}});
        QVERIFY(m.load(R"({"app:go-back":[{"modifiers":["Ctrl"],"key":"g"}]})"));
        const auto eff = m.effectiveHotkeys(QStringLiteral("app:go-back"));
        QCOMPARE(eff.size(), 1);
        QCOMPARE(eff.first().modifiers, QList<HotkeyModifier>{HotkeyModifier::Ctrl});
    }

    void testManagerUnbindSurvivesSaveAndReload()
    {
        HotkeyManager m;
        m.addDefaultHotkeys(QStringLiteral("editor:toggle-bold"),
                            {Hotkey{{HotkeyModifier::Mod}, QStringLiteral("B")}});
        QVERIFY(m.load(R"({"editor:toggle-bold":[]})"));

        HotkeyManager reloaded;
        reloaded.addDefaultHotkeys(QStringLiteral("editor:toggle-bold"),
                                   {Hotkey{{HotkeyModifier::Mod}, QStringLiteral("B")}});
        QVERIFY(reloaded.load(m.save()));
        QCOMPARE(reloaded.effectiveHotkeys(QStringLiteral("editor:toggle-bold")),
                 QList<Hotkey>{});
    }

    void testManagerSaveWritesOverridesOnly()
    {
        HotkeyManager m;
        const auto defaults = obsidianDefaultHotkeys(false);
        for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
            m.addDefaultHotkeys(it.key(), it.value());
        }
        QVERIFY(m.load(R"({"editor:toggle-bold":[{"modifiers":["Mod"],"key":"j"}]})"));

        const auto out = m.save();
        const auto doc = QJsonDocument::fromJson(out);
        QVERIFY(doc.isObject());
        const auto obj = doc.object();
        QCOMPARE(obj.size(), 1);
        QVERIFY(obj.contains(QStringLiteral("editor:toggle-bold")));
        // A default leaking into the file would become a user override on the
        // next load (Obsidian save() writes customKeys verbatim, 0833-$w.js:24).
        QVERIFY(!out.contains("editor:toggle-italics"));
    }

    void testManagerLoadIgnoresNonObjectBody()
    {
        HotkeyManager m;
        QVERIFY(m.load(R"({"editor:toggle-bold":[{"modifiers":["Mod"],"key":"j"}]})"));
        QVERIFY(!m.load(QByteArray("[1, 2, 3]")));
        QVERIFY(!m.load(R"("a string")"));
        QVERIFY(!m.load(R"(42)"));
        QVERIFY(!m.load(QByteArray("{ this is not json")));
        // Obsidian's typeof === "object" gate leaves the previous map in place.
        QCOMPARE(m.effectiveHotkeys(QStringLiteral("editor:toggle-bold")).first().key,
                 QStringLiteral("j"));
    }

    void testManagerUnknownIdsSurviveWithoutDefaults()
    {
        HotkeyManager m;
        QVERIFY(m.load(R"({"bookmarks:bookmark-current-file":[{"modifiers":["Mod"],"key":"Alt+D"}]})"));
        QCOMPARE(m.effectiveHotkeys(QStringLiteral("bookmarks:bookmark-current-file")).size(), 1);
        QCOMPARE(m.overriddenIds(),
                 QStringList{QStringLiteral("bookmarks:bookmark-current-file")});
    }

    void testManagerSetAndRemoveCustomHotkeys()
    {
        HotkeyManager m;
        m.addDefaultHotkeys(QStringLiteral("app:open-help"),
                            {Hotkey{{}, QStringLiteral("F1")}});
        m.setCustomHotkeys(QStringLiteral("app:open-help"),
                           {Hotkey{{HotkeyModifier::Ctrl}, QStringLiteral("H")}});
        QCOMPARE(m.effectiveHotkeys(QStringLiteral("app:open-help")).first().key,
                 QStringLiteral("H"));
        m.removeCustomHotkeys(QStringLiteral("app:open-help"));
        QCOMPARE(m.effectiveHotkeys(QStringLiteral("app:open-help")).first().key,
                 QStringLiteral("F1"));
        m.removeDefaultHotkeys(QStringLiteral("app:open-help"));
        QVERIFY(!m.isBound(QStringLiteral("app:open-help")));
    }

    // ---- Obsidian's built-in default table

    void testDefaultTableCoversEveryHotkeyBearingCommandId()
    {
        // 41 ids, enumerated by grepping `hotkeys: [` across every bundle:
        //  16  App.registerCommands      (app/own/2075-Vie.js)
        //  21  Workspace                 (app/own/1932-r4.js, counting goto-tab-1..8)
        //   4  core plugins              (global-search, graph, command-palette, switcher)
        QCOMPARE(obsidianDefaultHotkeys(false).size(), 41);
        QCOMPARE(obsidianDefaultHotkeys(true).size(), 41);
    }

    void testDefaultTableIncludesCorePluginCommands()
    {
        // These four are the only core-plugin commands in 1.13.7 with a
        // `hotkeys:` field. `registerGlobalCommand` reaches `app.commands`
        // (and therefore addDefaultHotkeys, 1963-s5.js:22) only when the
        // plugin is enabled; all four owners are `defaultOn`.
        const auto t = obsidianDefaultHotkeys(false);
        QCOMPARE(t.value(QStringLiteral("global-search:open")),
                 (QList<Hotkey>{Hotkey{{HotkeyModifier::Mod, HotkeyModifier::Shift},
                                       QStringLiteral("F")}}));
        QCOMPARE(t.value(QStringLiteral("graph:open")),
                 (QList<Hotkey>{Hotkey{{HotkeyModifier::Mod}, QStringLiteral("G")}}));
        QCOMPARE(t.value(QStringLiteral("command-palette:open")),
                 (QList<Hotkey>{Hotkey{{HotkeyModifier::Mod}, QStringLiteral("P")}}));
        QCOMPARE(t.value(QStringLiteral("switcher:open")),
                 (QList<Hotkey>{Hotkey{{HotkeyModifier::Mod}, QStringLiteral("O")}}));
        // Sibling plugin commands with no `hotkeys:` field must stay absent,
        // or bake() would invent a default Obsidian never had.
        for (const auto &id : {"graph:open-local", "graph:animate",
                               "global-search:quick-switcher"}) {
            QVERIFY(!t.contains(QString(QLatin1String(id))));
        }
    }

    void testDefaultTableHasNoPlatformIndependentDuplicates()
    {
        const auto win = obsidianDefaultHotkeys(false);
        const auto mac = obsidianDefaultHotkeys(true);
        const QStringList conditional = {
            QStringLiteral("editor:open-search-replace"),
            QStringLiteral("workspace:next-tab"),
            QStringLiteral("workspace:previous-tab"),
        };
        for (const auto &id : conditional) {
            QVERIFY(win.value(id) != mac.value(id));
        }
        QCOMPARE(win.value(QStringLiteral("editor:open-search-replace")).first().key,
                 QStringLiteral("H"));
        QCOMPARE(mac.value(QStringLiteral("editor:open-search-replace")).first().key,
                 QStringLiteral("F"));
        // "and only those": every other id must be platform-invariant. Without
        // this the loop above would still pass if some other row silently grew
        // a Zp-branch that macOs never selects.
        for (auto it = win.cbegin(); it != win.cend(); ++it) {
            if (conditional.contains(it.key())) continue;
            QCOMPARE(mac.value(it.key()), it.value());
        }
    }

    void testDefaultTableSpotChecks()
    {
        const auto t = obsidianDefaultHotkeys(false);
        QCOMPARE(t.value(QStringLiteral("app:go-back")),
                 (QList<Hotkey>{Hotkey{{HotkeyModifier::Mod, HotkeyModifier::Alt},
                                       QStringLiteral("ArrowLeft")}}));
        // Yw([], "F1") — no modifiers means "F1 with nothing held".
        QCOMPARE(t.value(QStringLiteral("app:open-help")),
                 (QList<Hotkey>{Hotkey{{}, QStringLiteral("F1")}}));
        QCOMPARE(t.value(QStringLiteral("markdown:add-metadata-property")),
                 (QList<Hotkey>{Hotkey{{HotkeyModifier::Mod}, QStringLiteral(";")}}));
        for (int n = 1; n <= 8; ++n) {
            QCOMPARE(t.value(QStringLiteral("workspace:goto-tab-%1").arg(n)),
                     (QList<Hotkey>{Hotkey{{HotkeyModifier::Mod}, QString::number(n)}}));
        }
        QVERIFY(!t.contains(QStringLiteral("workspace:goto-tab-9")));
    }
};

QTEST_MAIN(TestHotkey)
#include "tst_hotkey.moc"
