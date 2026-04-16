// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/DiffMatchPatch.h"

#include <QStringList>
#include <QVector>
#include <algorithm>

namespace Corbomite {
namespace {

QStringList splitLines(const QString &text)
{
    if (text.isEmpty())
        return {};
    return text.split(QLatin1Char('\n'));
}

// Simple line-level LCS producing a list of matching line pairs (indexA, indexB)
QVector<std::pair<int, int>> lcs(const QStringList &a, const QStringList &b)
{
    const int m = a.size(), n = b.size();
    // DP table
    QVector<QVector<int>> dp(m + 1, QVector<int>(n + 1, 0));
    for (int i = 1; i <= m; ++i)
        for (int j = 1; j <= n; ++j)
            dp[i][j] = (a[i - 1] == b[j - 1])
                            ? dp[i - 1][j - 1] + 1
                            : std::max(dp[i - 1][j], dp[i][j - 1]);

    // Backtrack to get matching pairs
    QVector<std::pair<int, int>> result;
    int i = m, j = n;
    while (i > 0 && j > 0) {
        if (a[i - 1] == b[j - 1]) {
            result.prepend({i - 1, j - 1});
            --i; --j;
        } else if (dp[i - 1][j] >= dp[i][j - 1]) {
            --i;
        } else {
            --j;
        }
    }
    return result;
}

} // anonymous namespace

QString DiffMatchPatch::threeWayMerge(const QString &base,
                                      const QString &local,
                                      const QString &remote)
{
    if (base == remote) return local;
    if (base == local) return remote;

    QStringList baseLines = splitLines(base);
    QStringList localLines = splitLines(local);
    QStringList remoteLines = splitLines(remote);

    // Find common lines between base and local, and base and remote
    auto baseLocalMatches = lcs(baseLines, localLines);
    auto baseRemoteMatches = lcs(baseLines, remoteLines);

    // Build a map: for each base line index, what does it map to in local/remote?
    // -1 means "deleted in that version"
    QVector<int> baseToLocal(baseLines.size(), -1);
    QVector<int> baseToRemote(baseLines.size(), -1);

    for (auto &[bi, li] : baseLocalMatches)
        baseToLocal[bi] = li;
    for (auto &[bi, ri] : baseRemoteMatches)
        baseToRemote[bi] = ri;

    // Walk through base lines, building merged output.
    // Strategy: for each region of base, check if local or remote changed it.
    // If only one changed, take that change. If both changed, remote wins.
    QStringList result;

    // Track where we are in local and remote
    int localPos = 0;
    int remotePos = 0;

    for (int bi = 0; bi < baseLines.size(); ++bi) {
        bool localKept = baseToLocal[bi] >= 0;
        bool remoteKept = baseToRemote[bi] >= 0;

        if (localKept && remoteKept) {
            // Both kept this base line. First, emit any insertions before this point.
            // Local insertions: lines in local between localPos and baseToLocal[bi]
            for (int li = localPos; li < baseToLocal[bi]; ++li)
                result.append(localLines[li]);
            // Remote insertions: lines in remote between remotePos and baseToRemote[bi]
            for (int ri = remotePos; ri < baseToRemote[bi]; ++ri)
                result.append(remoteLines[ri]);
            // Emit the common line
            result.append(baseLines[bi]);
            localPos = baseToLocal[bi] + 1;
            remotePos = baseToRemote[bi] + 1;
        } else if (localKept && !remoteKept) {
            // Remote deleted this line — accept the deletion (don't emit)
        } else if (!localKept && remoteKept) {
            // Local deleted this line — accept the deletion (don't emit)
        } else {
            // Both deleted — don't emit
        }
    }

    // Emit remaining insertions after the last matched base line
    for (int li = localPos; li < localLines.size(); ++li)
        result.append(localLines[li]);
    for (int ri = remotePos; ri < remoteLines.size(); ++ri)
        result.append(remoteLines[ri]);

    return result.join(QLatin1Char('\n'));
}

} // namespace Corbomite
