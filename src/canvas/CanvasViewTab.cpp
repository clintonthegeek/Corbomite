// SPDX-License-Identifier: GPL-3.0-or-later
#include "CanvasViewTab.h"

#include <canvas/CanvasDocument.h>
#include <canvas/CanvasFilePickerDialog.h>
#include <canvas/CanvasScene.h>
#include <canvas/CanvasView.h>
#include <corbomite/core/MarkdownRenderEngine.h>
#include <corbomite/vault/FileManager.h>
#include <corbomite/vault/TFolder.h>
#include <corbomite/vault/Vault.h>

#include <KLocalizedString>

#include "corbomitesettings.h"

#include <QDir>
#include <algorithm>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>

namespace Corbomite {

namespace {

/// The app's one FileManager, which MainWindow parents to itself and
/// which MainWindow.cpp does not inject, so it is
/// resolved through the widget tree the same way CanvasFileView resolves
/// SessionManager. Null outside a MainWindow (tests, headless), where
/// "Convert to File…" has nowhere to write.
FileManager *fileManagerFor(QWidget *widget)
{
    auto *window = widget ? widget->window() : nullptr;
    return window ? window->findChild<FileManager *>() : nullptr;
}

/// Obsidian's new-file-name modal rejects the same three classes
/// (`fR.validate`, `app/own/1312-hR.js:81-82`): characters no filename may
/// contain (`ND`/`HD`, platform-conditional exactly as upstream's is), a
/// leading dot, and characters that would break links (`RD`/`zD`). Upstream
/// re-opens the modal with the error inline; stock QInputDialog can't, so the
/// caller re-asks. Empty means valid — upstream accepts an empty name here
/// too and lets createNewFile fall back to "Untitled" (`:330`).
QString newFileNameError(const QString &name)
{
#ifdef Q_OS_WIN
    const QString badChars = QStringLiteral("*\"\\/<>:|?");
#else
    const QString badChars = QStringLiteral("\\/:");
#endif
    for (const QChar c : badChars) {
        if (name.contains(c))
            return i18n("File name cannot contain any of these characters: %1", badChars);
    }
    if (name.startsWith(QLatin1Char('.')))
        return i18n("File name must not start with a dot.");
    const QString unsafeChars = QStringLiteral("#^[]|");
    for (const QChar c : unsafeChars) {
        if (name.contains(c))
            return i18n("Links will not work with file names containing any of these characters: %1",
                        unsafeChars);
    }
    return {};
}

} // namespace

CanvasViewTab::CanvasViewTab(const QString &filePath, const QString &vaultRoot, QWidget *parent)
    : QWidget(parent)
    , m_filePath(filePath)
    , m_vaultRoot(vaultRoot)
{
    m_document = new Canvas::CanvasDocument(this);
    m_view = new Canvas::CanvasView(this);

    m_document->loadFromFile(filePath);
    m_view->setDocument(m_document);
    m_view->canvasScene()->setSessionContext(m_vaultRoot, canvasRelativePath());

    // File-card paths in a .canvas file are vault-relative (Obsidian spec),
    // not relative to the canvas file's own directory. Fall back to the
    // canvas file's directory when no vault root is supplied.
    QString resolveBase = vaultRoot.isEmpty() ? QFileInfo(filePath).absolutePath() : vaultRoot;
    m_view->canvasScene()->setFileResolver([resolveBase](const QString &path) -> QString {
        QString fullPath = resolveBase + QLatin1Char('/') + path;
        QFile file(fullPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QString::fromUtf8(file.readAll());
    });

    // Distinguish a missing file-card target from an existing empty
    // file, so the placeholder only appears for a dangling card.
    m_view->canvasScene()->setFileExistsChecker([resolveBase](const QString &path) -> bool {
        if (path.isEmpty())
            return false;
        return QFileInfo::exists(resolveBase + QLatin1Char('/') + path);
    });

    m_view->canvasScene()->setFileSaver([resolveBase](const QString &path, const QString &content) {
        QString fullPath = resolveBase + QLatin1Char('/') + path;
        QFile file(fullPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(content.toUtf8());
        }
    });

    // "New file card…" candidate source. No Vault object is
    // reachable at this layer (CanvasViewTab only ever gets a vaultRoot
    // path string), so this scans the vault tree directly for markdown +
    // common attachment files rather than reusing Vault::getFiles(); a
    // real Vault-backed source is app-side follow-up work.
    // CanvasFilePickerDialog does the actual fuzzy filter/rank.
    QPointer<Canvas::CanvasView> viewPtr(m_view);
    m_view->canvasScene()->setFilePickerRequestor([resolveBase, viewPtr, this]() -> QString {
        static const QStringList kAttachmentExts = {
            QStringLiteral("md"), QStringLiteral("png"), QStringLiteral("jpg"),
            QStringLiteral("jpeg"), QStringLiteral("gif"), QStringLiteral("svg"),
            QStringLiteral("pdf"), QStringLiteral("canvas"),
        };

        QStringList candidates;
        if (m_fileLister) {
            // The VAULT's file list, not a filesystem
            // walk. The walk below is kept only as a fallback for callers
            // that never called setFileLister().
            candidates = m_fileLister();
            const auto hasAllowedExt = [](const QString &rel) {
                return kAttachmentExts.contains(QFileInfo(rel).suffix().toLower());
            };
            candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                            [&](const QString &rel) {
                                                return !hasAllowedExt(rel);
                                            }),
                             candidates.end());
        } else if (!resolveBase.isEmpty()) {
            const QDir root(resolveBase);
            QDirIterator it(resolveBase, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString abs = it.next();
                const QFileInfo fi(abs);
                if (!kAttachmentExts.contains(fi.suffix().toLower()))
                    continue;
                const QString rel = root.relativeFilePath(abs);
                // Skip dot-prefixed path segments (.obsidian, .trash, .git,
                // ...) — this scan has no Vault object to consult the real
                // config-dir/exclude list (see comment above), but hiding
                // dotfiles/dot-dirs matches Obsidian's own baseline
                // behavior and keeps internal config/plugin JSON out of the
                // picker.
                bool dotSegment = false;
                for (const auto &part : rel.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
                    if (part.startsWith(QLatin1Char('.'))) {
                        dotSegment = true;
                        break;
                    }
                }
                if (dotSegment)
                    continue;
                candidates << rel;
            }
        }
        candidates.sort(Qt::CaseInsensitive);

        if (!viewPtr)
            return {};
        return Canvas::CanvasFilePickerDialog::pickFile(viewPtr, candidates);
    });

