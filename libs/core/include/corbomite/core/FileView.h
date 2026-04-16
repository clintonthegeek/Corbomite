// libs/core/include/corbomite/core/FileView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/ItemView.h"

namespace Corbomite {

class NoteDocument;

class FileView : public ItemView
{
    Q_OBJECT

public:
    explicit FileView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);

    NoteDocument *file() const;
    bool loadFile(NoteDocument *file);

    virtual bool canAcceptExtension(const QString &ext) const;

    QString getDisplayText() const override;
    QJsonObject getState() const override;
    void setState(const QJsonObject &state) override;

protected:
    virtual void onLoadFile(NoteDocument *file);
    virtual void onUnloadFile(NoteDocument *file);
    void onOpen() override;
    void onClose() override;

    NoteDocument *m_file = nullptr;
    bool m_navigation = true;
    bool m_allowNoFile = false;
};

} // namespace Corbomite
