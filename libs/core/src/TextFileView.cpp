// libs/core/src/TextFileView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/TextFileView.h"
#include "corbomite/core/ThreeWayMerge.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/RecoveryStore.h"
#include "corbomite/core/DataAdapter.h"

#include <QTimer>

namespace Corbomite {

TextFileView::TextFileView(WorkspaceLeaf *leaf, QWidget *parent)
    : EditableFileView(leaf, parent)
    , m_debounceTimer(new QTimer(this))
{
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(SaveDebounceMs);
    connect(m_debounceTimer, &QTimer::timeout, this, [this] { save(); });
}

void TextFileView::setDataAdapter(DataAdapter *adapter) { m_adapter = adapter; }
void TextFileView::setVaultRoot(const QString &root) { m_vaultRoot = root; }

void TextFileView::requestSave()
{
    m_dirty = true;
    m_debounceTimer->start();
}

void TextFileView::save(bool immediate)
{
    if (!m_file) return;
    if (m_neverLoaded) return;

    if (m_saving) {
        if (!immediate)
            m_saveAgain = true;
        return;
    }

    QString currentData = getViewData();
    if (m_lastSavedData == currentData)
        return;

    m_saving = true;
    QString previousLastSaved = m_lastSavedData;

    if (immediate) {
        m_data = QString();
        m_lastSavedData = QString();
        m_neverLoaded = true;
        clear();
    } else {
        m_data = currentData;
        m_lastSavedData = currentData;
    }

    bool success = false;
    if (m_adapter && !m_vaultRoot.isEmpty()) {
        QString absPath = m_vaultRoot + QLatin1Char('/') + m_file->relativePath();
        success = m_adapter->write(absPath, currentData);
    }

    if (!success) {
        m_lastSavedData = previousLastSaved;
        writeRecoverySnapshot(currentData, /*failure=*/true);
        Q_EMIT saveError(m_file->relativePath());
    } else {
        m_dirty = false;
        Q_EMIT saved();
    }

    m_saving = false;
    if (m_saveAgain && !immediate) {
        m_saveAgain = false;
        save();
    }
}

bool TextFileView::hasUnsavedChanges() const
{
    if (m_dirty)
        return true;
    return m_file && m_file->isModified();
}

void TextFileView::saveImmediately()
{
    if (m_dirty)
        save(true);
}

void TextFileView::onLoadFile(NoteDocument *file)
{
    EditableFileView::onLoadFile(file);
    if (!m_adapter || m_vaultRoot.isEmpty()) return;

    QString absPath = m_vaultRoot + QLatin1Char('/') + file->relativePath();
    auto content = m_adapter->read(absPath);
    if (content) {
        m_lastSavedData = *content;
        m_data = *content;
        m_neverLoaded = false;
        setViewData(*content, true);
    }
}

void TextFileView::onUnloadFile(NoteDocument *file)
{
    m_debounceTimer->stop();
    if (m_dirty)
        save(true);
    EditableFileView::onUnloadFile(file);
}

void TextFileView::onExternalModify(const QString &relativePath)
{
    if (!m_file || m_file->relativePath() != relativePath) return;
    if (m_saving) return;
    if (!m_adapter || m_vaultRoot.isEmpty()) return;

    QString absPath = m_vaultRoot + QLatin1Char('/') + m_file->relativePath();
    auto freshOpt = m_adapter->read(absPath);
    if (!freshOpt) return;

    const QString &freshDisk = *freshOpt;
    if (m_lastSavedData == freshDisk) return;

    QString currentView = getViewData();
    if (currentView == freshDisk) {
        m_lastSavedData = freshDisk;
        return;
    }

    QString merged = ThreeWayMerge::threeWayMerge(m_lastSavedData, currentView, freshDisk);
    m_lastSavedData = freshDisk;
    setViewData(merged, false);
}

// Save-failure path only. Periodic snapshots are written by RecoveryService,
// which owns a RecoveryStore and the retention policy; both share the same
// on-disk layout through RecoveryStore.
void TextFileView::writeRecoverySnapshot(const QString &content, bool failure)
{
    if (!m_file) return;
    RecoveryStore(m_adapter, m_vaultRoot)
        .write(m_file->relativePath(), content, failure);
}

} // namespace Corbomite
