// libs/core/src/TextFileView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/TextFileView.h"
#include "corbomite/core/DiffMatchPatch.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/PathUtils.h"
#include "corbomite/core/DataAdapter.h"

#include <QTimer>
#include <QDateTime>
#include <QFileInfo>
#include <QStandardPaths>

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
        writeBackup(currentData);
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

    QString merged = DiffMatchPatch::threeWayMerge(m_lastSavedData, currentView, freshDisk);
    m_lastSavedData = freshDisk;
    setViewData(merged, false);
}

void TextFileView::writeBackup(const QString &content)
{
    if (m_vaultRoot.isEmpty() || !m_adapter || !m_file) return;

    // Backup destination lives OUTSIDE the vault so the recovery copy
    // doesn't appear in the file tree, search index, tag/graph views, or
    // re-trigger Vault::modified for the leaf that just failed to save.
    const QString dataRoot =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dataRoot.isEmpty()) return;

    // Per-vault subdir keyed by basename + stable 12-char SHA-256 prefix of
    // the absolute vault path (same keying as PathUtils::vaultId / the index +
    // metadata-cache location). Uses a sibling "file-recovery" subtree so the
    // two concerns don't share a directory.
    const QString id = PathUtils::vaultId(m_vaultRoot);
    if (id.isEmpty()) return;

    const QString recoveryDir =
        dataRoot + QStringLiteral("/file-recovery/") + id;
    m_adapter->mkpath(recoveryDir);

    QString baseName = QFileInfo(m_file->relativePath()).baseName();
    QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    timestamp.replace(QLatin1Char(':'), QLatin1Char('-'));
    QString backupPath = recoveryDir + QLatin1Char('/') + baseName
                         + QLatin1Char('-') + timestamp + QStringLiteral(".md");

    m_adapter->write(backupPath, content);
}

} // namespace Corbomite
