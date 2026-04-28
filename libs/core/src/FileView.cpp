// libs/core/src/FileView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/FileView.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include <KLocalizedString>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>

namespace Corbomite {

FileView::FileView(WorkspaceLeaf *leaf, QWidget *parent)
    : ItemView(leaf, parent)
{
}

NoteDocument *FileView::file() const { return m_file; }

bool FileView::loadFile(NoteDocument *file)
{
    if (m_file) {
        if (m_pathChangedConn)
            disconnect(m_pathChangedConn);
        if (m_deletedConn)
            disconnect(m_deletedConn);
        m_pathChangedConn = {};
        m_deletedConn = {};
        onUnloadFile(m_file);
        m_file = nullptr;
    }

    if (!file) return true;

    m_file = file;
    try {
        onLoadFile(file);
    } catch (...) {
        m_file = nullptr;
        return false;
    }
    // Mirror Obsidian's `FileView.onload` subscription to `vault.on('rename')`.
    // When the open file is renamed (programmatic or external), the cached
    // NoteDocument's relativePath shifts and `pathChanged` fires; re-emit
    // `displayTextChanged` so the leaf's tab caption refreshes from the new
    // `name()`. Audit: views.md §"Top suspected bugs" — title not refreshed
    // on external rename.
    m_pathChangedConn = connect(file, &NoteDocument::pathChanged,
                                  this, [this](const QString &) {
        Q_EMIT displayTextChanged();
    });
    // Mirror `vault.on('delete')` per-FileView subscription. When the file
    // is dropped (programmatic remove/trash or external deletion), null
    // the cached pointer so subsequent saves can't silently re-create the
    // file via `Vault::modify`, and request the leaf to close. Audit:
    // views.md §"Top suspected bugs" — open file deleted externally orphans
    // leaf.
    m_deletedConn = connect(file, &NoteDocument::deleted, this, [this] {
        m_file = nullptr;
        requestLeafClose();
    });
    Q_EMIT displayTextChanged();
    return true;
}

bool FileView::canAcceptExtension(const QString &) const { return false; }

QString FileView::getDisplayText() const
{
    if (m_file)
        return m_file->name();
    return i18n("No file");
}

QJsonObject FileView::getState() const
{
    QJsonObject state;
    if (m_file)
        state[QStringLiteral("file")] = m_file->relativePath();
    return state;
}

void FileView::setState(const QJsonObject &state)
{
    QString filePath = state[QStringLiteral("file")].toString();
    if (filePath.isEmpty())
        return;
    if (!m_leaf)
        return;
    auto *reg = m_leaf->registry();
    if (!reg)
        return;
    auto *doc = reg->resolveFile(filePath);
    if (doc) {
        loadFile(doc);
        return;
    }
    // The file no longer exists (e.g. session restore after the file was
    // deleted out-of-band). Don't leave a zombie tab — close the leaf on
    // the next event-loop turn. Mirrors Obsidian's `n.close = true`
    // branch in FileView.setState. Audit: views.md §"Top suspected bugs"
    // — `FileView::setState` swallows missing-file silently.
    if (!m_allowNoFile)
        requestLeafClose();
}

void FileView::onLoadFile(NoteDocument *) {}
void FileView::onUnloadFile(NoteDocument *) {}

void FileView::onOpen()
{
    ItemView::onOpen();
}

void FileView::onClose()
{
    if (m_file) {
        onUnloadFile(m_file);
        m_file = nullptr;
    }
    ItemView::onClose();
}

void FileView::requestLeafClose()
{
    QPointer<WorkspaceLeaf> leaf(m_leaf);
    QTimer::singleShot(0, this, [leaf] {
        if (!leaf) return;
        if (auto *ws = leaf->workspace())
            ws->closeLeaf(leaf);
    });
}

} // namespace Corbomite
