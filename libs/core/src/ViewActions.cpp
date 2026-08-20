// libs/core/src/ViewActions.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/ViewActions.h"

namespace Corbomite {

ViewActions::ViewActions(QObject *parent)
    : QObject(parent)
    , KXMLGUIClient()
{
}

ViewActions::~ViewActions() = default;

} // namespace Corbomite
