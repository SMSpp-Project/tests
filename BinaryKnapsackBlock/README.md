# tests/BinaryKnapsackBlock

The suite of `BinaryKnapsackBlock`: every `:Solver` that solves a binary
knapsack, checked against one another and against the published optima of the
public benchmarks, and the Frank-Wolfe decomposition of a father `Block` whose
leaves are those same knapsacks.

`BinaryKnapsackBlock_test` writes the instance itself, drawing weights,
profits, integrality and capacity from the distributions its options describe.
It then attaches to the `Block` every `:Solver` its `BlockSolverConfig` names
and cross-checks what they return, on the instance as it is and while it
changes: the profits and the weights are modified over a `Range` or a
`Subset`, and items are fixed and released. With `-C` it reads a curated
`.csv` instead, loading every instance of a class in one process rather than
paying a start-up per instance. Those `.csv` carry the published optimum
beside the data, so the same pass checks the `:Solver` against one another and
all of them against a number nobody here computed.

The cross-check lineups are the `BSPar*`, and the principle is that a run
attaches at once every `:Solver` that has to give the same answer.
[BSPar.txt](BSPar.txt) is the pure-binary one: a `:MILPSolver` on the exact
MIP, the full-table `DPBinaryKnapsackSolver` with its parallel and its core
variants, the `BranchAndXSolver` in each of its exploration modes, and the
`GreedyRelaxationBinaryKnapsackSolver`. The last one solves a relaxation, so
it is held to bracketing the optimum rather than to equality.
[BSPar-mixed.txt](BSPar-mixed.txt) is the same for the instances that carry
continuous items, where the `BranchAndXSolver` has nothing to branch on and is
left out. [BSPar-fast.txt](BSPar-fast.txt) keeps one `:Solver` per family,
without the full-table DP, for the instances on which the whole lineup would
not finish. The rest are the harnesses of one path at a time, i.e., the lazy
bounding protocol, the reoptimization, the diving and the thread scaling of
the `BranchAndXSolver`. The `ComputeConfig` they share are the fragments
[MILPCfg.txt](MILPCfg.txt), [DPCfg.txt](DPCfg.txt), [CDPCfg.txt](CDPCfg.txt),
[PDPCfg.txt](PDPCfg.txt), [GRCfg.txt](GRCfg.txt) and
[BX-baseCfg.txt](BX-baseCfg.txt), each written once and pointed at by whoever
needs it.

The batteries are in [batches](batches), one per family of instances:

- [batch](batches/batch) and [batch-mixed](batches/batch-mixed), the random
  sweep on pure-binary and on mixed-binary instances, which have no external
  optimum and cross-check the `:Solver` against one another;

- [batch-negative](batches/batch-negative), the same cross-check on the two
  regimes the sweep does not reach, i.e. weights and profits mostly negative,
  where an item *frees* capacity and the normalization of the relaxation has
  to complement it, and small instances re-solved many times, which keep
  landing in the corner cases a larger one hits rarely;

- [batch-pisinger](batches/batch-pisinger), the curated Pisinger classes, each
  a single `.csv` carrying the published optimum, where the cross-check is
  therefore also against a reference; it is the battery that runs the
  decomposition as well;

- [batch-jooken](batches/batch-jooken) and
  [batch-pisinger-large](batches/batch-pisinger-large), which are not
  pass/fail: the hard instances of Jooken, Leyman and De Causmaecker are
  engineered to defeat every standard exact method, and the large Pisinger
  classes are where the tree search blows up while the core DP and the MILP
  keep scaling, so each `:Solver` is run one per process under a time limit
  and a memory cap and what the battery produces is the map of what each of
  them cracks, a timeout or an out-of-memory being an outcome and not a
  failure.

The curated data is fetched and unpacked by the `fetch_bk_data` and
`fetch_jooken_data` fixtures, the netCDF instances the decomposition reads are
written by the `run_bk2nc4` one, and every battery skips itself, saying so,
when what it reads is not there.


