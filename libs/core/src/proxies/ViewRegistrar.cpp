// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/ViewRegistrar.h"

namespace Corbomite {

void ViewRegistrar::registerView(const QString &, ViewFactory) {}
void ViewRegistrar::registerExtensions(const QStringList &, const QString &) {}
void ViewRegistrar::unregisterView(const QString &) {}

} // namespace Corbomite
