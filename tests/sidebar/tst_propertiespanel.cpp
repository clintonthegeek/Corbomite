// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster L: Properties panel — unit + integration tests.
//
// Covers:
//   - Type inference rules (Text / Number / Checkbox / Date / DateTime / List).
//   - Per-type editor round-trip (setValue → currentValue).
//   - PropertiesPanel refresh against a real MetadataCache.
//   - Write-back via FrontMatterWriter with debounce flush.

#include <QTest>
#include <QSignalSpy>

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <QTemporaryDir>

#include <markoff/parser/YamlValue.h>

#include "corbomite/core/NoteDocument.h"
#include "corbomite/models/PropertyType.h"
#include "corbomite/models/PropertyTypeInference.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"

#include "PropertiesPanel.h"
#include "PropertyEditorWidget.h"

using namespace Corbomite;

namespace {

Markoff::YamlValue makeScalar(const QString &yaml)
{
    // Parse "k: <value>" and return the single child.
    auto root = Markoff::YamlValue::parse(QStringLiteral("k: ") + yaml);
    return root.get(QStringLiteral("k"));
}

Markoff::YamlValue makeBool(bool v)
{
    return makeScalar(v ? QStringLiteral("true") : QStringLiteral("false"));
}

Markoff::YamlValue makeInt(int v)
{
    return makeScalar(QString::number(v));
}

Markoff::YamlValue makeString(const QString &s)
{
    // Force string kind — quote the value.
    auto root = Markoff::YamlValue::emptyMap();
    root.setString(QStringLiteral("k"), s);
    return root.get(QStringLiteral("k"));
}

Markoff::YamlValue makeNull()
{
    auto root = Markoff::YamlValue::emptyMap();
    root.setNull(QStringLiteral("k"));
    return root.get(QStringLiteral("k"));
}

Markoff::YamlValue makeSeq(const QStringList &items)
{
    auto root = Markoff::YamlValue::emptyMap();
    root.setSeq(QStringLiteral("k"), items);
    return root.get(QStringLiteral("k"));
}

void pumpEvents(int ms = 10)
{
    for (int i = 0; i < ms; ++i) {
        QCoreApplication::processEvents();
        QTest::qWait(1);
    }
}

}  // namespace

