// src/editor/MarkdownView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/TextFileView.h"

namespace Corbomite {

class NoteEditorWidget;
class HoverPopover;
class EditorSuggestManager;
class Vault;

class MarkdownView : public TextFileView
{
    Q_OBJECT

public:
    explicit MarkdownView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);

    static View *factory(WorkspaceLeaf *leaf);

    QString getViewType() const override;
    QString getDisplayText() const override;
    QString getIcon() const override;

    QString getViewData() const override;
    void setViewData(const QString &data, bool clear) override;
    void clear() override;

    bool canAcceptExtension(const QString &ext) const override;

    bool setCursorLine(int line) override;

    QJsonObject getState() const override;
    void setState(const QJsonObject &state) override;
    QJsonObject getEphemeralState() const override;
    void setEphemeralState(const QJsonObject &state) override;

    NoteEditorWidget *editorWidget() const;

    /// Ensure a YAML frontmatter block exists, then append a blank property
    /// row. If the view is currently in Reading mode, switch to LivePreview
    /// so the new row is visible. Cluster R Task 3.2 — backs the
    /// `markdown:add-metadata-property` command.
    void insertFrontmatterProperty();

    void setVault(Vault *vault);
    void setHoverPopover(HoverPopover *popover);
    void setEditorSuggestManager(EditorSuggestManager *manager);

protected:
    void onOpen() override;
    void onClose() override;
    void onLoadFile(NoteDocument *file) override;

private:
    NoteEditorWidget *m_editorWidget;
};

} // namespace Corbomite
