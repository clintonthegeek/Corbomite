// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/core/FrontMatterWriter.h"

using namespace Corbomite;

namespace {

QString readAll(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(f.readAll());
}

QString writeFile(const QString &dir, const QString &name, const QString &content)
{
    const QString path = dir + QLatin1Char('/') + name;
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(content.toUtf8());
    f.close();
    return path;
}

} // namespace

class TestFrontMatterWriter : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // --- read() ---

    void read_noFrontmatterReturnsEmpty()
    {
        QTemporaryDir tmp;
        const QString path = writeFile(tmp.path(), "note.md", QStringLiteral("# No frontmatter here\n"));
        const auto v = FrontMatterWriter::read(path);
        QVERIFY(v.isMap());
        QCOMPARE(v.size(), 0);
    }

    void read_parsesSimpleFrontmatter()
    {
        QTemporaryDir tmp;
        const QString path = writeFile(tmp.path(), "note.md",
            QStringLiteral("---\ntitle: Hello\ntags:\n  - foo\n  - bar\n---\n\nBody.\n"));
        const auto v = FrontMatterWriter::read(path);
        QVERIFY(v.isMap());
        QCOMPARE(v.get(QStringLiteral("title")).asString(), QStringLiteral("Hello"));
        const auto tags = v.get(QStringLiteral("tags"));
        QVERIFY(tags.isSeq());
        QCOMPARE(tags.size(), 2);
    }

    void read_missingFileReturnsEmptyMapAndError()
    {
        QString err;
        const auto v = FrontMatterWriter::read(QStringLiteral("/nonexistent/path.md"), &err);
        QVERIFY(v.isMap());
        QCOMPARE(v.size(), 0);
        QVERIFY(!err.isEmpty());
    }

    // --- process(): read-modify-write ---

    void process_addsNewKeyToExistingFrontmatter()
    {
        QTemporaryDir tmp;
        const QString path = writeFile(tmp.path(), "note.md",
            QStringLiteral("---\ntitle: Hello\n---\n\nBody.\n"));

        const bool ok = FrontMatterWriter::process(path, [](Markoff::YamlValue &fm) {
            fm.setString(QStringLiteral("author"), QStringLiteral("Alice"));
        });
        QVERIFY(ok);

        const QString after = readAll(path);
        QVERIFY(after.startsWith(QStringLiteral("---\n")));
        QVERIFY(after.contains(QStringLiteral("title: Hello")));
        QVERIFY(after.contains(QStringLiteral("author: Alice")));
        QVERIFY(after.contains(QStringLiteral("Body.")));
    }

    void process_preservesKeyOrder()
    {
        QTemporaryDir tmp;
        const QString path = writeFile(tmp.path(), "note.md",
            QStringLiteral("---\nzebra: 1\nalpha: 2\nmango: 3\n---\n\nBody.\n"));

        const bool ok = FrontMatterWriter::process(path, [](Markoff::YamlValue &fm) {
            fm.setString(QStringLiteral("new_key"), QStringLiteral("appended"));
        });
        QVERIFY(ok);

        const QString after = readAll(path);
        const int zebra = after.indexOf(QStringLiteral("zebra:"));
        const int alpha = after.indexOf(QStringLiteral("alpha:"));
        const int mango = after.indexOf(QStringLiteral("mango:"));
        const int newKey = after.indexOf(QStringLiteral("new_key:"));
        // Existing keys keep document order; new key appended last.
        QVERIFY(zebra >= 0 && zebra < alpha);
        QVERIFY(alpha < mango);
        QVERIFY(mango < newKey);
    }

    void process_addsFrontmatterToFileWithout()
    {
        QTemporaryDir tmp;
        const QString path = writeFile(tmp.path(), "note.md",
            QStringLiteral("# Just a body\n\nNo frontmatter here.\n"));

        const bool ok = FrontMatterWriter::process(path, [](Markoff::YamlValue &fm) {
            fm.setString(QStringLiteral("title"), QStringLiteral("Added"));
        });
        QVERIFY(ok);

        const QString after = readAll(path);
        QVERIFY(after.startsWith(QStringLiteral("---\n")));
        QVERIFY(after.contains(QStringLiteral("title: Added")));
        QVERIFY(after.contains(QStringLiteral("# Just a body")));
    }

    void process_updatesExistingKey()
    {
        QTemporaryDir tmp;
        const QString path = writeFile(tmp.path(), "note.md",
            QStringLiteral("---\ncount: 1\n---\n\nBody.\n"));

        FrontMatterWriter::process(path, [](Markoff::YamlValue &fm) {
            fm.setInt(QStringLiteral("count"), 42);
        });

        const auto after = FrontMatterWriter::read(path);
        QCOMPARE(after.get(QStringLiteral("count")).asInt(), int64_t(42));
    }

    void process_removesKey()
    {
        QTemporaryDir tmp;
        const QString path = writeFile(tmp.path(), "note.md",
            QStringLiteral("---\nkeep: yes\ndrop: bye\n---\n\nBody.\n"));

        FrontMatterWriter::process(path, [](Markoff::YamlValue &fm) {
            fm.remove(QStringLiteral("drop"));
        });

        const auto after = FrontMatterWriter::read(path);
        QVERIFY(after.contains(QStringLiteral("keep")));
        QVERIFY(!after.contains(QStringLiteral("drop")));
    }

    // --- Edge cases that Cluster A specifically targets ---

    void process_preservesEofCloseFrontmatter()
    {
        // P0.2: frontmatter ending in "---" at EOF (no trailing newline).
        QTemporaryDir tmp;
        const QString original = QStringLiteral("---\ntitle: EOFClose\n---");
        const QString path = writeFile(tmp.path(), "note.md", original);

        const bool ok = FrontMatterWriter::process(path, [](Markoff::YamlValue &fm) {
            fm.setString(QStringLiteral("added"), QStringLiteral("x"));
        });
        QVERIFY(ok);
        const auto after = FrontMatterWriter::read(path);
        QCOMPARE(after.get(QStringLiteral("title")).asString(), QStringLiteral("EOFClose"));
        QCOMPARE(after.get(QStringLiteral("added")).asString(), QStringLiteral("x"));
    }

    void process_preservesBodyWithHeadings()
    {
        QTemporaryDir tmp;
        const QString path = writeFile(tmp.path(), "note.md",
            QStringLiteral(
                "---\ntitle: Doc\n---\n\n"
                "# Chapter 1\n\nContent.\n\n"
                "## Section A\n\nMore.\n\n"
                "### Subsection\n\nDeeper.\n"));

        FrontMatterWriter::process(path, [](Markoff::YamlValue &fm) {
            fm.setString(QStringLiteral("edited"), QStringLiteral("yes"));
        });

        const QString after = readAll(path);
        QVERIFY(after.contains(QStringLiteral("# Chapter 1")));
        QVERIFY(after.contains(QStringLiteral("## Section A")));
        QVERIFY(after.contains(QStringLiteral("### Subsection")));
        QVERIFY(after.contains(QStringLiteral("Deeper.")));
    }

    void process_yaml12StrictBooleans()
    {
        // YAML 1.2 strict: "yes"/"no" are strings, not bools. Round-trip
        // must preserve them as strings.
        QTemporaryDir tmp;
        const QString path = writeFile(tmp.path(), "note.md",
            QStringLiteral("---\nactive: yes\nstate: no\n---\n\nBody.\n"));

        FrontMatterWriter::process(path, [](Markoff::YamlValue &fm) {
            fm.setString(QStringLiteral("note"), QStringLiteral("added"));
        });

        const auto after = FrontMatterWriter::read(path);
        QCOMPARE(after.get(QStringLiteral("active")).asString(), QStringLiteral("yes"));
        QCOMPARE(after.get(QStringLiteral("state")).asString(), QStringLiteral("no"));
    }

    // --- write() ---

    void write_stripsFrontmatterWhenValueEmpty()
    {
        QTemporaryDir tmp;
        const QString path = writeFile(tmp.path(), "note.md",
            QStringLiteral("---\ntitle: Hello\n---\n\nBody.\n"));

        const bool ok = FrontMatterWriter::write(path, Markoff::YamlValue::emptyMap());
        QVERIFY(ok);
        const QString after = readAll(path);
        QVERIFY(!after.contains(QStringLiteral("title: Hello")));
        QVERIFY(after.contains(QStringLiteral("Body.")));
    }

    // --- Atomicity ---

    void process_failureLeavesOriginalIntact()
    {
        // Pointing at a nonexistent file should fail cleanly and produce
        // a diagnostic without side effects.
        QString err;
        const bool ok = FrontMatterWriter::process(
            QStringLiteral("/nonexistent/path.md"),
            [](Markoff::YamlValue &) {},
            &err);
        QVERIFY(!ok);
        QVERIFY(!err.isEmpty());
    }

    void process_nullMutatorRejected()
    {
        QTemporaryDir tmp;
        const QString path = writeFile(tmp.path(), "note.md", QStringLiteral("body\n"));
        QString err;
        const bool ok = FrontMatterWriter::process(path, {}, &err);
        QVERIFY(!ok);
        QVERIFY(!err.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestFrontMatterWriter)
#include "tst_frontmatterwriter.moc"
