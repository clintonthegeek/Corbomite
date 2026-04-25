// libs/core/src/WorkspaceWindow.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceWindow.h"

#include <QRandomGenerator>

namespace Corbomite {

namespace {
QString generateId()
{
    static const char chars[] = "0123456789abcdef";
    QString result;
    result.reserve(16);
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < 16; ++i)
        result.append(QLatin1Char(chars[rng->bounded(16)]));
    return result;
}
} // namespace

WorkspaceWindow::WorkspaceWindow(QObject *parent)
    : QObject(parent)
    , m_id(generateId())
{
}

WorkspaceWindow::~WorkspaceWindow() = default;

QString WorkspaceWindow::id() const { return m_id; }
void WorkspaceWindow::setId(const QString &id) { m_id = id; }

} // namespace Corbomite