    // Drag-drop candidate paths arrive as absolute filesystem paths
    // (text/uri-list); resolve to vault-relative, rejecting anything
    // outside resolveBase (no copy-into-vault here).
    m_view->canvasScene()->setVaultPathResolver([resolveBase](const QString &absoluteFilePath) -> QString {
        if (resolveBase.isEmpty())
            return {};
        const QDir root(resolveBase);
        const QString rel = root.relativeFilePath(absoluteFilePath);
        if (rel.startsWith(QStringLiteral("..")) || QDir::isAbsolutePath(rel))
            return {}; // outside the vault
        return rel;
    });

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    installConvertToFileSeams();

    // Autosave: arm on every mutation, write the whole document on timeout.
    // `modificationChanged(false)` comes back from saveToFile(), so only the
    // `true` edge arms the timer — otherwise a save would schedule its own
    // successor forever. The interval is read at arm time, so a change to
    // Settings → Editor → Autosave delay applies to the next edit without a
    // rewire.
    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    connect(m_autosaveTimer, &QTimer::timeout, this, [this] {
        if (m_document->isModified())
            save();
    });

    connect(m_document, &Canvas::CanvasDocument::modificationChanged,
            this, [this](bool modified) {
                if (modified)
                    m_autosaveTimer->start(CorbomiteSettings::self()->autoSaveDelayMs());
            });
    connect(m_document, &Canvas::CanvasDocument::modificationChanged,
            this, &CanvasViewTab::modificationChanged);
    connect(m_view->canvasScene(), &Canvas::CanvasScene::readOnlyChanged,
            this, [this](bool readOnly) {
                if (!m_localDataWriter)
                    return;
                // Obsidian writes `{readonly: true}` when locked and `null`
                // (i.e. removes the key) when not — app/own/2028-n9.js:917.
                m_localDataWriter(localDataKey(),
                                  readOnly ? QJsonObject{{QStringLiteral("readonly"), true}}
                                           : QJsonObject{});
            });
}

