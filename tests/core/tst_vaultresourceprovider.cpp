// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::Core::VaultResourceProvider — promoted from
// libs/readingview/ in Cluster J Phase 1 Task 1.1.
//
// Contract mirrors the previous ReadingView-local interface exactly
// (see libs/readingview/include/markoff/reading/VaultResourceProvider.h
// which is now a forwarding typedef).

#include <QTest>

#include "corbomite/core/VaultResourceProvider.h"

class StubResourceProvider : public Corbomite::Core::VaultResourceProvider
{
public:
    QUrl resolveImage(const QString &name) const override
    {
        return QUrl(QStringLiteral("file:///vault/") + name);
    }
    QByteArray loadImageBytes(const QString &name) const override
    {
        return QByteArray("bytes:") + name.toUtf8();
    }
    std::optional<QString> resolveEmbed(const QString &name) const override
    {
        if (name == QStringLiteral("Missing")) {
            return std::nullopt;
        }
        return QStringLiteral("# body of ") + name;
    }
    QUrl resolveWikiLink(const QString &target) const override
    {
        return QUrl(QStringLiteral("vault://") + target);
    }
    bool wikiLinkExists(const QString &target) const override
    {
        return target != QStringLiteral("NoSuch");
    }
};

class TstVaultResourceProvider : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testInterfaceDispatch()
    {
        StubResourceProvider p;
        QCOMPARE(p.resolveImage(QStringLiteral("img.png")),
                 QUrl(QStringLiteral("file:///vault/img.png")));
        QCOMPARE(p.loadImageBytes(QStringLiteral("img.png")), QByteArray("bytes:img.png"));
        QCOMPARE(p.resolveEmbed(QStringLiteral("Foo")).value_or(QString()),
                 QStringLiteral("# body of Foo"));
        QVERIFY(!p.resolveEmbed(QStringLiteral("Missing")).has_value());
        QCOMPARE(p.resolveWikiLink(QStringLiteral("Bar")),
                 QUrl(QStringLiteral("vault://Bar")));
        QVERIFY(p.wikiLinkExists(QStringLiteral("Bar")));
        QVERIFY(!p.wikiLinkExists(QStringLiteral("NoSuch")));
    }

    void testPolymorphism()
    {
        StubResourceProvider p;
        Corbomite::Core::VaultResourceProvider *ptr = &p;
        QCOMPARE(ptr->resolveImage(QStringLiteral("x")),
                 QUrl(QStringLiteral("file:///vault/x")));
    }
};

QTEST_APPLESS_MAIN(TstVaultResourceProvider)
#include "tst_vaultresourceprovider.moc"
