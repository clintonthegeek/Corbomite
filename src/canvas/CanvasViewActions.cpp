// src/canvas/CanvasViewActions.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CanvasViewActions.h"

#include "CanvasFileView.h"
#include "CanvasViewTab.h"
#include "corbomite/core/View.h"
#include "corbomitesettings.h"

#include <canvas/CanvasAlignmentStrategy.h>
#include <canvas/CanvasScene.h>
#include <canvas/CanvasView.h>

#include <KActionCollection>
#include <KLocalizedString>

#include <QAction>
#include <QIcon>
#include <QKeySequence>

namespace Corbomite {

namespace {
const QString kCanvasGuiXml = QStringLiteral(
    "<!DOCTYPE gui SYSTEM \"kpartgui.dtd\">"
    "<gui name=\"canvas_view_actions\" version=\"1\">"
    "  <MenuBar>"
    "    <Menu name=\"canvas\">"
    "      <text>&amp;Canvas</text>"
    "      <Action name=\"canvas_snap_grid\"/>"
    "      <Action name=\"canvas_snap_objects\"/>"
    "      <Action name=\"canvas_show_grid\"/>"
    "      <Separator/>"
    "      <Action name=\"canvas_zoom_in\"/>"
    "      <Action name=\"canvas_zoom_out\"/>"
    "      <Action name=\"canvas_zoom_reset\"/>"
    "      <Action name=\"canvas_zoom_to_fit\"/>"
    "      <Action name=\"canvas_zoom_to_selection\"/>"
    "      <Separator/>"
    "      <Action name=\"canvas_lock\"/>"
    "      <Action name=\"canvas_jump_to_group\"/>"
    "      <Action name=\"canvas_convert_to_file\"/>"
    "    </Menu>"
    "  </MenuBar>"
    "</gui>");
} // namespace

CanvasViewActions::CanvasViewActions(QObject *parent)
    : ViewActions(parent)
{
    setComponentName(QStringLiteral("corbomite_canvas_actions"), i18n("Canvas"));
    setupActions();
    setXML(kCanvasGuiXml, false /*merge*/);

    actionCollection()->setConfigGroup(QStringLiteral("Shortcuts"));

    // Nothing bound yet — disable everything (Tier B "unbound" default).
    refresh();
}

QString CanvasViewActions::viewType() const
{
    return QStringLiteral("canvas");
}

void CanvasViewActions::bind(View *view)
{
    disconnect(m_contextConnection);

    m_boundView = qobject_cast<CanvasFileView *>(view);

    if (m_boundView) {
        // The generic Tier-B refresh trigger: CanvasFileView::onLoadFile
        // already forwards the scene's selectionChanged/undoStack signals
        // onto View::contextChanged() — one connection covers all of them,
        // same mechanism ActionContextController itself uses for its own
        // refresh().
        m_contextConnection =
            connect(m_boundView, &View::contextChanged, this, &CanvasViewActions::refresh);

        // CanvasAlignmentStrategy hardcodes its own true/true
        // defaults at construction (it has no kcfg dependency of its own),
        // so a freshly-created canvas tab won't reflect a previously
        // persisted (non-default) setting until something re-runs the
        // fan-out. MainWindow::applyCanvasSettings() covers every ALREADY
        // OPEN leaf on a settings change; this covers the newly-bound one
        // immediately on first focus, closing the gap for a canvas tab
        // that opens directly into an already-non-default setting.
        auto *settings = CorbomiteSettings::self();
        if (auto *tab = m_boundView->canvasWidget()) {
            if (auto *scene = tab->canvasScene())
                if (auto *align = scene->alignmentStrategy()) {
                    align->setSnapToGridEnabled(settings->snapToGrid());
                    align->setSnapToObjectsEnabled(settings->snapToObjects());
                }
            if (auto *cview = tab->canvasView())
                cview->setGridVisible(settings->showGrid());
        }
    }

    refresh();
}

void CanvasViewActions::unbind()
{
    disconnect(m_contextConnection);
    m_boundView = nullptr;
    refresh();
}

Canvas::CanvasScene *CanvasViewActions::canvasScene() const
{
    auto *tab = canvas() ? canvas()->canvasWidget() : nullptr;
    return tab ? tab->canvasScene() : nullptr;
}

bool CanvasViewActions::canvasHasGroups() const
{
    auto *scene = canvasScene();
    return scene && !scene->groupJumpTargets().isEmpty();
}

QString CanvasViewActions::selectedTextNodeId() const
{
    auto *scene = canvasScene();
    return scene ? scene->selectedTextNodeId() : QString();
}

void CanvasViewActions::refresh()
{
    auto *ac = actionCollection();
    const bool isCanvas = m_boundView != nullptr;

    static const QStringList alwaysUsable = {
        QStringLiteral("canvas_snap_grid"), QStringLiteral("canvas_snap_objects"),
        QStringLiteral("canvas_show_grid"),
        QStringLiteral("canvas_zoom_in"), QStringLiteral("canvas_zoom_out"),
        QStringLiteral("canvas_zoom_reset"), QStringLiteral("canvas_zoom_to_fit"),
    };
    for (const auto &id : alwaysUsable)
        if (auto *a = ac->action(id)) a->setEnabled(isCanvas);

    // Tier B: zoom-to-selection needs something selected.
    if (auto *a = ac->action(QStringLiteral("canvas_zoom_to_selection")))
        a->setEnabled(isCanvas && m_boundView->hasSelection());

    // The read-only lock is always usable; the two commands below are
    // Tier-B gated.
    if (auto *a = ac->action(QStringLiteral("canvas_lock"))) {
        a->setEnabled(isCanvas);
        a->setChecked(m_boundView && m_boundView->isReadOnly());
    }

    // Tier B, Obsidian's own checkCallback conditions
    // (app/own/2028-n9.js:671/682-692): jump-to-group needs a canvas with at
    // least one group; convert-to-file needs exactly one selected text node.
    if (auto *a = ac->action(QStringLiteral("canvas_jump_to_group")))
        a->setEnabled(isCanvas && canvasHasGroups());
    if (auto *a = ac->action(QStringLiteral("canvas_convert_to_file")))
        a->setEnabled(isCanvas && !m_boundView->isReadOnly()
                      && selectedTextNodeId().size() > 0);

    // Tier C — the three toggles are app-wide (kcfg), not per-document, so
    // their checked state reflects the persisted setting regardless of
    // bind state. MainWindow's
    // applyCanvasSettings() is the actual settings-application fan-out;
    // this is display-only sync.
    auto *settings = CorbomiteSettings::self();
    if (auto *a = ac->action(QStringLiteral("canvas_snap_grid")))
        a->setChecked(settings->snapToGrid());
    if (auto *a = ac->action(QStringLiteral("canvas_snap_objects")))
        a->setChecked(settings->snapToObjects());
    if (auto *a = ac->action(QStringLiteral("canvas_show_grid")))
        a->setChecked(settings->showGrid());
}

QList<QAction *> CanvasViewActions::toolBarActions() const
{
    auto *ac = actionCollection();
    QList<QAction *> actions;
    for (const auto &id : {
             QStringLiteral("canvas_snap_grid"),
             QStringLiteral("canvas_snap_objects"),
             QStringLiteral("canvas_show_grid"),
             QStringLiteral("canvas_zoom_in"),
             QStringLiteral("canvas_zoom_out"),
             QStringLiteral("canvas_zoom_to_fit"),
         }) {
        if (auto *a = ac->action(id)) actions << a;
    }
    return actions;
}

// ---------------------------------------------------------------------
// setupActions()
// ---------------------------------------------------------------------

void CanvasViewActions::setupActions()
{
    auto *ac = actionCollection();

    // Snap-to-grid / snap-to-objects / show-grid: app-wide toggles backed
    // by corbomite.kcfg's Canvas group. The trigger handler writes
    // through kcfg and saves; MainWindow::applyCanvasSettings(), wired to
    // CorbomiteSettings::configChanged, is what actually fans the new
    // value out to every open CanvasAlignmentStrategy/CanvasView — this
    // provider only owns the QAction and the persisted value ("one
    // definition, N surfaces").
    auto addToggle = [this, ac](
        const QString &id, const QString &icon, const QString &label,
        void (*setter)(bool)) -> QAction* {
        auto *act = ac->addAction(id);
        act->setText(label);
        act->setIcon(QIcon::fromTheme(icon));
        act->setCheckable(true);
        connect(act, &QAction::triggered, this, [setter](bool checked) {
            setter(checked);
            CorbomiteSettings::self()->save();
        });
        return act;
    };
    addToggle(QStringLiteral("canvas_snap_grid"),
              QStringLiteral("snap-to-grid"), i18n("Snap to Grid"),
              &CorbomiteSettings::setSnapToGrid);
    addToggle(QStringLiteral("canvas_snap_objects"),
              QStringLiteral("snap-bounding-box"), i18n("Snap to Objects"),
              &CorbomiteSettings::setSnapToObjects);
    addToggle(QStringLiteral("canvas_show_grid"),
              QStringLiteral("view-grid"), i18n("Show Grid"),
              &CorbomiteSettings::setShowGrid);

    // Zoom: reuses the View::zoomIn/zoomOut/zoomReset virtuals
    // (already implemented on CanvasFileView) so the Canvas menu/toolbar
    // and the universal View menu's zoom entries stay in lock-step — same
    // dispatch, just a second surface. No default shortcut, mirroring
    // view_zoom_in/out/reset's own "Ctrl+= is ambiguous" restraint
    // (MainWindow::setupActions).
    auto addZoomVerb = [this, ac](
        const QString &id, const QString &icon, const QString &label,
        void (View::*verb)()) -> QAction* {
        auto *act = ac->addAction(id);
        act->setText(label);
        act->setIcon(QIcon::fromTheme(icon));
        connect(act, &QAction::triggered, this, [this, verb]() {
            if (m_boundView) (m_boundView->*verb)();
        });
        return act;
    };
    addZoomVerb(QStringLiteral("canvas_zoom_in"), QStringLiteral("zoom-in"),
                i18n("Zoom In"), &View::zoomIn);
    addZoomVerb(QStringLiteral("canvas_zoom_out"), QStringLiteral("zoom-out"),
                i18n("Zoom Out"), &View::zoomOut);
    addZoomVerb(QStringLiteral("canvas_zoom_reset"), QStringLiteral("zoom-original"),
                i18n("Reset Zoom"), &View::zoomReset);

    auto *zoomToFit = ac->addAction(QStringLiteral("canvas_zoom_to_fit"));
    zoomToFit->setText(i18n("Zoom to Fit"));
    zoomToFit->setIcon(QIcon::fromTheme(QStringLiteral("zoom-fit-best")));
    KActionCollection::setDefaultShortcut(zoomToFit, QKeySequence(Qt::SHIFT | Qt::Key_1));
    connect(zoomToFit, &QAction::triggered, this, [this]() {
        if (auto *tab = canvas() ? canvas()->canvasWidget() : nullptr)
            if (auto *view = tab->canvasView())
                view->zoomToFit();
    });

    auto *zoomToSelection = ac->addAction(QStringLiteral("canvas_zoom_to_selection"));
    zoomToSelection->setText(i18n("Zoom to Selection"));
    zoomToSelection->setIcon(QIcon::fromTheme(QStringLiteral("zoom-fit-selection")));
    KActionCollection::setDefaultShortcut(zoomToSelection, QKeySequence(Qt::SHIFT | Qt::Key_2));
    connect(zoomToSelection, &QAction::triggered, this, [this]() {
        if (auto *tab = canvas() ? canvas()->canvasWidget() : nullptr)
            if (auto *view = tab->canvasView())
                view->zoomToSelection();
    });

    // The read-only lock. Checkable rather than a one-shot verb, so
    // the menu entry and a toolbar button are the same widget (Obsidian's
    // quick-settings entry is `setChecked(canvas.readonly)`,
    // app/own/2018-k7.js:1417). The scene emits readOnlyChanged, which
    // CanvasFileView already funnels into View::contextChanged, so the
    // checked state tracks a lock set from anywhere, not just this action.
    auto *lock = ac->addAction(QStringLiteral("canvas_lock"));
    lock->setText(i18n("Lock Canvas (Read-Only)"));
    lock->setIcon(QIcon::fromTheme(QStringLiteral("object-locked")));
    lock->setCheckable(true);
    connect(lock, &QAction::triggered, this, [this](bool checked) {
        if (!m_boundView) return;
        if (auto *tab = m_boundView->canvasWidget())
            if (auto *scene = tab->canvasScene())
                scene->setReadOnly(checked);
        refresh();
    });

    // The two commands registered as disabled placeholders.
    // Both dispatch straight to CanvasViewTab, which owns the prompt and the
    // FileManager write (libs/canvas may not reach either).
    auto *jump = ac->addAction(QStringLiteral("canvas_jump_to_group"));
    jump->setText(i18n("Jump to Group..."));
    jump->setIcon(QIcon::fromTheme(QStringLiteral("go-jump")));
    connect(jump, &QAction::triggered, this, [this]() {
        if (auto *tab = canvas() ? canvas()->canvasWidget() : nullptr)
            tab->jumpToGroup();
    });

    auto *convert = ac->addAction(QStringLiteral("canvas_convert_to_file"));
    convert->setText(i18n("Convert to File..."));
    convert->setIcon(QIcon::fromTheme(QStringLiteral("document-export")));
    connect(convert, &QAction::triggered, this, [this]() {
        const QString nodeId = selectedTextNodeId();
        if (nodeId.isEmpty())
            return;
        if (auto *tab = canvas() ? canvas()->canvasWidget() : nullptr)
            tab->convertTextCardToFile(nodeId);
    });
}

} // namespace Corbomite
