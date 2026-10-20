// src/canvas/CanvasFileView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CanvasFileView.h"
#include "CanvasViewTab.h"
#include "corbomite/core/MenuSectionHelper.h"
#include "corbomite/core/NoteDocument.h"

#include "SessionManager.h"
#include "app/MessageService.h"

#include <canvas/CanvasDocument.h>
#include <canvas/CanvasDocumentRegistry.h>
#include <canvas/CanvasScene.h>
#include <corbomite/core/ThemeService.h>
#include <canvas/CanvasView.h>

#include <KLocalizedString>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGroupBox>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QUndoStack>
#include <QVBoxLayout>

namespace Corbomite {

namespace {

/// The read-only lock's tier-3 home is SessionManager (the app's only
/// machine-local store), and
/// MainWindow owns the one live instance. MainWindow propagates the store
/// explicitly (propagateServicesToView); this walk is the fallback for the
/// cases where it has not been propagated yet (tests, headless), where an
/// in-memory-only lock is correct anyway. Returns null outside a MainWindow.
SessionManager *tier3StoreFor(QWidget *leaf)
{
    auto *window = leaf ? leaf->window() : nullptr;
    return window ? window->findChild<SessionManager *>() : nullptr;
}

} // namespace

CanvasFileView::CanvasFileView(WorkspaceLeaf *leaf, QWidget *parent)
    : FileView(leaf, parent)
{
}

View *CanvasFileView::factory(WorkspaceLeaf *leaf)
{
    return new CanvasFileView(leaf);
}

CanvasFileView::~CanvasFileView()
{
    // Teardown trap: the scene (and its QUndoStack) are QObject
    // children reachable through the QWidget tree, so they're destroyed
    // inside QWidget::~QWidget()'s child teardown — which runs AFTER
    // View::~View() has already completed. QUndoStack::~QUndoStack()
    // calls clear(), which synchronously emits canUndoChanged/
    // canRedoChanged; those were forwarded straight to
    // View::contextChanged() (a signal declared on an intermediate base,
    // not CanvasFileView itself), and invoking a connection into a base
    // class whose destructor has already run crashes ("class destructor
    // may have already run" — the object's vtable/metaobject no longer
    // resolves to Corbomite::View at that point). Disconnecting here,
    // before any base-class destructor has run and while `this` is still
    // fully valid, severs the connection early enough to avoid it.
    if (m_canvasWidget && !m_registeredCanvasPath.isEmpty()) {
        Canvas::CanvasDocumentRegistry::instance().detach(
            m_registeredCanvasPath, m_canvasWidget->canvasDocument());
        m_registeredCanvasPath.clear();
    }
    if (m_canvasWidget) {
        if (auto *scene = m_canvasWidget->canvasScene()) {
            disconnect(scene, nullptr, this, nullptr);
            if (auto *stack = scene->undoStack())
                disconnect(stack, nullptr, this, nullptr);
        }
    }
}

QString CanvasFileView::getViewType() const { return QStringLiteral("canvas"); }
QString CanvasFileView::getIcon() const { return QStringLiteral("palette"); }

bool CanvasFileView::canAcceptExtension(const QString &ext) const
{
    return ext.compare(QStringLiteral("canvas"), Qt::CaseInsensitive) == 0;
}

QJsonObject CanvasFileView::getState() const
{
    QJsonObject state = FileView::getState();
    if (m_canvasWidget) {
        if (auto *view = m_canvasWidget->canvasView()) {
            const QJsonObject viewState = view->viewState();
            if (!viewState.isEmpty())
                state[QStringLiteral("viewState")] = viewState;
        }
    }
    return state;
}

void CanvasFileView::setState(const QJsonObject &state)
{
    const QJsonObject viewState = state.value(QStringLiteral("viewState")).toObject();

    // FileView::setState resolves and loads the file, which is what creates
    // the CanvasViewTab on first open. Order matters and mirrors Obsidian's
    // CanvasView.setState (app/own/2028-n9.js:842-844): super.setState()
    // first, viewState second — there is no scene to centre on before the
    // document is loaded, and the scene's own populate step would otherwise
    // fight the incoming transform.
    FileView::setState(state);

    if (viewState.isEmpty())
        return;
    // A failed load leaves no canvas tab; applying a viewport then would
    // centre on a scene with no document, and FileView::setState has already
    // scheduled the leaf close in that case.
    if (m_canvasWidget) {
        if (auto *view = m_canvasWidget->canvasView())
            view->setViewState(viewState);
    }
}

void CanvasFileView::onExternalModify(const QString &relativePath)
{
    if (!m_canvasWidget || !file())
        return;
    if (file()->relativePath() != relativePath)
        return;

    // Our own autosave writes through QFile (not Vault::modify), so its echo
    // arrives here too — classify() drops it because the bytes match.
    const QByteArray disk = m_canvasWidget->diskBytes();
    const QByteArray ours = m_canvasWidget->serializedBytes();
    const auto action = CanvasViewTab::classifyExternalEdit(
        ours, disk, m_canvasWidget->isModified(), m_lastExternalDisk);
    if (action == CanvasViewTab::ExternalEdit::NoChange)
        return;

    m_lastExternalDisk = disk;

    if (action == CanvasViewTab::ExternalEdit::ReloadFromDisk) {
        if (m_canvasWidget->reloadFromDisk()) {
            MessageService::postFrom(this,
                i18n("\"%1\" changed on disk — canvas reloaded.", relativePath),
                MessageService::Severity::Info);
        }
        return;
    }

    // KeepLocalAndWarn: the user has edits in flight and the file moved
    // underneath them. Say so, and say what happens next, because the pending
    // autosave will replace the file with this document.
    MessageService::postFrom(this,
        i18n("\"%1\" changed on disk while you were editing. Keeping your "
             "version — saving will replace the file.",
             relativePath),
        MessageService::Severity::Warning);
}

void CanvasFileView::setRenderEngine(MarkdownRenderEngine *engine)
{
    m_renderEngine = engine;
    if (m_canvasWidget)
        m_canvasWidget->setRenderEngine(engine);
}

CanvasViewTab *CanvasFileView::canvasWidget() const { return m_canvasWidget; }

void CanvasFileView::refreshFileCards(const QString &vaultRelativePath)
{
    if (m_canvasWidget)
        m_canvasWidget->refreshFileCards(vaultRelativePath);
}

void CanvasFileView::setFileLister(std::function<QStringList()> lister)
{
    m_fileLister = std::move(lister);
    // The tab may already exist (late service propagation), so push now as
    // well as at construction time.
    if (m_canvasWidget && m_fileLister)
        m_canvasWidget->setFileLister(m_fileLister);
}

void CanvasFileView::setLocalDataStore(LocalDataReader reader, LocalDataWriter writer)
{
    m_localDataReader = std::move(reader);
    m_localDataWriter = std::move(writer);
    if (m_canvasWidget)
        m_canvasWidget->setLocalDataStore(m_localDataReader, m_localDataWriter);
}

bool CanvasFileView::isReadOnly() const
{
    auto *scene = m_canvasWidget ? m_canvasWidget->canvasScene() : nullptr;
    return scene && scene->isReadOnly();
}

void CanvasFileView::setThemeService(Core::ThemeService *service)
{
    if (m_themeService == service) return;
    if (m_themeService)
        disconnect(m_themeService, nullptr, this, nullptr);
    m_themeService = service;
    if (!m_themeService) return;

    auto apply = [this]() {
        if (auto *tab = canvasWidget())
            if (auto *scene = tab->canvasScene())
                scene->applyDarkTheme(m_themeService->isDark());
    };
    connect(m_themeService, &Core::ThemeService::themeChanged, this,
            [apply](const Markoff::Theme &) { apply(); });
    apply();   // covers a canvas tab that is already open
}

void CanvasFileView::zoomIn()
{
    if (m_canvasWidget)
        if (auto *view = m_canvasWidget->canvasView())
            view->zoomIn();
}

void CanvasFileView::zoomOut()
{
    if (m_canvasWidget)
        if (auto *view = m_canvasWidget->canvasView())
            view->zoomOut();
}

void CanvasFileView::zoomReset()
{
    if (m_canvasWidget)
        if (auto *view = m_canvasWidget->canvasView())
            view->resetTransform();
}

void CanvasFileView::onLoadFile(NoteDocument *file)
{
    FileView::onLoadFile(file);
    if (!m_canvasWidget && file) {
        m_canvasWidget = new CanvasViewTab(file->filePath(), file->vaultRoot(), contentWidget());
        if (m_fileLister)
            m_canvasWidget->setFileLister(m_fileLister);
        auto *layout = new QVBoxLayout(contentWidget());
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_canvasWidget);
        if (m_renderEngine)
            m_canvasWidget->setRenderEngine(m_renderEngine);

        // Read-only lock persistence. Done after addWidget() because
        // tier3StoreFor() resolves through window(), which only has a chain
        // to this tab once it is in the layout.
        if (m_localDataReader || m_localDataWriter) {
            m_canvasWidget->setLocalDataStore(m_localDataReader, m_localDataWriter);
        } else if (auto *tier3 = tier3StoreFor(this)) {
            m_canvasWidget->setLocalDataStore(
                [tier3](const QString &key) { return tier3->fileLocalData(key); },
                [tier3](const QString &key, const QJsonObject &value) {
                    tier3->setFileLocalData(key, value);
                });
        }

        // Forward the scene's own selection/undo
        // signals onto the generic View::contextChanged() so
        // ActionContextController's refresh() picks up mid-session
        // selection changes and undo-stack pushes, not just leaf/view
        // switches (the edit_undo/edit_redo enablement previously
        // only recomputed on those coarser triggers).
        if (auto *scene = m_canvasWidget->canvasScene()) {
            connect(scene, &QGraphicsScene::selectionChanged,
                    this, &View::contextChanged);
            if (auto *stack = scene->undoStack()) {
                connect(stack, &QUndoStack::canUndoChanged,
                        this, &View::contextChanged);
                connect(stack, &QUndoStack::canRedoChanged,
                        this, &View::contextChanged);
            }
        }
    }

