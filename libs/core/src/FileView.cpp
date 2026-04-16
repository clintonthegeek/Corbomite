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
