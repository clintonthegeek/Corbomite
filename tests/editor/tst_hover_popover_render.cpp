// SPDX-License-Identifier: GPL-3.0-or-later
// Hover preview re-light (2026-06-11) — HoverPopover renders a resolvable
// link target through StyledRenderEngine into its QTextBrowser. Resolution
// is supplied by a VaultResourceProvider; rendering by the headless engine.

#include <QApplication>
#include <QHash>
#include <QPoint>
#include <QString>
#include <QTest>

#include <optional>

#include "corbomite/core/StyledRenderEngine.h"
#include "corbomite/core/VaultResourceProvider.h"
#include "editor/HoverPopover.h"

using namespace Corbomite;

namespace {
class InMemoryResources : public Corbomite::Core::VaultResourceProvider
{
public:
    void addNote(const QString &name, const QString &content)
    {
        m_notes.insert(name, content);
    }
    QUrl resolveImage(const QString &name) const override
    {
        return QUrl(QStringLiteral("file:///fake/") + name);
    }
    QByteArray loadImageBytes(const QString &) const override { return {}; }
    std::optional<QString> resolveEmbed(const QString &name) const override
    {
        const auto it = m_notes.constFind(name);
        if (it == m_notes.constEnd()) return std::nullopt;
        return it.value();
    }
    QUrl resolveWikiLink(const QString &target) const override
    {
        return QUrl(QStringLiteral("vault:///") + target);
    }
    bool wikiLinkExists(const QString &target) const override
    {
        return m_notes.contains(target);
    }

private:
    QHash<QString, QString> m_notes;
};
} // namespace

class TstHoverPopoverRender : public QObject
{
    Q_OBJECT
private slots:
    void rendersResolvableTarget();
    void rendersPlaceholderForUnresolved();
    void emptyTargetDoesNotShow();
};

void TstHoverPopoverRender::rendersResolvableTarget()
{
    InMemoryResources resources;
    resources.addNote(QStringLiteral("Note.md"),
                      QStringLiteral("# Title\n\nSome body text.\n"));
    StyledRenderEngine engine;

    HoverPopover popover;
    popover.setRenderEngine(&engine);
    popover.setResources(&resources);

    popover.scheduleShow(QStringLiteral("Note.md"), QPoint(10, 10));
    QTRY_VERIFY_WITH_TIMEOUT(popover.isVisible(), 1000);

    const QString shown = popover.previewPlainText();
    QVERIFY2(shown.contains(QStringLiteral("Some body text")),
             qPrintable(shown));
}

void TstHoverPopoverRender::rendersPlaceholderForUnresolved()
{
    InMemoryResources resources; // empty — nothing resolves
    StyledRenderEngine engine;

    HoverPopover popover;
    popover.setRenderEngine(&engine);
    popover.setResources(&resources);

    popover.scheduleShow(QStringLiteral("Missing.md"), QPoint(10, 10));
    QTRY_VERIFY_WITH_TIMEOUT(popover.isVisible(), 1000);

    const QString shown = popover.previewPlainText();
    QVERIFY2(shown.contains(QStringLiteral("unresolved")), qPrintable(shown));
}

void TstHoverPopoverRender::emptyTargetDoesNotShow()
{
    StyledRenderEngine engine;
    HoverPopover popover;
    popover.setRenderEngine(&engine);
    popover.scheduleShow(QString(), QPoint(0, 0));
    QTest::qWait(350);
    QVERIFY(!popover.isVisible());
}

QTEST_MAIN(TstHoverPopoverRender)
#include "tst_hover_popover_render.moc"
