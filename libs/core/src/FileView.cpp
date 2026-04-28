// libs/core/src/FileView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/FileView.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include <KLocalizedString>
#include <QJsonObject>

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
        m_pathChangedConn = {};
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
    if (doc)
        loadFile(doc);
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

} // namespace Corbomite