    // Publish the live document so a vault rename mutates it instead
    // of writing a second copy to disk (single writer). Keyed on the
    // vault-relative canvas path, the same key FileManager uses.
    if (m_canvasWidget && file) {
        m_registeredCanvasPath = m_canvasWidget->canvasRelativePath();
        Canvas::CanvasDocumentRegistry::instance().attach(
            m_registeredCanvasPath, m_canvasWidget->canvasDocument());
    }
}

bool CanvasFileView::canEdit() const { return !isReadOnly(); }
bool CanvasFileView::canSave() const { return true; }

bool CanvasFileView::hasSelection() const
{
    auto *scene = m_canvasWidget ? m_canvasWidget->canvasScene() : nullptr;
    return scene && !scene->selectedItems().isEmpty();
}

bool CanvasFileView::canUndo() const
{
    auto *scene = m_canvasWidget ? m_canvasWidget->canvasScene() : nullptr;
    return scene && scene->canUndo();
}

bool CanvasFileView::canRedo() const
{
    auto *scene = m_canvasWidget ? m_canvasWidget->canvasScene() : nullptr;
    return scene && scene->canRedo();
}

void CanvasFileView::onUnloadFile(NoteDocument *file)
{
    if (m_canvasWidget && m_canvasWidget->isModified())
        m_canvasWidget->save();
    if (m_canvasWidget && !m_registeredCanvasPath.isEmpty()) {
        Canvas::CanvasDocumentRegistry::instance().detach(
            m_registeredCanvasPath, m_canvasWidget->canvasDocument());
        m_registeredCanvasPath.clear();
    }
    FileView::onUnloadFile(file);
}

