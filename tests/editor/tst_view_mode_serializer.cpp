// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster E Phase 1 — unit tests for Corbomite::ViewModeSerializer.
//
// Covers every combination of the `{mode: "source"|"preview", source?: bool}`
// compound encoding per `docs/obsidian-audit/domains/editor-markdown.md §8
// invariant 2`, plus the absent-`source`/unknown-`mode` edge cases.
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"
#include "ViewModeSerializer.h"

#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <QTest>

#include <optional>

using Corbomite::NoteEditorWidget;
using Corbomite::ViewModeCompound;
using Corbomite::ViewModeSerializer;

class ViewModeSerializerTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void toCompoundSource()
    {
        const auto c = ViewModeSerializer::toCompound(NoteEditorWidget::ViewMode::Source);
        QCOMPARE(c.mode, QStringLiteral("source"));
        QCOMPARE(c.source, true);
    }

    void toCompoundLivePreview()
    {
        const auto c = ViewModeSerializer::toCompound(NoteEditorWidget::ViewMode::LivePreview);
        QCOMPARE(c.mode, QStringLiteral("source"));
        QCOMPARE(c.source, false);
    }

    void toCompoundReading()
    {
        const auto c = ViewModeSerializer::toCompound(NoteEditorWidget::ViewMode::Reading);
        QCOMPARE(c.mode, QStringLiteral("preview"));
        // `source` field is don't-care on output; we emit `false` deterministically.
        QCOMPARE(c.source, false);
    }

    void fromCompoundSourceTrue()
    {
        QCOMPARE(ViewModeSerializer::fromCompound({QStringLiteral("source"), true}),
                 NoteEditorWidget::ViewMode::Source);
    }

    void fromCompoundSourceFalse()
    {
        QCOMPARE(ViewModeSerializer::fromCompound({QStringLiteral("source"), false}),
                 NoteEditorWidget::ViewMode::LivePreview);
    }

    void fromCompoundPreviewIgnoresSource()
    {
        QCOMPARE(ViewModeSerializer::fromCompound({QStringLiteral("preview"), false}),
                 NoteEditorWidget::ViewMode::Reading);
        QCOMPARE(ViewModeSerializer::fromCompound({QStringLiteral("preview"), true}),
                 NoteEditorWidget::ViewMode::Reading);
    }

    void fromCompoundAbsentSourceOnSourceDefaultsToLivePreview()
    {
        QCOMPARE(ViewModeSerializer::fromCompound(QStringLiteral("source"), std::nullopt),
                 NoteEditorWidget::ViewMode::LivePreview);
    }

    void fromCompoundAbsentSourceOnPreviewStaysReading()
    {
        QCOMPARE(ViewModeSerializer::fromCompound(QStringLiteral("preview"), std::nullopt),
                 NoteEditorWidget::ViewMode::Reading);
    }

    void fromCompoundUnknownModeDefaultsToLivePreview()
    {
        // Contract: unknown mode strings (future Obsidian versions?) fall
        // back to LivePreview + a qWarning on the "corbomite.viewmode"
        // logging category. We suppress the category to avoid noisy test
        // output.
        QLoggingCategory::setFilterRules(QStringLiteral("corbomite.viewmode.warning=false"));
        QCOMPARE(ViewModeSerializer::fromCompound(QStringLiteral("bogus"), std::nullopt),
                 NoteEditorWidget::ViewMode::LivePreview);
        QCOMPARE(ViewModeSerializer::fromCompound(QStringLiteral("bogus"), std::optional<bool>{true}),
                 NoteEditorWidget::ViewMode::LivePreview);
        QLoggingCategory::setFilterRules(QString{});
    }

    void roundTripAllThreeModes()
    {
        for (auto mode : {NoteEditorWidget::ViewMode::Source,
                          NoteEditorWidget::ViewMode::LivePreview,
                          NoteEditorWidget::ViewMode::Reading}) {
            const auto compound = ViewModeSerializer::toCompound(mode);
            QCOMPARE(ViewModeSerializer::fromCompound(compound), mode);
        }
    }
};

QTEST_APPLESS_MAIN(ViewModeSerializerTest)
#include "tst_view_mode_serializer.moc"
