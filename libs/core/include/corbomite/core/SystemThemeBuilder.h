// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_SYSTEMTHEMEBUILDER_H
#define CORBOMITE_CORE_SYSTEMTHEMEBUILDER_H

#include <markoff/Theme.h>

class KColorSchemeManager;

namespace Corbomite::Core::SystemThemeBuilder {

/// Build a `Markoff::Theme` reflecting the OS / KDE color scheme. Reads
/// palette roles from `KColorScheme(QPalette::Active, View)` + `Selection`.
/// Non-palette elements (callout accents, code syntax token colors) come
/// from a hand-tuned base layer differentiated by background luminance.
///
/// `mgr` is currently unused — KColorScheme reads the active palette
/// directly from QGuiApplication. Kept in the signature so future
/// implementations can opt to consult a specific manager (e.g. for
/// stash/preview).
Markoff::Theme buildFromKColorScheme(KColorSchemeManager *mgr = nullptr);

} // namespace Corbomite::Core::SystemThemeBuilder

#endif // CORBOMITE_CORE_SYSTEMTHEMEBUILDER_H
