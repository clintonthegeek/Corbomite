// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QString>

namespace Canvas {

class CanvasDocument;

/// Tracks the live `CanvasDocument` for each open canvas, keyed by the
/// vault-relative canvas path (the same key `FileManager` uses). The rename
/// rewriter consults it so it mutates the open instance instead of writing a
/// second copy to disk — the single-writer rule the Cluster M M6.2 plan
/// requires.
///
/// Main-thread only. Attach/detach happen when a canvas view is created and
/// destroyed; renames happen on the same thread.
class CanvasDocumentRegistry {
public:
    static CanvasDocumentRegistry &instance();

    /// Register `doc` as the live document for `path`. Replaces any previous
    /// entry for the path.
    void attach(const QString &path, CanvasDocument *doc);
    /// Remove `doc` from `path` if it is still the registered live document.
    /// A no-op when a newer document has since been attached.
    void detach(const QString &path, CanvasDocument *doc);
    /// The live document for `path`, or nullptr when the canvas is closed.
    CanvasDocument *documentFor(const QString &path) const;

private:
    CanvasDocumentRegistry() = default;
    QHash<QString, CanvasDocument *> m_docs;
};

}  // namespace Canvas
