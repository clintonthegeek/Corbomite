// SPDX-License-Identifier: GPL-3.0-or-later
// Hover preview re-light (2026-06-11) — NoteEditorWidget forwards the shared
// LinkService hover signals to the HoverPopover. Both Live and Reading leaves
// share m_linkService, so driving notifyHover() here proves the wiring.

#include "NoteEditorWidget.h"
#include "HoverPopover.h"

#include <markoff/core/DefaultLinkService.h>
#include <markoff/core/LinkActivation.h>
#include <markoff/core/LinkKind.h>
#include <markoff/live/EditorWidget.h>
#include <markoff/live/LiveListModelBinding.h>

#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/StyledRenderEngine.h"
#include "corbomite/core/VaultResourceProvider.h"

#include <QHash>
#include <QPoint>
#include <QTest>

#include <optional>

using Corbomite::HoverPopover;
using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;
using Corbomite::StyledRenderEngine;

namespace {
// Minimal test double. NOTE: unlike the production VaultScopedResources,
// resolveEmbed() here matches the bare key verbatim — it intentionally omits
// the ".md" normalization production applies. Tests therefore add notes under
// the exact key they hover; don't copy this into a real provider.
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

class HoverTriggerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void linkHover_forwardsToPopover_andRenders()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[[Target]]"));
        widget.setNoteDocument(&doc);

        InMemoryResources resources;
        resources.addNote(QStringLiteral("Target"),
                          QStringLiteral("# T\n\nhovered body.\n"));
        StyledRenderEngine engine;

        HoverPopover popover;
        popover.setRenderEngine(&engine);
        popover.setResources(&resources);
        widget.setHoverPopover(&popover);

        auto *svc = widget.editor()->binding()->linkService();
        QVERIFY(svc);

        Markoff::LinkActivation act;
        act.kind    = Markoff::LinkKind::WikiLink;
        act.page    = QStringLiteral("Target");
        act.rawText = QStringLiteral("[[Target]]");

        svc->notifyHover(act, QPoint(50, 50));
        // scheduleShow() entered the Pending state synchronously.
        QCOMPARE(popover.stateForTest(), HoverPopover::State::Pending);

        // After the 300ms delay the popover shows the rendered target.
        QTRY_VERIFY_WITH_TIMEOUT(popover.isVisible(), 1000);
        QVERIFY2(popover.previewPlainText().contains(QStringLiteral("hovered body")),
                 qPrintable(popover.previewPlainText()));
    }

    void linkHoverLeft_whilePending_cancels()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[[Target]]"));
        widget.setNoteDocument(&doc);

        HoverPopover popover;
        widget.setHoverPopover(&popover);

        auto *svc = widget.editor()->binding()->linkService();
        Markoff::LinkActivation act;
        act.kind    = Markoff::LinkKind::WikiLink;
        act.page    = QStringLiteral("Target");
        act.rawText = QStringLiteral("[[Target]]");

        svc->notifyHover(act, QPoint(50, 50));
        QCOMPARE(popover.stateForTest(), HoverPopover::State::Pending);

        svc->notifyHoverLeft(QStringLiteral("[[Target]]"));
        QCOMPARE(popover.stateForTest(), HoverPopover::State::Hidden);
    }
};

QTEST_MAIN(HoverTriggerTest)
#include "tst_note_editor_widget_hover.moc"
