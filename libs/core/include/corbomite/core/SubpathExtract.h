// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>

namespace Corbomite {
/// Extract the section of `markdown` addressed by an Obsidian-style subpath
/// ("#heading" or "#^block-id"). Empty subpath → returns `markdown` unchanged.
QString extractMarkdownSubpath(const QString &markdown, const QString &subpath);
}
