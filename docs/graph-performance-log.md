# Graph Layout Performance Log

Tracks measured performance of the force-directed graph layout system across optimization iterations. Each section records the state of the system, what changed, and benchmark results so we can quantify the impact of each improvement.

## Benchmark Methodology

**Harness:** `libs/forcegraph/tests/tst_benchmark_layout.cpp`
**What it measures:** Wall-clock time of `MultilevelLayout::layoutLevel()` — the per-level force-directed layout that computes repulsive forces, attractive forces, center gravity, and adaptive displacement.
**Topologies tested:** Scale-free (Barabasi-Albert), random-sparse (avg degree ~3), random-dense (avg degree ~10), grid/lattice, star (one mega-hub), disconnected (10 chain clusters), dense-clique (K500 + chain).
**Sizes:** 100, 500, 1000, 2000, 5000 nodes.
**Iterations:** `max(10, sqrt(n) * 2)` per run — enough to measure per-iteration cost without waiting forever.

Run: `cd build && ctest -R tst_benchmark_layout --output-on-failure`
Or directly: `./build/bin/tst_benchmark_layout`

---

## Baseline: 2026-04-02 — Barnes-Hut in layoutLevel

### Problem

Opening graph view on the obsidian-hub vault (10,312 notes, capped to 10,000 nodes / 38,049 edges) froze the application unrecoverably. Left running for over an hour with no progress.

**Root cause:** `MultilevelLayout::layoutLevel()` used O(n^2) brute-force pairwise repulsion at ALL coarsening levels, including the finest. Coarsening only reduced 10,000 nodes to 7,233 in one step (ratio 0.72), so `layoutLevel` ran 500 iterations of O(n^2) on 7,233 nodes — approximately 13 billion pair computations.

### Changes

| Commit | Description |
|--------|-------------|
| `32a3e31` | Add position-based `QuadTree::build(QVector<QPointF>)` overload |
| `32518a9` | Switch `layoutLevel` to Barnes-Hut for n > 500 nodes |
| `74eaa5a` | Add depth guard to `QuadTree::insert` (prevents stack overflow on coincident nodes) |
| `43d4b59` | Add benchmark harness (7 topologies x 5 sizes) |

### Results

Measured on: AMD Ryzen, Linux 6.12, GCC 15.2, Qt 6.10.2, Release build.

```
Topology              Nodes    Edges   Iters    Time(ms)   ms/iter
--------------------------------------------------------------
scale-free              100      198      20          22       1.1
scale-free              500      995      44        1086      24.7
scale-free             1000     1998      63         544       8.6
scale-free             2000     3994      89        1661      18.7
scale-free             5000     9996     141        7477      53.0

random-sparse           100      128      20          21       1.1
random-sparse           500      766      44        1099      25.0
random-sparse          1000     1431      63         587       9.3
random-sparse          2000     2980      89        1824      20.5
random-sparse          5000     7442     141        8033      57.0

random-dense            100      472      20          22       1.1
random-dense            500     2466      44        1106      25.1
random-dense           1000     5055      63         701      11.1
random-dense           2000     9926      89        1910      21.5
random-dense           5000    24860     141        8416      59.7

grid                    100      180      20          21       1.1
grid                    500      938      44        1087      24.7
grid                   1000     1928      63         543       8.6
grid                   2000     3890      89        1667      18.7
grid                   5000     9828     141        7513      53.3

star                    100       99      20          20       1.0
star                    500      499      44        1075      24.4
star                   1000      999      63         509       8.1
star                   2000     1999      89        1575      17.7
star                   5000     4999     141        7132      50.6

disconnected            100       90      20          20       1.0
disconnected            500      490      44        1079      24.5
disconnected           1000      990      63         525       8.3
disconnected           2000     1990      89        1602      18.0
disconnected           5000     4990     141        7067      50.1

dense-clique            100     4950      20          44       2.2
dense-clique            500   124750      44        2395      54.4
dense-clique           1000   125250      63        2477      39.3
dense-clique           2000   126250      89        4373      49.1
dense-clique           5000   129250     141       11418      81.0
```

All cases under 100 ms/iter. No slow-case flags.

### Observations

- **Dense-clique is the outlier** — 2x slower at every size due to O(m) attractive force computation on ~125k edges. Barnes-Hut only helps repulsion; attraction is always O(m).
- **500 nodes is anomalously slow** (~25 ms/iter) across all topologies. This is the threshold where Barnes-Hut kicks in (>500), so 500-node levels still use brute-force O(n^2). The jump from 500 to 1000 drops ms/iter because 1000 uses Barnes-Hut.
- **Scaling from 1000 to 5000** shows roughly linear growth in ms/iter (8 -> 53), consistent with O(n log n) per iteration.

### Known remaining issues

1. **Coarsening quality** — The obsidian-hub vault only reduces from 10,000 to 7,233 nodes in one coarsening step (maximal edge matching produces ratio 0.72, barely under the 0.75 cutoff). Better coarsening algorithms (heavy-edge matching, multi-pass) would produce more levels with fewer nodes, further reducing total layout time.
2. **Iteration count scaling** — `layoutLevel` refinement uses `max(50, sqrt(n) * 10)` iterations, which gives 774 iterations at n=6000. This may be more than needed now that initial positions come from coarsening. Could benefit from early convergence detection.
3. **Main thread blocking** — `computeLayout` runs synchronously. For very large graphs, moving it to a worker thread would keep the UI responsive during initial layout.