## The Frank-Wolfe decomposition of a father of K knapsacks

The knapsacks of this suite are also solved as the leaves of a decomposition.
One of them is read `K` times, the `K` copies become the sub-`Block` of a
father `AbstractBlock` carrying a random objective over their `Variable`, and
the `FrankWolfeSolver` that decomposes the father is compared with a
monolithic `:MILPSolver`; the oracle of each copy is its
`DPBinaryKnapsackSolver` ([BKBSCfg.txt](BKBSCfg.txt)). The tester is the
generic one, [`fw_test.cpp`](../fw_test.cpp), the same the suites of
`MCFBlock` and of `UCBlock` build on their own `Block`. Which `Block` is read
and which `:Solver` are attached is therefore the configurations' business,
and nothing of what follows is in the source. The dynamic programme is a
genuine oracle: it minimizes over the knapsack whatever linear objective it is
given, and a change of the profits reaches its own data, which is what the
decomposition does to it at every iteration.

What is compared is not an equality. Frank-Wolfe minimizes the father
objective over the product of the *convex hulls* of the knapsacks, and no
compact formulation describes the convex hull of a knapsack. The monolithic
`:MILPSolver` minimizes instead over the product of the knapsacks themselves,
so its optimum bounds from above what the decomposition computes; the
difference is the integrality gap. The battery therefore declares the
decomposition a relaxation (`-R`) and holds it to nothing (`-E`). What
survives is the check that matters: the variants have to agree with one
another, and none of them may pass the bound. The instances are the netCDF
ones [tools/batch](../../BinaryKnapsackBlock/tools/batch) writes out of the
curated archive the rest of the battery reads. They are minimization problems,
`bk2nc4` having changed the sign of the profits, a component of a
decomposition of a minimization problem having to have the sense of its
father.

The configurations are the same set, with the same names, in every suite that
poses this tester on its own `Block`. `FatherBSPar.txt` is the
meta-`BlockSolverConfig` that dispatches by classname: the father goes to
`FatherBSCfg.txt` and each leaf to the `BlockSolverConfig` that makes its
`:Solver` the oracle. `FatherBSCfg.txt` registers the monolithic reference and
one `FrankWolfeSolver` per variant of the decomposition, the way the `BSPar`
of a suite do with the `:Solver` of its `Block`, so that a single run
cross-checks the variants against one another and all of them against the
reference; the variants are vanilla, Away-step, Blended Pairwise, Away-step
with aggregation, vanilla with the oracles in parallel, and the two that take
the direction from a stabilized master of two and of ten pieces, the master of
the last being a `MasterProblemBlock` solved by the `:Solver` of
`MPBCfg-FW.txt`, which names a backend that solves QPs.

Each variant is written as an override of `FWCfg.txt`, the fragment holding
what they have in common, so that what a variant changes is the only thing its
lines say; the reference reads `MILPCfg-FW.txt`. An override block writes the
extra-`Configuration` slot even when it changes nothing of it, because one
that leaves it out is read on to the end of the stream and swallows the
`ComputeConfig` that follows it. What `intLMOObj` selects is not among the
variants: that parameter decides whether the sub-`Block` objectives enter the
problem at all, and not merely what the oracle is shown, so `LMOLinear` solves
a different problem and has nothing to be compared with here.

Three more shapes of the same lineup exist, and no battery rewrites a
configuration file to select a variant. `FatherBSPar-fast.txt` keeps the
reference and the two variants that between them walk the most of the
machinery, Away-step with aggregation and the master problem one, for the
instances on which a single run costs the best part of an hour;
`FatherBSPar-fw.txt` keeps a variant alone, with nothing to compare it with,
for the runs that only time it; `FatherBSPar-milp.txt` keeps the reference
alone, for the same reason.

## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

- **Donato Meoli**  
  Dipartimento di Informatica  
  Università di Pisa


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
