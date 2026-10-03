// libs/core/src/SystemThemeBuilder.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Release hygiene C.3 — this file USED to be ~219 lines, of which 205 were
// inside an `#if 0` block. Deleted for the same reason as MarkdownRenderer: a
// disabled implementation is not a specification, it is a fossil.
//
// WHAT SURVIVES, AND WHY IT IS NOT DEAD: `buildFromKColorScheme` was defined
// OUTSIDE the old `#if 0`, so it is a real symbol that the linker resolves and
// that `tests/app/tst_system_theme_builder` references (behind an `if(FALSE)`
// gate). It currently has no live callers — `ThemeService` derives its system
// theme from the application palette instead — but dropping the definition
// would leave the installed header declaring a function with no definition,
// which breaks that test the moment its gate is removed.
//
// Retiring the class entirely means deleting the header and that gated test
// together. That is a decision, not a cleanup.
#include "corbomite/core/SystemThemeBuilder.h"

namespace Corbomite::Core::SystemThemeBuilder {

Markoff::Theme buildFromKColorScheme(KColorSchemeManager *)
{
    return {};
}

} // namespace Corbomite::Core::SystemThemeBuilder
