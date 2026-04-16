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
    void writeBackup(const QString &content);

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