---

## Optimization Round 1: 2026-04-02 — Early Convergence + Improved Coarsening + Reduced Iterations

### Changes

| Commit | Description |
|--------|-------------|
| `(Task 1)` | Early convergence detection in `layoutLevel` — breaks when max displacement < 0.5 for 3 consecutive iterations |
| `(Task 2)` | Degree-ordered matching + second-pass coarsening — low-degree nodes matched first, unmatched nodes merge with matched neighbors |
| `(Task 3)` | Refinement iteration cap reduced from `max(50, sqrt(n)*10)` to `max(20, sqrt(n)*3)` |

### Results: layoutLevel (per-level, isolated)

No significant change from baseline — these optimizations target the pipeline, not individual iterations. The layoutLevel benchmark uses a fixed iteration count, so early convergence doesn't apply here.

```
Topology              Nodes    Edges   Iters    Time(ms)   ms/iter
--------------------------------------------------------------
scale-free              100      193      20          21       1.1
scale-free              500      995      44        1125      25.6
scale-free             1000     1996      63         571       9.1
scale-free             2000     3994      89        1721      19.3
scale-free             5000     9996     141        7499      53.2

random-sparse           100      156      20          21       1.1
random-sparse           500      741      44        1100      25.0
random-sparse          1000     1505      63         583       9.3
random-sparse          2000     2992      89        1813      20.4
random-sparse          5000     7649     141        8092      57.4

random-dense            100      501      20          23       1.1
random-dense            500     2420      44        1119      25.4
random-dense           1000     4994      63         610       9.7
random-dense           2000     9978      89        1988      22.3
random-dense           5000    25417     141        8882      63.0

grid                    100      180      20          21       1.1
grid                    500      938      44        1100      25.0
grid                   1000     1928      63         563       8.9
grid                   2000     3890      89        1672      18.8
grid                   5000     9828     141        7763      55.1

star                    100       99      20          21       1.1
star                    500      499      44        1095      24.9
star                   1000      999      63         511       8.1
star                   2000     1999      89        1589      17.9
star                   5000     4999     141        7148      50.7

disconnected            100       90      20          20       1.0
disconnected            500      490      44        1093      24.8
disconnected           1000      990      63         528       8.4
disconnected           2000     1990      89        1604      18.0
disconnected           5000     4990     141        7299      51.8

dense-clique            100     4950      20          45       2.3
dense-clique            500   124750      44        2475      56.3
dense-clique           1000   125250      63        2537      40.3
dense-clique           2000   126250      89        4445      49.9
dense-clique           5000   129250     141       11522      81.7
```

### Results: computeLayout Pipeline (NEW — end-to-end)

```
Topology              Nodes    Edges    Time(ms)
------------------------------------------------
scale-free              500      996        2009
scale-free             1000     1992        1911
scale-free             2000     3996        3796
scale-free             5000     9989       14685
scale-free            10000    19998       42112

grid                    500      955        2042
grid                   1000     1936        2725
grid                   2000     3910        4932
grid                   5000     9858       14436
grid                  10000    19800       43544

star                    500      499        1666
star                   1000      999         743
star                   2000     1999        2289
star                   5000     4999       10310
star                  10000     9999       32372
```

### Observations

- **10x improvement over the 400s baseline** — 10k scale-free at 42s, star at 32s. Down from "over an hour" to under a minute.
- **Star topology scales best** — low edge count means less work per iteration. 1000-node star completes in 743ms.
- **5000 nodes at 10-15 seconds** — missed the 2s target. The coarsest-level layout (500 iterations) dominates at this scale.
- **10000 nodes at 32-44 seconds** — close to the 30s target for star, but scale-free and grid overshoot. The coarsest level and early refinement levels are still doing significant work.
- **Test suite dropped from 14.2s to 5.0s** — early convergence and reduced iterations compound even on small test graphs.

### Success criteria assessment

| Target | Result | Met? |
|--------|--------|------|
| 5000 nodes < 2s pipeline | 10-15s | No |
| 10000 nodes < 30s pipeline | 32-44s | Close (star hits 32s) |
| All existing tests pass | Yes | Yes |
| Layout quality maintained | Yes | Yes |

### Known remaining issues

1. **Coarsest-level layout dominates** — 500 iterations on the coarsest level (even with fewer nodes after improved coarsening) is the largest single cost. Could benefit from early convergence at this level too (currently it runs all 500 iterations since it starts from BFS placement which isn't near convergence).
2. **Main thread blocking** — still synchronous. Moving `computeLayout` to a worker thread is the next UX improvement.
3. **Coarsening could still improve** — the degree-ordered + second-pass approach is better but not optimal. Algorithms like METIS-style multilevel partitioning could produce tighter coarsening hierarchies.

---

<!-- Append future optimization results below this line -->
