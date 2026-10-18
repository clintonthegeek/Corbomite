// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../KilnRunner.h"

#include <QJsonObject>
#include <QString>

namespace Corbomite {

/// A runner that answers immediately.
///
/// The point of the seam: the interesting decision in this feature is what the panel
/// does with the engine's answer, and a test that waits for a real subprocess to
/// finish tests the subprocess. `autoAnswer` is off by default so a test can look at
/// what was handed over before anything comes back — the "refuses before it spawns"
/// case depends on that.
class FakeKilnRunner : public KilnRunner
{
    Q_OBJECT
public:
    FakeKilnRunner()
        : KilnRunner(nullptr)
    {
    }

    bool run(const QString &enginePath, const QString &mode, const QJsonObject &config) override
    {
        ++calls;
        lastEngine = enginePath;
        lastMode = mode;
        lastConfig = config;
        if (autoAnswer)
            Q_EMIT finished(answerExit, answerReport, answerError);
        return true;
    }

    int calls = 0;
    QString lastEngine;
    QString lastMode;
    QJsonObject lastConfig;

    bool autoAnswer = false;
    int answerExit = 0;
    QJsonObject answerReport;
    QString answerError;
};

} // namespace Corbomite
