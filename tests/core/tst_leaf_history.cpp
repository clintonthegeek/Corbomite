// tests/core/tst_leaf_history.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QJsonObject>
#include "corbomite/core/LeafHistory.h"

using Corbomite::LeafHistory;
using Corbomite::LeafHistoryEntry;

class TestLeafHistory : public QObject
{
    Q_OBJECT

private:
    LeafHistoryEntry makeEntry(const QString &title)
    {
        LeafHistoryEntry e;
        e.title = title;
        e.icon  = QStringLiteral("document");
        e.state = QJsonObject{{QStringLiteral("file"), title}};
        return e;
    }

private Q_SLOTS:
    void initiallyEmpty()
    {
        LeafHistory h;
        QVERIFY(!h.canGoBack());
        QVERIFY(!h.canGoForward());
    }

    void pushAndGoBack()
    {
        LeafHistory h;
        h.push(makeEntry(QStringLiteral("a")));
        h.push(makeEntry(QStringLiteral("b")));

        QVERIFY(h.canGoBack());
        QVERIFY(!h.canGoForward());

        auto entry = h.goBack(makeEntry(QStringLiteral("c")));
        QCOMPARE(entry.title, QStringLiteral("b"));
        QVERIFY(h.canGoBack());
        QVERIFY(h.canGoForward());
    }

    void goForwardAfterBack()
    {
        LeafHistory h;
        h.push(makeEntry(QStringLiteral("a")));
        h.push(makeEntry(QStringLiteral("b")));

        auto back = h.goBack(makeEntry(QStringLiteral("c")));
        auto fwd = h.goForward(back);
        QCOMPARE(fwd.title, QStringLiteral("c"));
    }

    void pushClearsForward()
    {
        LeafHistory h;
        h.push(makeEntry(QStringLiteral("a")));
        h.push(makeEntry(QStringLiteral("b")));

        h.goBack(makeEntry(QStringLiteral("c")));
        QVERIFY(h.canGoForward());

        h.push(makeEntry(QStringLiteral("d")));
        QVERIFY(!h.canGoForward());
    }

    void capAt20()
    {
        LeafHistory h;
        for (int i = 0; i < 25; ++i)
            h.push(makeEntry(QString::number(i)));

        int count = 0;
        auto current = makeEntry(QStringLiteral("final"));
        while (h.canGoBack()) {
            current = h.goBack(current);
            ++count;
        }
        QCOMPARE(count, 20);
    }

    void serializeRoundTrip()
    {
        LeafHistory h;
        h.push(makeEntry(QStringLiteral("a")));
        h.push(makeEntry(QStringLiteral("b")));

        QJsonObject json = h.serialize();
        LeafHistory h2 = LeafHistory::deserialize(json);

        QVERIFY(h2.canGoBack());
        auto entry = h2.goBack(makeEntry(QStringLiteral("c")));
        QCOMPARE(entry.title, QStringLiteral("b"));
    }

    void emptyBackReturnsInvalid()
    {
        LeafHistory h;
        auto entry = h.goBack(makeEntry(QStringLiteral("x")));
        QVERIFY(entry.title.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestLeafHistory)
#include "tst_leaf_history.moc"
