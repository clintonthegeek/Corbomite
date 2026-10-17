// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include <optional>

#include <QByteArray>
#include <QString>

namespace Corbomite {

/// Per-extension link rewriter, the Corbomite counterpart of Obsidian's
/// `metadataCache.linkUpdaters[ext]` registry
/// (`docs/obsidian-audit/domains/metadata.md` §7; the canvas updater is `t8`,
/// `domains/canvas.md` §1). Markdown link rewriting lives in `FileManager`
/// itself because `.md` links carry byte-span positions into the body; this
/// seam is for non-Markdown, link-bearing formats (`.canvas`) whose
/// references are structural fields, not body offsets.
///
/// `FileManager` owns the registry and calls the rewriter while rewriting a
/// vault rename. The interface lives in `Corbomite::Core` so both `libs/vault`
/// (which calls it) and `libs/canvas` (which implements it) can depend on it
/// without a cycle.
class LinkRewriter {
public:
    virtual ~LinkRewriter() = default;

    /// Maps a bare link target — a vault-relative path with no `#subpath` or
    /// `|alias` — to its post-rename path. Returns nullopt when the target
    /// does not move.
    using TargetMapper = std::function<std::optional<QString>(const QString &path)>;

    /// Extension handled, lowercase and without the dot ("canvas"). Matched
    /// against `TFile::extension`.
    virtual QString extension() const = 0;

    /// Rewrite the file at `path` (vault-relative) whose current bytes are
    /// `bytes`, remapping every link target through `mapTarget`.
    ///
    /// Returns the new bytes when the caller should write them to disk.
    /// Returns nullopt when nothing changed, or when the rewriter already
    /// applied the change to a live document — the open-canvas single-writer
    /// case, where writing disk behind the open instance would desync it.
    virtual std::optional<QByteArray> rewrite(const QString &path,
                                              const QByteArray &bytes,
                                              const TargetMapper &mapTarget) = 0;
};

}  // namespace Corbomite
