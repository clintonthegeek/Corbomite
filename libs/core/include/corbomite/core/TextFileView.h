// libs/core/include/corbomite/core/TextFileView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/EditableFileView.h"

class QTimer;

namespace Corbomite {

class DataAdapter;

class TextFileView : public EditableFileView
{
    Q_OBJECT

public:
    explicit TextFileView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);

    void requestSave();
    void save(bool immediate = false);
    void saveImmediately();

    void setDataAdapter(DataAdapter *adapter);
    void setVaultRoot(const QString &root);

    /// True when there are unsaved edits (a debounced save is pending).
    bool isDirty() const { return m_dirty; }

    /// Write a recovery snapshot of `content` for the current file.
    /// `failure` marks a save-*failure* backup, which retention pruning never
    /// removes (Obsidian's `{"failure": true}` exemption). The store is
    /// deliberately OUTSIDE the vault root — see the 2026-04-27 decision: a
    /// recovery copy inside the vault shows up in the tree/search/graph and
    /// re-triggers `Vault::modified` for the leaf that just failed.
    void writeRecoverySnapshot(const QString &content, bool failure);

    virtual QString getViewData() const = 0;
    virtual void setViewData(const QString &data, bool clear) = 0;
    virtual void clear() = 0;

    void onExternalModify(const QString &relativePath);

Q_SIGNALS:
    void saved();
    void saveError(const QString &error);

protected:
    void onLoadFile(NoteDocument *file) override;
    void onUnloadFile(NoteDocument *file) override;

private:
    QString m_data;
    bool m_dirty = false;
    bool m_saving = false;
    bool m_saveAgain = false;
    QString m_lastSavedData;
    bool m_neverLoaded = true;
    QTimer *m_debounceTimer = nullptr;
    DataAdapter *m_adapter = nullptr;
    QString m_vaultRoot;

    static constexpr int SaveDebounceMs = 2000;
};

} // namespace Corbomite
