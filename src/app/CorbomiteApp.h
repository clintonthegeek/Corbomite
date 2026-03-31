// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

namespace Corbomite {

class CorbomiteApp : public QObject {
    Q_OBJECT
public:
    explicit CorbomiteApp(QObject *parent = nullptr);
};

} // namespace Corbomite
