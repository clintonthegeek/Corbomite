// SPDX-License-Identifier: GPL-3.0-or-later
#include "ViewModeSerializer.h"

#include <QLoggingCategory>

namespace Corbomite {

namespace {
Q_LOGGING_CATEGORY(lcViewMode, "corbomite.viewmode")

const QString &modeSource()
{
    static const QString s = QStringLiteral("source");
    return s;
}
const QString &modePreview()
{
    static const QString s = QStringLiteral("preview");
    return s;
}
} // namespace

ViewModeCompound ViewModeSerializer::toCompound(NoteEditorWidget::ViewMode m)
{
    switch (m) {
    case NoteEditorWidget::ViewMode::Source:
        return {modeSource(), true};
    case NoteEditorWidget::ViewMode::LivePreview:
        return {modeSource(), false};
    case NoteEditorWidget::ViewMode::Reading:
        // Reading carries `source=false` on output by contract (value is
        // don't-care; determinism helps golden-fixture tests).
        return {modePreview(), false};
    }
    return {modeSource(), false};
}

NoteEditorWidget::ViewMode ViewModeSerializer::fromCompound(const ViewModeCompound &c)
{
    return fromCompound(c.mode, std::optional<bool>{c.source});
}

NoteEditorWidget::ViewMode ViewModeSerializer::fromCompound(
    const QString &mode, const std::optional<bool> &source)
{
    if (mode == modePreview()) {
        return NoteEditorWidget::ViewMode::Reading;
    }
    if (mode == modeSource()) {
        // Absent source defaults to LivePreview (source=false).
        const bool src = source.value_or(false);
        return src ? NoteEditorWidget::ViewMode::Source
                   : NoteEditorWidget::ViewMode::LivePreview;
    }
    qCWarning(lcViewMode) << "Unknown mode string:" << mode
                          << "- defaulting to LivePreview";
    return NoteEditorWidget::ViewMode::LivePreview;
}

} // namespace Corbomite
