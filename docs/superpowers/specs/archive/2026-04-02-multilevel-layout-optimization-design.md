# MultilevelLayout Optimization Design

**Goal:** Reduce `computeLayout` wall-clock time from 400s to <30s on 10k-node vaults by improving convergence detection, coarsening quality, and refinement iteration counts.

**Scope:** All changes are in `MultilevelLayout.cpp` and `MultilevelLayout.h`. No changes to the runtime `ForceLayoutEngine` or rendering pipeline.

---

## 1. Early Convergence in layoutLevel

`layoutLevel` currently runs a fixed iteration count with no convergence check. Positions that are already stable (e.g., after interpolation from a coarser level) waste hundreds of iterations doing nothing.

**Mechanism:** At the end of each iteration, track max displacement across all nodes. If max displacement falls below a threshold for 3 consecutive iterations, break early.

**Threshold:** Add `convergenceThreshold` to `MultilevelConfig` (default 0.5 scene units). This is the max displacement below which we consider the layout converged. Exposed as a config parameter so the benchmark can sweep values.

**Coarsest vs. refinement:** Both use the same mechanism. The coarsest level will naturally take more iterations since it starts from scratch (BFS radial placement). Refinement levels will converge quickly since positions come pre-seeded from interpolation.

## 2. Improved Coarsening

Current coarsening uses greedy maximal matching that picks the heaviest unmatched neighbor. With uniform edge weights (all 1.0 on the first coarsening pass), "heaviest" is meaningless — match quality depends on random traversal order. The obsidian-hub vault only reduces from 10,000 to 7,233 nodes in one step (ratio 0.72).

**Fix: Degree-ordered matching.** Sort nodes by degree ascending before matching. Low-degree nodes have fewer matching opportunities, so matching them first produces better overall coverage. High-degree nodes (hubs) have many neighbors and can match later.

**Fix: Multi-pass coarsening.** After the first matching pass, unmatched nodes exist because all their neighbors were already matched. Run a second pass on unmatched nodes only — allow them to merge with already-matched neighbors (creating triplet coarse nodes with combined weight). This increases the coarsening ratio without changing the graph structure.

**Expected impact:** Coarsening ratio should improve from ~0.72 to ~0.55 per level, producing 5-6 levels instead of 2. The coarsest level should have ~200-500 nodes instead of 7,233.

## 3. Reduced Refinement Iterations

Current formula: `max(50, sqrt(n) * 10)` — gives 774 iterations at n=6000.

**New formula:** `max(20, sqrt(n) * 3)` — gives 232 iterations at n=6000. Combined with early convergence (change 1), most refinement levels will exit well before this cap since positions are already close to correct from the coarser level.

The coarsest level keeps its existing `config.coarsestIterations` (default 500). It starts from BFS placement and needs more iterations to find a good layout.

## 4. Full-Pipeline Benchmark

The existing benchmark measures `layoutLevel` in isolation. Add a second benchmark function that measures `computeLayout` end-to-end (coarsening + all levels + interpolation) across the same topology/size matrix. This is what the user actually experiences.

**Output:** Same table format as the existing benchmark, but with columns for total pipeline time, number of coarsening levels produced, and coarsest level size.

---

## Config Changes

```cpp
struct MultilevelConfig {
    double repelForce = 1500.0;
    double linkForce = 0.05;
    double linkDistance = 100.0;
    double centerForce = 0.01;
    int minCoarseNodes = 50;
    int coarsestIterations = 500;
    double convergenceThreshold = 0.5; // NEW: max displacement for early exit
};
```

## Files Changed

- `libs/forcegraph/include/forcegraph/MultilevelLayout.h` — add `convergenceThreshold` to config
- `libs/forcegraph/src/MultilevelLayout.cpp` — all three optimizations
- `libs/forcegraph/tests/tst_benchmark_layout.cpp` — add full-pipeline benchmark
- `libs/forcegraph/tests/tst_forcelayout.cpp` — update tests for new behavior

## Success Criteria

- 5000-node benchmark cases: total pipeline time under 2 seconds
- obsidian-hub vault (10k nodes): under 30 seconds for `computeLayout`
- All existing tests continue to pass
- Layout quality: connected nodes still closer than distant nodes (existing test assertions)