CanvasViewTab::~CanvasViewTab() = default;

void CanvasViewTab::setFileLister(std::function<QStringList()> lister)
{
    m_fileLister = std::move(lister);
}

QString CanvasViewTab::canvasRelativePath() const
{
    // Obsidian keys on TFile.path, i.e. vault-relative with forward slashes
    // (app/own/2028-n9.js:565 — "canvas-" + e.path). Outside a vault there is
    // no such path, so fall back to the absolute file path.
    QString rel = m_filePath;
    if (!m_vaultRoot.isEmpty()) {
        const QString candidate = QDir(m_vaultRoot).relativeFilePath(m_filePath);
        if (!candidate.startsWith(QStringLiteral("..")))
            rel = candidate;
    }
    return rel;
}

QString CanvasViewTab::localDataKey() const
{
    return QStringLiteral("canvas-") + canvasRelativePath();
}

Canvas::CanvasDocument *CanvasViewTab::canvasDocument() const
{
    return m_document;
}

void CanvasViewTab::refreshFileCards(const QString &vaultRelativePath)
{
    if (auto *scene = m_view ? m_view->canvasScene() : nullptr)
        scene->refreshFileCards(vaultRelativePath);
}

void CanvasViewTab::setLocalDataStore(LocalDataReader reader, LocalDataWriter writer)
{
    m_localDataReader = std::move(reader);
    m_localDataWriter = std::move(writer);
    if (m_localDataReader && m_view && m_view->canvasScene())
        applyLocalData(m_localDataReader(localDataKey()));
}

void CanvasViewTab::applyLocalData(const QJsonObject &data)
{
    // Obsidian's guard is `hasOwnProperty("readonly") && typeof
    // data.readonly == "boolean"` (app/own/2028-n9.js:879) — a hand-edited
    // half-state is ignored, not coerced.
    if (!data.contains(QStringLiteral("readonly")))
        return;
    const QJsonValue v = data.value(QStringLiteral("readonly"));
    if (!v.isBool())
        return;
    m_view->canvasScene()->setReadOnly(v.toBool());
}

void CanvasViewTab::setRenderEngine(MarkdownRenderEngine *engine)
{
    m_view->canvasScene()->setRenderEngine(engine);
}

QString CanvasViewTab::filePath() const
{
    return m_filePath;
}

bool CanvasViewTab::save()
{
    return m_document->saveToFile(m_filePath);
}

QByteArray CanvasViewTab::serializedBytes() const
{
    return m_document->serializeObsidianStyle();
}

QByteArray CanvasViewTab::diskBytes() const
{
    if (m_filePath.isEmpty())
        return {};
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};  // null, not an empty-but-present file
    return file.readAll();
}

CanvasViewTab::ExternalEdit CanvasViewTab::classifyExternalEdit(const QByteArray &ours,
                                                               const QByteArray &disk,
                                                               bool hasUnsavedEdits,
                                                               const QByteArray &lastSeenDisk)
{
    // Unreadable file: never act on a failed read.
    if (disk.isNull())
        return ExternalEdit::NoChange;
    // Already identical either way — nothing to reconcile, and the pending
    // autosave would write these very bytes.
    if (disk == ours)
        return ExternalEdit::NoChange;
    // This revision has been handled already; don't warn twice for one write.
    if (disk == lastSeenDisk)
        return ExternalEdit::NoChange;
    return hasUnsavedEdits ? ExternalEdit::KeepLocalAndWarn
                           : ExternalEdit::ReloadFromDisk;
}

