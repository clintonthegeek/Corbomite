// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "corbomite/vault/Plugin.h"

namespace Corbomite { class MainWindow; }

namespace NoteStats {

class NoteStatsPlugin : public Corbomite::Plugin
{
    Q_OBJECT
public:
    NoteStatsPlugin(QObject *parent, const QVariantList &args);
    ~NoteStatsPlugin() override;

    QObject *createView(Corbomite::MainWindow *mw) override;
};

} // namespace NoteStats
