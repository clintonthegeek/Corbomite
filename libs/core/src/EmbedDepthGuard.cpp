// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#include "corbomite/core/EmbedDepthGuard.h"

#include <KLocalizedString>

namespace Corbomite::Core {

QString EmbedDepthGuard::placeholder(const QString &targetLabel)
{
    return i18n("[%1 — embed depth exceeded]", targetLabel);
}

} // namespace Corbomite::Core