bool CanvasViewTab::reloadFromDisk()
{
    if (m_filePath.isEmpty())
        return false;

    // A pending autosave would write the document we are about to replace.
    m_autosaveTimer->stop();

    if (!m_document->loadFromFile(m_filePath))
        return false;
    m_document->setModified(false);

    // loadFromJson() replaces the document's contents without emitting the
    // per-node signals the scene syncs on (the initial load happens before
    // the scene is attached), so hand the same document back to the scene to
    // repopulate. The camera lives on the view, so it survives.
    m_view->canvasScene()->setDocument(m_document);
    return true;
}

bool CanvasViewTab::isModified() const
{
    return m_document->isModified();
}

void CanvasViewTab::installConvertToFileSeams()
{
    QPointer<Canvas::CanvasView> viewPtr(m_view);

    m_view->canvasScene()->setFileNameRequestor([viewPtr]() -> QString {
        if (!viewPtr)
            return {};
        for (;;) {
            bool ok = false;
            const QString typed = QInputDialog::getText(
                                      viewPtr, i18n("Convert to file"), i18n("Note name:"),
                                      QLineEdit::Normal, i18n("Untitled"), &ok)
                                      .trimmed();
            if (!ok || !viewPtr)
                return {};
            const QString error = newFileNameError(typed);
            if (error.isEmpty())
                return typed.isEmpty() ? QStringLiteral("Untitled") : typed;
            QMessageBox::warning(viewPtr, i18n("Convert to file"), error);
        }
    });

    m_view->canvasScene()->setMarkdownFileCreator(
        [this](const QString &name, const QString &content) -> QString {
            auto *fm = fileManagerFor(this);
            if (!fm)
                return {};
            // The vault root, not the canvas's own folder: Obsidian's default
            // new-file location is "root" for markdown (app/own/0920-eE.js:1205)
            // and for canvas (app/own/2030-b9.js:48), so
            // getNewFileParent(canvasPath, name) resolves there too.
            auto *tf = fm->createNewMarkdownFile(fm->vault()->getRoot(), name,
                                                 content.toUtf8());
            return tf ? tf->path : QString();
        });
}

void CanvasViewTab::setFileNameRequestor(std::function<QString()> requestor)
{
    m_fileNameRequestor = std::move(requestor);
    if (m_view && m_view->canvasScene())
        m_view->canvasScene()->setFileNameRequestor(m_fileNameRequestor);
}

void CanvasViewTab::setMarkdownFileCreator(
    std::function<QString(const QString &, const QString &)> creator)
{
    m_markdownFileCreator = std::move(creator);
    if (m_view && m_view->canvasScene())
        m_view->canvasScene()->setMarkdownFileCreator(m_markdownFileCreator);
}

void CanvasViewTab::jumpToGroup()
{
    if (!m_view)
        return;
    auto *scene = m_view->canvasScene();
    if (!scene)
        return;
    const auto targets = scene->groupJumpTargets();
    if (targets.isEmpty())
        return;

    QVector<Canvas::CanvasFilePickerDialog::Entry> entries;
    entries.reserve(targets.size());
    for (const auto &target : targets)
        entries.append({target.label, target.nodeId});

    const QString chosen = Canvas::CanvasFilePickerDialog::pickPayload(
        m_view, entries, i18n("Jump to group"), i18n("Type to search groups…"));
    if (chosen.isEmpty())
        return;
    for (const auto &target : targets) {
        if (target.nodeId != chosen)
            continue;
        m_view->zoomToBbox(target.bounds);
        return;
    }
}

void CanvasViewTab::convertTextCardToFile(const QString &nodeId)
{
    if (m_view && m_view->canvasScene())
        m_view->canvasScene()->convertTextCardToFile(nodeId);
}

Canvas::CanvasScene *CanvasViewTab::canvasScene() const
{
    return m_view ? m_view->canvasScene() : nullptr;
}

Canvas::CanvasView *CanvasViewTab::canvasView() const
{
    return m_view;
}

} // namespace Corbomite