class TestPropertiesPanel : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // --- Type inference ------------------------------------------------

    void testInferTextDefault()
    {
        QCOMPARE(inferPropertyType(makeString(QStringLiteral("hello"))),
                 PropertyType::Text);
    }

    void testInferCheckbox()
    {
        QCOMPARE(inferPropertyType(makeBool(true)), PropertyType::Checkbox);
        QCOMPARE(inferPropertyType(makeBool(false)), PropertyType::Checkbox);
    }

    void testInferNumber()
    {
        QCOMPARE(inferPropertyType(makeInt(42)), PropertyType::Number);
        QCOMPARE(inferPropertyType(makeScalar(QStringLiteral("3.14"))),
                 PropertyType::Number);
    }

    void testInferDate()
    {
        QCOMPARE(inferPropertyType(makeString(QStringLiteral("2026-04-15"))),
                 PropertyType::Date);
    }

    void testInferDateTime()
    {
        QCOMPARE(inferPropertyType(makeString(QStringLiteral("2026-04-15T14:30:00"))),
                 PropertyType::DateTime);
    }

    void testInferList()
    {
        QCOMPARE(inferPropertyType(makeSeq({QStringLiteral("a"),
                                            QStringLiteral("b")})),
                 PropertyType::List);
    }

    void testInferNullIsText()
    {
        QCOMPARE(inferPropertyType(makeNull()), PropertyType::Text);
    }

    void testInferRejectsDateLikePrefix()
    {
        // "2026-04-15 is my birthday" contains an ISO prefix but is free text.
        QCOMPARE(inferPropertyType(makeString(
                     QStringLiteral("2026-04-15 is my birthday"))),
                 PropertyType::Text);
    }

    // --- QJsonValue → YamlValue conversion -----------------------------

    void testJsonToYamlBool()
    {
        const auto y = qJsonValueToYaml(QJsonValue(true));
        QVERIFY(y.isBool());
        QCOMPARE(y.asBool(), true);
    }

    void testJsonToYamlInt()
    {
        const auto y = qJsonValueToYaml(QJsonValue(42));
        QVERIFY(y.isInt() || y.isDouble());
        QCOMPARE(static_cast<int>(y.isInt() ? y.asInt() : y.asDouble()), 42);
    }

    void testJsonToYamlString()
    {
        const auto y = qJsonValueToYaml(QJsonValue(QStringLiteral("hi")));
        QVERIFY(y.isString());
        QCOMPARE(y.asString(), QStringLiteral("hi"));
    }

    void testJsonToYamlArray()
    {
        QJsonArray arr{QStringLiteral("a"), QStringLiteral("b")};
        const auto y = qJsonValueToYaml(QJsonValue(arr));
        QVERIFY(y.isSeq());
        QCOMPARE(y.size(), 2);
        QCOMPARE(y.at(0).asString(), QStringLiteral("a"));
        QCOMPARE(y.at(1).asString(), QStringLiteral("b"));
    }

    // --- Editor round-trips --------------------------------------------

    void testTextEditorRoundTrip()
    {
        TextPropertyEditor e;
        e.setValue(makeString(QStringLiteral("hi")));
        QCOMPARE(e.currentValue().asString(), QStringLiteral("hi"));

        e.setValue(makeString(QStringLiteral("world")));
        QCOMPARE(e.currentValue().asString(), QStringLiteral("world"));
    }

    void testCheckboxEditorRoundTrip()
    {
        CheckboxPropertyEditor e;
        e.setValue(makeBool(true));
        QCOMPARE(e.currentValue().asBool(), true);

        e.setValue(makeBool(false));
        QCOMPARE(e.currentValue().asBool(), false);
    }

    void testNumberEditorRoundTrip()
    {
        NumberPropertyEditor e;
        e.setValue(makeInt(42));
        const auto v = e.currentValue();
        QVERIFY(v.isInt() || v.isDouble());
        const int64_t n = v.isInt() ? v.asInt() : static_cast<int64_t>(v.asDouble());
        QCOMPARE(static_cast<int>(n), 42);
    }

    void testDateEditorRoundTrip()
    {
        DatePropertyEditor e;
        e.setValue(makeString(QStringLiteral("2026-04-15")));
        QCOMPARE(e.currentValue().asString(), QStringLiteral("2026-04-15"));
    }

    void testDateTimeEditorRoundTrip()
    {
        DateTimePropertyEditor e;
        e.setValue(makeString(QStringLiteral("2026-04-15T14:30:00")));
        QCOMPARE(e.currentValue().asString(),
                 QStringLiteral("2026-04-15T14:30:00"));
    }

    void testListEditorRoundTrip()
    {
        ListPropertyEditor e;
        const auto seq = makeSeq({QStringLiteral("a"),
                                  QStringLiteral("b"),
                                  QStringLiteral("c")});
        e.setValue(seq);
        const auto out = e.currentValue();
        QVERIFY(out.isSeq());
        QCOMPARE(out.size(), 3);
        QCOMPARE(out.at(0).asString(), QStringLiteral("a"));
        QCOMPARE(out.at(1).asString(), QStringLiteral("b"));
        QCOMPARE(out.at(2).asString(), QStringLiteral("c"));
    }

    void testFactoryPicksRightEditor()
    {
        auto *txt = makePropertyEditor(PropertyType::Text,
                                       makeString(QStringLiteral("hi")));
        QVERIFY(qobject_cast<TextPropertyEditor *>(txt));
        delete txt;

        auto *ck = makePropertyEditor(PropertyType::Checkbox, makeBool(true));
        QVERIFY(qobject_cast<CheckboxPropertyEditor *>(ck));
        delete ck;

        auto *num = makePropertyEditor(PropertyType::Number, makeInt(3));
        QVERIFY(qobject_cast<NumberPropertyEditor *>(num));
        delete num;

        auto *dt = makePropertyEditor(PropertyType::Date,
                                      makeString(QStringLiteral("2026-01-01")));
        QVERIFY(qobject_cast<DatePropertyEditor *>(dt));
        delete dt;

        auto *dtt = makePropertyEditor(PropertyType::DateTime,
                                       makeString(QStringLiteral("2026-01-01T00:00:00")));
        QVERIFY(qobject_cast<DateTimePropertyEditor *>(dtt));
        delete dtt;

        auto *lst = makePropertyEditor(PropertyType::List,
                                       makeSeq({QStringLiteral("a")}));
        QVERIFY(qobject_cast<ListPropertyEditor *>(lst));
        delete lst;
    }

    // --- Panel integration ---------------------------------------------

    void testPanelHidesWhenNoCurrentDoc()
    {
        PropertiesPanel panel;
        QCOMPARE(panel.rowCount(), 0);
    }

    void testPanelShowsFrontmatter()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString vaultRoot = tmp.path();

        const QString noteRel = QStringLiteral("a.md");
        const QString notePath = vaultRoot + QLatin1Char('/') + noteRel;

        const QByteArray content =
            "---\n"
            "title: Hello\n"
            "done: true\n"
            "count: 7\n"
            "---\n"
            "# Body\n";
        {
            QFile f(notePath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
            f.close();
        }

        LinkResolver resolver;
        resolver.setVaultPaths({noteRel});
        MetadataCache cache(resolver);

        QSignalSpy changedSpy(&cache, &MetadataCache::cacheChanged);
        cache.onFileChanged(noteRel, content, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(changedSpy.count(), 1, 2000);

        NoteDocument doc(vaultRoot, noteRel);

        PropertiesPanel panel;
        panel.setMetadataCache(&cache);
        panel.setCurrentNote(&doc);

        QCOMPARE(panel.rowCount(), 3);
    }

    void testPanelEmptyWhenNoFrontmatter()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString vaultRoot = tmp.path();
        const QString noteRel = QStringLiteral("nofm.md");
        const QString notePath = vaultRoot + QLatin1Char('/') + noteRel;

        const QByteArray content = "# Heading only\n";
        {
            QFile f(notePath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
            f.close();
        }

        LinkResolver resolver;
        resolver.setVaultPaths({noteRel});
        MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &MetadataCache::cacheChanged);
        cache.onFileChanged(noteRel, content, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);

        NoteDocument doc(vaultRoot, noteRel);
        PropertiesPanel panel;
        panel.setMetadataCache(&cache);
        panel.setCurrentNote(&doc);

        QCOMPARE(panel.rowCount(), 0);
    }

    void testPanelWritebackThroughFileManager()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString vaultRoot = tmp.path();
        const QString noteRel = QStringLiteral("w.md");
        const QString notePath = vaultRoot + QLatin1Char('/') + noteRel;

        const QByteArray content =
            "---\n"
            "title: Old\n"
            "---\n"
            "Body\n";
        {
            QFile f(notePath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
            f.close();
        }

        LinkResolver resolver;
        resolver.setVaultPaths({noteRel});
        MetadataCache cache(resolver);
        QSignalSpy cSpy(&cache, &MetadataCache::cacheChanged);
        cache.onFileChanged(noteRel, content, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(cSpy.count(), 1, 2000);

        FileSystemAdapter fs;
        Vault vault(&fs);
        vault.load(vaultRoot);
        FileManager fileManager(&vault, &cache);

        NoteDocument doc(vaultRoot, noteRel);
        PropertiesPanel panel;
        QSignalSpy writtenSpy(&panel, &PropertiesPanel::propertiesWritten);

        panel.setMetadataCache(&cache);
        panel.setVault(&vault);
        panel.setFileManager(&fileManager);
        panel.setCurrentNote(&doc);
        QCOMPARE(panel.rowCount(), 1);

        // Set the title editor's value directly via our internal API —
        // we don't have widget-level access so we just write via
        // addPropertyNamed to add a new key, then flush. Actually we
        // can reuse addPropertyNamed which exercises the same path.
        panel.addPropertyNamed(QStringLiteral("status"));
        panel.flushPendingWrite();

        QTRY_COMPARE_WITH_TIMEOUT(writtenSpy.count(), 1, 2000);

        // Read the file back; verify frontmatter has "status" key.
        QFile rf(notePath);
        QVERIFY(rf.open(QIODevice::ReadOnly));
        const QByteArray updated = rf.readAll();
        rf.close();
        QVERIFY(updated.contains("status"));
        QVERIFY(updated.contains("title"));
    }

    void testPanelAddPropertyNamed()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString vaultRoot = tmp.path();
        const QString noteRel = QStringLiteral("a.md");
        const QString notePath = vaultRoot + QLatin1Char('/') + noteRel;

        const QByteArray content = "---\ntitle: X\n---\nBody\n";
        {
            QFile f(notePath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
            f.close();
        }

        LinkResolver resolver;
        resolver.setVaultPaths({noteRel});
        MetadataCache cache(resolver);
        QSignalSpy cSpy(&cache, &MetadataCache::cacheChanged);
        cache.onFileChanged(noteRel, content, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(cSpy.count(), 1, 2000);

        NoteDocument doc(vaultRoot, noteRel);
        PropertiesPanel panel;
        panel.setMetadataCache(&cache);
        panel.setCurrentNote(&doc);

        const int before = panel.rowCount();
        panel.addPropertyNamed(QStringLiteral("new_prop"));
        QCOMPARE(panel.rowCount(), before + 1);

        // Duplicate call should be a no-op.
        panel.addPropertyNamed(QStringLiteral("new_prop"));
        QCOMPARE(panel.rowCount(), before + 1);
    }

    void testClearCurrentNoteClears()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString vaultRoot = tmp.path();
        const QString noteRel = QStringLiteral("a.md");
        const QString notePath = vaultRoot + QLatin1Char('/') + noteRel;

        const QByteArray content = "---\ntitle: X\n---\nBody\n";
        {
            QFile f(notePath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
            f.close();
        }

        LinkResolver resolver;
        resolver.setVaultPaths({noteRel});
        MetadataCache cache(resolver);
        QSignalSpy cSpy(&cache, &MetadataCache::cacheChanged);
        cache.onFileChanged(noteRel, content, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(cSpy.count(), 1, 2000);

        NoteDocument doc(vaultRoot, noteRel);
        PropertiesPanel panel;
        panel.setMetadataCache(&cache);
        panel.setCurrentNote(&doc);
        QVERIFY(panel.rowCount() >= 1);

        panel.setCurrentNote(nullptr);
        QCOMPARE(panel.rowCount(), 0);

        Q_UNUSED(notePath);
    }
};

QTEST_MAIN(TestPropertiesPanel)
#include "tst_propertiespanel.moc"