void CanvasFileView::setCanvasCommandDispatcher(CommandDispatch dispatcher)
{
    m_canvasCommandDispatcher = std::move(dispatcher);
}

void CanvasFileView::showExportAsImageModal()
{
    if (!m_canvasWidget) return;
    auto *scene = m_canvasWidget->canvasScene();
    if (!scene) return;

    QDialog dlg(this);
    dlg.setWindowTitle(i18n("Export canvas as image"));

    auto *lay = new QVBoxLayout(&dlg);

    // --- Area radio group ---
    auto *areaGroup = new QGroupBox(i18n("Area"), &dlg);
    auto *areaLay = new QVBoxLayout(areaGroup);
    auto *selectedRadio = new QRadioButton(i18n("Only selected nodes"), areaGroup);
    auto *fullRadio = new QRadioButton(i18n("Full canvas"), areaGroup);
    const bool hasSelection = !scene->selectedItems().isEmpty();
    selectedRadio->setEnabled(hasSelection);
    (hasSelection ? selectedRadio : fullRadio)->setChecked(true);
    areaLay->addWidget(selectedRadio);
    areaLay->addWidget(fullRadio);
    lay->addWidget(areaGroup);

    // --- Format combo ---
    auto *formatCombo = new QComboBox(&dlg);
    formatCombo->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    formatCombo->addItem(QStringLiteral("SVG"), QStringLiteral("svg"));
    lay->addWidget(new QLabel(i18n("Format"), &dlg));
    lay->addWidget(formatCombo);

    // --- Checkbox toggles ---
    auto *transparentBg = new QCheckBox(i18n("Transparent background"), &dlg);
    auto *showEdges = new QCheckBox(i18n("Show edges / connections"), &dlg);
    showEdges->setChecked(true);
    lay->addWidget(transparentBg);
    lay->addWidget(showEdges);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dlg);
    bb->button(QDialogButtonBox::Ok)->setText(i18n("Export"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    // --- Compute bounds ---
    QRectF bounds;
    if (selectedRadio->isChecked() && hasSelection) {
        for (auto *item : scene->selectedItems()) {
            bounds = bounds.united(item->sceneBoundingRect());
        }
    } else {
        bounds = scene->itemsBoundingRect();
    }
    if (bounds.isEmpty()) return;

    // --- Save-as path ---
    const QString format = formatCombo->currentData().toString();
    const QString defaultName =
        QStringLiteral("canvas-export.") + format;
    const QString out = QFileDialog::getSaveFileName(
        this, i18n("Export canvas"), defaultName,
        format == QStringLiteral("png")
            ? i18n("PNG files (*.png)")
            : i18n("SVG files (*.svg)"));
    if (out.isEmpty()) return;

    // --- Write ---
    if (format == QStringLiteral("png")) {
        QImage img = scene->renderToImage(bounds, transparentBg->isChecked(),
                                            showEdges->isChecked(), 2.0);
        img.save(out, "PNG");
    } else {
        QFile f(out);
        if (f.open(QIODevice::WriteOnly)) {
            scene->renderToSvg(bounds, &f,
                               transparentBg->isChecked(),
                               showEdges->isChecked());
        }
    }
}

void CanvasFileView::onMoreOptionsMenu(MenuSectionHelper &helper)
{
    auto dispatch = [this](const QString &cmd) {
        if (m_canvasCommandDispatcher) m_canvasCommandDispatcher(cmd);
    };

    // ---- pane: Split right / Split down ----
    auto *splitR = new QAction(
        QIcon::fromTheme(QStringLiteral("view-split-left-right")),
        i18n("Split right"), this);
    connect(splitR, &QAction::triggered, this,
            [dispatch] { dispatch(QStringLiteral("split_right")); });
    helper.addToSection(splitR, QStringLiteral("pane"));

    auto *splitD = new QAction(
        QIcon::fromTheme(QStringLiteral("view-split-top-bottom")),
        i18n("Split down"), this);
    connect(splitD, &QAction::triggered, this,
            [dispatch] { dispatch(QStringLiteral("split_down")); });
    helper.addToSection(splitD, QStringLiteral("pane"));

    // ---- action: Bookmark (placeholder) ----
    auto *bookmarkAct = new QAction(
        QIcon::fromTheme(QStringLiteral("bookmark-new")),
        i18n("Bookmark..."), this);
    bookmarkAct->setEnabled(false);
    bookmarkAct->setToolTip(
        i18n("Adding a bookmark is not available yet"));
    helper.addToSection(bookmarkAct, QStringLiteral("action"));

    // ---- action: Export as image ----
    auto *exportAct = new QAction(
        QIcon::fromTheme(QStringLiteral("image-x-generic")),
        i18n("Export as image"), this);
    connect(exportAct, &QAction::triggered, this,
            &CanvasFileView::showExportAsImageModal);
    helper.addToSection(exportAct, QStringLiteral("action"));

    // ---- view.linked submenu: single entry (Backlinks) ----
    auto *linkedSub = helper.addSubmenu(
        QStringLiteral("view.linked"),
        i18n("Open linked view"),
        QIcon::fromTheme(QStringLiteral("tab-detach")));
    auto *backlinksAct = new QAction(
        QIcon::fromTheme(QStringLiteral("go-previous")),
        i18n("Open backlinks"), this);
    connect(backlinksAct, &QAction::triggered, this, [dispatch] {
        dispatch(QStringLiteral("corbomite-backlinks:open"));
    });
    linkedSub->addToSection(backlinksAct, QStringLiteral("action"));

    // Note: plan calls for chaining to EditableFileView::onMoreOptionsMenu
    // for universal rename/move/delete/copy-path, but CanvasFileView is a
    // FileView (not EditableFileView). Promotion to EditableFileView is a
    // follow-up. The base View::onMoreOptionsMenu is an empty no-op, so
    // we call it here for forward compatibility if ever it gains body.
    FileView::onMoreOptionsMenu(helper);
}

} // namespace Corbomite
