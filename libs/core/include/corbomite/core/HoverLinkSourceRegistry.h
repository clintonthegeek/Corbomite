// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QString>

#include "corbomite/core/HoverLinkSource.h"

namespace Corbomite {

// Per-view-type opt-in registry for hover-link sources.
//
// Registration is keyed by the source `id` (a view-type identifier such as
// "editor", "search", "backlinks", "graph"). The shipped registry initialises
// with the built-in view types so the editor can fire hover-link previews
// out of the box.
//
// When Cluster N's plugin layer lands, plugin views will call
// registerSource() at onload and unregisterSource() at onunload — exactly
// matching Obsidian's `workspace.registerHoverLinkSource(id, info)` API.
class HoverLinkSourceRegistry : public QObject {
    Q_OBJECT

public:
    explicit HoverLinkSourceRegistry(QObject *parent = nullptr);

    // Returns false if a source with the same id was already registered; the
    // existing entry stays in place (Obsidian behaviour: first wins).
    bool registerSource(const HoverLinkSource &source);
    void unregisterSource(const QString &id);

    bool isRegistered(const QString &id) const;
    HoverLinkSource lookup(const QString &id) const;
    QList<HoverLinkSource> allSources() const;

    // Convenience: install the built-in source set documented in
    // domains/workspace.md §7 + domains/rendering.md §11.
    void registerBuiltins();

Q_SIGNALS:
    void sourceRegistered(const QString &id);
    void sourceUnregistered(const QString &id);

private:
    QHash<QString, HoverLinkSource> m_sources;
};

} // namespace Corbomite
