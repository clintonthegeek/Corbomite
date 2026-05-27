// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QUndoCommand>
#include <QVariant>

#include <functional>

namespace Corbomite {
class FileManager;
class TFile;
}  // namespace Corbomite

namespace Corbomite::Bases {

/// Undoable single-key frontmatter edit. All reads/writes happen inside one
/// FileManager::processFrontMatter mutator, so the stale check runs against the
/// frontmatter freshly parsed from disk (no MetadataCache async-lag race).
///
/// On external drift (the on-disk value no longer matches what this command
/// last wrote) the command does NOT overwrite: it calls `notify` with a
/// user-facing string and neutralizes itself (all further redo/undo are
/// no-ops). A no-op undo() still lets QUndoStack advance its index, so the
/// stale command is skipped and older history stays reachable.
class CmdSetFrontMatter : public QUndoCommand
{
public:
    CmdSetFrontMatter(Corbomite::FileManager *fm,
                      Corbomite::TFile *file,
                      QString key,
                      QVariant newValue,
                      std::function<void(const QString &)> notify);

    void redo() override;
    void undo() override;

private:
    Corbomite::FileManager *m_fm;
    Corbomite::TFile       *m_file;
    QString  m_key;
    QVariant m_newValue;
    QVariant m_oldValue;            // captured lazily on first redo()
    bool     m_oldCaptured = false;
    bool     m_neutralized = false;
    std::function<void(const QString &)> m_notify;
};

}  // namespace Corbomite::Bases
