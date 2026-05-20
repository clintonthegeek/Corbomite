// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CORBOMITE_CORE_LINKUTILS_H
#define CORBOMITE_CORE_LINKUTILS_H

#include <QList>
#include <QString>

#include <markoff/parser/Document.h>

namespace Corbomite {

/// Result of resolving a wikilink subpath (`#heading`, `#^block`, `#[^footnote]`)
/// against a Markdown document.
struct SubpathResolution {
    enum class Kind { None, Heading, Block, Footnote };
    Kind kind = Kind::None;

    /// Byte offset in the document source where the resolved range starts.
    /// -1 when kind == None.
    int startOffset = -1;

    /// Byte offset where the resolved range ends (exclusive). -1 means "to EOF"
    /// (used for the last heading in the document, matching Obsidian's
    /// `end = null` convention).
    int endOffset = -1;

    /// For Kind::Heading: index into `Document::headings()` of the matched
    /// heading. -1 otherwise.
    int headingIndex = -1;
};

/// Strip display-unfriendly characters from a heading string for use in the UI
/// (not in links). Mirrors Obsidian's `stripHeading` (`utils/stripHeading.js:5–7`).
///
/// Regex `AT`: replace any of `! " # $ % & ( ) * + , . : ; < = > ? @ ^ \` { | } ~
/// / [ ] \ \r \n` with a space, then collapse runs of whitespace to a single
/// space and trim.
QString stripHeading(const QString &heading);

/// Narrower strip for use when generating a `[[Note#Heading]]` link fragment.
/// Mirrors Obsidian's `stripHeadingForLink` (`utils/stripHeadingForLink.js:5–7`).
///
/// Regex `PT`: replace any of `: # | ^ \ \r \n` or the multi-char tokens `%%`,
/// `[[`, `]]` with a space, then collapse runs of whitespace to a single space
/// and trim. Preserves most printable punctuation so heading links stay
/// readable; removes only characters that would break wikilink syntax.
QString stripHeadingForLink(const QString &heading);

/// Resolve a subpath (`#heading`, `#^block`, `#[^footnote]`, or empty) against
/// a parsed document. Mirrors Obsidian's `resolveSubpath`
/// (`utils/resolveSubpath.js:5–79`).
///
/// - Empty or non-leading-`#` subpath → Kind::None.
/// - `#^id` → Kind::Block, offsets span the paragraph containing `^id`.
/// - `#[^id]` → Kind::Footnote, offsets span the footnote definition.
/// - `#heading` → Kind::Heading, offsets span from the heading line to the next
///   heading of the same or higher level (or EOF, signalled by endOffset = -1).
///   Heading match is case-insensitive after `stripHeading()` normalisation.
///
/// `source` is the complete markdown source. It's used to locate byte offsets;
/// it must be the same source that produced `doc`.
SubpathResolution resolveSubpath(const Markoff::Document &doc,
                                 const QString &source,
                                 const QString &subpath);

} // namespace Corbomite

#endif // CORBOMITE_CORE_LINKUTILS_H
