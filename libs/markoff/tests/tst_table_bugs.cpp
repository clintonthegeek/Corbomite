// SPDX-License-Identifier: GPL-3.0-or-later
// Reproduction tests for showcase table rendering bugs
#include <QObject>
#include <QTest>
#include <QApplication>
#include <markoff/Editor.h>

using namespace Markoff;

class TestTableBugs : public QObject {
    Q_OBJECT

private slots:
    void noRemnantTextAroundTables();
    void textAfterTablesPreserved();
    void twoTablesRoundTrip();
};

void TestTableBugs::noRemnantTextAroundTables()
{
    Editor editor;
    editor.resize(800, 600);

    QString md = QStringLiteral(
        "# Heading\n"
        "\n"
        "Text before.\n"
        "\n"
        "| A | B |\n"
        "|---|---|\n"
        "| 1 | 2 |\n"
        "\n"
        "| X | Y |\n"
        "|---|---|\n"
        "| 3 | 4 |\n"
        "\n"
        "## After\n"
        "\n"
        "Text after tables.");

    editor.setPlainText(md);
    editor.show();
    QTest::qWaitForWindowExposed(&editor);

    QString output = editor.toPlainText();
    qDebug() << "OUTPUT:" << output;

    // Should NOT contain stray single-char lines (the 'e' and 's' bugs)
    QStringList lines = output.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];
        if (line.trimmed().length() == 1) {
            QChar c = line.trimmed().at(0);
            // Allow '#' (heading marker) and '|' (table pipe) and '-' (separator)
            if (c != QLatin1Char('#') && c != QLatin1Char('|') && c != QLatin1Char('-')) {
                QFAIL(qPrintable(QStringLiteral("Remnant char '%1' at line %2")
                    .arg(c).arg(i)));
            }
        }
    }

    // Should NOT contain partial pipe text like "| A | B" outside a table row
    // (the table should be fully serialized or fully converted)
    QVERIFY2(output.contains(QLatin1Char('A')), "Table A content missing");
    QVERIFY2(output.contains(QLatin1Char('X')), "Table X content missing");
}

void TestTableBugs::textAfterTablesPreserved()
{
    Editor editor;
    editor.resize(800, 600);

    QString md = QStringLiteral(
        "Before\n"
        "\n"
        "| H |\n"
        "|---|\n"
        "| V |\n"
        "\n"
        "## After\n"
        "\n"
        "Paragraph after table.");

    editor.setPlainText(md);
    editor.show();
    QTest::qWaitForWindowExposed(&editor);

    QString output = editor.toPlainText();
    qDebug() << "OUTPUT:" << output;

    QVERIFY2(output.contains(QStringLiteral("Before")), "Text before table missing");
    QVERIFY2(output.contains(QStringLiteral("After")), "Heading after table missing");
    QVERIFY2(output.contains(QStringLiteral("Paragraph")), "Paragraph after table missing");
}

void TestTableBugs::twoTablesRoundTrip()
{
    Editor editor;
    editor.resize(800, 600);

    QString md = QStringLiteral(
        "| A | B |\n"
        "|---|---|\n"
        "| 1 | 2 |\n"
        "\n"
        "Middle text\n"
        "\n"
        "| X | Y |\n"
        "|---|---|\n"
        "| 3 | 4 |");

    editor.setPlainText(md);
    editor.show();
    QTest::qWaitForWindowExposed(&editor);

    QString output = editor.toPlainText();
    qDebug() << "OUTPUT:" << output;

    QVERIFY2(output.contains(QStringLiteral("Middle")), "Middle text lost");
    // Both tables should produce pipe output
    int pipeCount = output.count(QLatin1Char('|'));
    QVERIFY2(pipeCount >= 12, qPrintable(
        QStringLiteral("Expected >= 12 pipes, got %1").arg(pipeCount)));
}

QTEST_MAIN(TestTableBugs)
#include "tst_table_bugs.moc"
