# test/HQPBlock

A tester of `HQPBlock` and `HQPSolver`, the module of the hierarchical
(lexicographic) Quadratic Programs of task-priority control. Level `p` of a
hierarchy has the tasks `we_p (A_p x - b_p) = v_p` and `wi_p (C_p x - d_p) <=
w_p`, with `w_p >= 0`, and it minimizes `||v_p||^2 / 2 + ||w_p||^2 / 2` (plus
a regularization) keeping the slacks of the higher levels at their optimal
values. `HQPSolver` solves the levels one after the other with an inner
Solver taken from the factory, a `:MILPSolver` for convex QPs here, which is
why the tester lives here rather than in the `test` of the module: the unit
test of the module checks the `HQPBlock` alone, with the core library only.

Every `HQPSolver` that the configuration given with `-S` registers to the
`HQPBlock` solves the same hierarchy in the same run, and each of them is
compared level by level with an independent implementation of the classical
approach, which solves each level in the null space of the equality tasks of
the higher ones with a small dense active-set method (`reference.h`): the
values of the levels, which are unique, always, and the solution `x` after
each level when the `HQPSolver` solves exactly the problem of the reference
(`intProxCenter 1`). Since all of them are compared with the same reference,
they are compared with each other as well. `BSPar.txt` is a map from the
classname of a Block to its `BlockSolverConfig`, of which the tester applies
to each Block that of its classname, to the Block alone and not to its
sub-Blocks: `HQPBSCfg.txt` attaches to the `HQPBlock` one `HQPSolver` for
each inner Solver and setting,

- the inner Solvers are DAQP and qpOASES (dense, active set) and MOSEK and
  Clarabel (sparse, interior point), each one in a `BlockSolverConfig` of
  its own (`InnerBSCfg-<solver>.txt`, MOSEK with the tolerances and the
  presolve of `MOSEKCfg.txt`, Clarabel with the tolerances of
  `ClarabelCfg.txt`, qpOASES with H declared positive definite in
  `QPOASESCfg.txt`);

- the settings are three `ComputeConfig`: `HQPCfg.txt`, with the rows of the
  lower levels relaxed and their slacks fixed and the center of the
  regularization at the solution of the previous level; `HQPCfg-free.txt`,
  with the rows of the lower levels in the model and their slacks free,
  which makes the Hessian singular; `HQPCfg-prox0.txt`, with the center of
  the regularization at 0;

- the 13th and the 14th are DAQP and qpOASES in the first setting with
  each solve from scratch (`InnerBSCfg-daqp-cold.txt`,
  `InnerBSCfg-qpoases-cold.txt`, with `intDAQPWarmStart 0` and
  `intQPOASESHotStart 0`) rather than from the previous solve of the same
  level, which the inner Solver of each level of an `HQPSolver`
  re-optimizes from step to step;

- the 15th is `HQPStageSolver` of HQPBlock in the first setting
  (`InnerBSCfg-stage.txt`), the primal active-set method whose subproblems
  are solved by a Riccati recursion over the stages, which applies only to
  a hierarchy with the stage structure of `HQPBlock` and is left out of the
  others;

- the 16th is DAQP in the first setting with a single inner Solver for all
  the levels (`intOneInner 1`), each level starting from the solution of
  the previous one rather than from the same level of the previous solve;

while the `HQPDistBlock`, where each hierarchy built by the tester is split
among 3 owners of consecutive blocks of columns, is solved once for each
formulation of `HQPDistForms.txt`, given with `-F`, a pair of a
`BlockConfig`, which gives the structure, and a `BlockSolverConfig`:

- F0, the structure `kConsensus` (`HQPDistBCfg-cons.txt`), with copies in
  the sub-Blocks and consensus rows in the root, and the `HQPSolver` of
  `HQPDistBSCfg.txt`: DAQP in the three settings and qpOASES in the first
  one, each solving the whole tree, i.e., the centralized hierarchy written
  that way, and in the first setting the Lagrangian dual of the consensus
  rows (`InnerBSCfg-lds.txt`), a `LagrangianDualSolver` (`LDCfg.txt`) whose
  sub-Blocks are solved by DAQP with the rows held to `1e-10`
  (`InnerBSCfg-daqp-tol10.txt`, `DAQPCfg-tol10.txt`) and whose dual by a
  `BundleSolver` with the master of the Benders decomposition below
  (`BDSMCfg.txt`), left out by a build without them;

- F1, the structure `kBenders` (`HQPDistBCfg-bds.txt`), with the coupling
  rows in the root, a copy in the root of each column of an equality
  coupling row and the parts of the others in the sub-Blocks, and those of
  `HQPDistBSCfg-bds.txt`, both in the first setting: DAQP on the whole
  tree, whose Hessian is singular, with the proximal-point iterations
  stopped at `1e-12` (`InnerBSCfg-daqp-prox.txt`, `DAQPCfg-prox.txt`), and
  the Benders decomposition of `InnerBSCfg-bds.txt`, a
  `BendersDecompositionSolver` (`BDSCfg.txt`) whose subproblems are solved
  by DAQP and whose master by Gurobi (`BDSMCfg.txt`), left out by a build
  without it, as it is and with the `Objective` of each level at value
  about 1 (`intLevelScale`).

The `HQPSolver` of each formulation follow those of the `HQPBlock`, and
those of the previous formulation, in the positions of `-E`, `-X` and
`-I`.

An `HQPSolver` whose inner Solver is not in the build is reported and left
out, and if none of them has one the tester has nothing to do and says so
with the status 77; one whose inner Solver does not apply to a hierarchy
(it throws `std::invalid_argument`, as `HQPStageSolver` does on a hierarchy
with no stage structure) is reported and left out for that hierarchy.

The usage of the executable is the following:

       ./HQPBlock_test [options] [<file>]
       -P, --previous <file>  the instance of the previous step, also
                              changed into this one by Modification
       -I, --fewer <i>:<j>[,...]
                              the HQPSolver at position i (from 1) takes
                              fewer iterations than that at j in the
                              steps reached by Modification
       -X, --xtol <tol>[,...]
                              the tolerance on x of each HQPSolver, in
                              the order of the BlockSolverConfig, an
                              empty one for the default

plus the options that every SMS++ tester understands (`--help` lists them
all), among which `-S`, the configuration, `-F`, the formulations of the
`HQPDistBlock`, and `-E`, the tolerance on the values of the levels of each
`HQPSolver` in the order of the `BlockSolverConfig` (those of the
`HQPDistBlock` after those of the `HQPBlock`). Without a file, the
hierarchies are built by the tester: random ones, one with a level of
equalities only, one of inequalities only
and an empty one, an inequality task that cannot be satisfied at its level
and one already satisfied (whose values are known), dependent rows, weights
different from 1, hard rows (not in the `HQPDistBlock`, which does not
split them), a small Model Predictive Control of two robots over four
stages with its stage structure, a step of Model Predictive Control that
changes `b`, `d`,
`A` (also out of the pattern of a row) and `C` of the same `HQPBlock`
between two solves, checked also against a new `HQPBlock` solved from
scratch, and a run of 10 small such steps; with `-F`, each of them also as
an `HQPDistBlock` in each formulation, the step out of the pattern
building its sub-Blocks anew. A file is an instance in the
netCDF format of `HQPBlock`, which carries with the `HQPBlock` the solution
of each level of the code that generated it, against which the values are
also compared; a file without it, e.g., an instance of TROTS, too large for
the dense reference, is solved by all the `HQPSolver`, which are compared
with each other (when one of them has `dblSlackTol > 0`, only up to the
first level where the values differ, the next ones being not unique) and
with the point of its group `HQPPoint`, if any, which none of them may
exceed lexicographically, and the time of each `compute()` is printed; a
file whose Block is not an `HQPBlock`, e.g., the weighted sum of the
objectives of a case of TROTS as a Linear Program, is solved by the Solver
of the `BlockSolverConfig` of its classname, compared with each other and
with the value at the point of its group `HQPPoint`, which none of them may
exceed. The
iterations
of the inner Solvers of each `HQPSolver` in the solves of the steps reached
by Modification (those of the run of Model Predictive Control of the
hierarchies built by the tester, that from the file of `-P`) are printed,
and those of the pairs of `-I` compared.

The batteries are:

- `batches/batch-generated`, the hierarchies built by the tester; the
  interior-point inner Solvers are held to `1e-5` on the values rather than
  `1e-6`, since an inequality task satisfied exactly at its level leaves
  them a slack of the order of the square root of their accuracy, the
  Lagrangian dual to `5e-5` on the values, since it solves the dual of each
  level to `1e-9` relative and the gap leaves the slacks of the level
  larger than at the optimum, which makes the next levels easier, the
  Benders decomposition to `1e-5` on the values and `1e-2` on `x` rather
  than `1e-3`, since it stops when no cut is violated by more than `1e-9`
  relative and with the regularization `1e-3` such a gap leaves `x` free by
  about `1e-3` (with the scale of the levels, which makes the gap relative
  to the level, to `1e-5` on the values and the default on `x`), and in
  the run of Model Predictive Control DAQP and qpOASES
  must take fewer iterations than the same ones solving from scratch;

- `batches/batch-qcqp`, the hierarchies with quadratic and with conic
  hard rows built by the tester with `-Q quad` and `-Q conic`, each loaded
  into an `HQPBlock` once for each formulation of `HQPForms-quad.txt` or
  `HQPForms-conic.txt`, given with `-F`, and compared only among the
  `HQPSolver` of all of them, to `1e-5` on the values, the hard rows of
  the data being checked at the solution of each one
  (`HQPBlock::is_feasible()`, to `1e-6`): the quadratic rows with a
  `QuadFunction` and MOSEK and Gurobi as inner Solvers
  (`HQPBCfg.txt`, `HQPBSCfg-quad.txt`), MOSEK with the tolerances `1e-8`
  of `MOSEKCfg-tol8.txt`, which unlike those of `MOSEKCfg.txt` it reaches
  on levels with almost no interior, and the quadratic rows, or the
  conic ones, as the cuts that the `HQPBlock` separates, of Kelley
  (`HQPBCfg.txt`, conic only) or at the boundary point
  (`HQPBCfg-cuts.txt`), with inner Solvers that solve and separate in a
  loop (`intRelaxIntVars 2`), MOSEK (again at `1e-8`), DAQP and qpOASES,
  the active-set ones also with a single inner Solver for all the levels
  (`HQPBSCfg-cuts.txt`);

- `batches/batch-radial`, the instances that HQPBlock distributes in
  `data/nc4` (some steps of a centralized Model Predictive Control of 8
  robots crossing in the middle of a circle, with the control horizon 3
  and, with the stage structure, two steps with the horizon 10),
  downloaded from the GitLab
  Package Registry when the battery is run; each step that follows the
  previous one in the run is also reached by Modification of the
  `HQPBlock` of the previous step, as the Model Predictive Control does,
  and the iterations of that step are printed (not compared, since a step
  may need no more than one iteration per level even from scratch);

- `batches/batch-trots`, the proton cases of TROTS, the Radiotherapy
  Optimisation Test Set (`Protons_04` unless `TROTS_CASES` lists others),
  taken out of `Protons.zip`, downloaded from Zenodo the first time, and
  converted by `tools/trots2nc4.py` of HQPBlock, with the constraints of
  the case as hard rows, in the directory `TROTS_DIR` (default `trots`);
  `BSPar-trots.txt` gives the `HQPBlock` the `HQPSolver` of
  `HQPBSCfg-trots.txt`, with the slacks fixed up to `1e-3`
  (`HQPCfg-trots.txt`) and MOSEK (`InnerBSCfg-mosek-tol8.txt`) or Gurobi
  (`InnerBSCfg-gurobi.txt`) as inner Solvers, one for each level or one for
  all of them; the weighted sum of the objectives of the case, of which
  the solution of TROTS is the optimum, is also converted
  (`trots2nc4.py --weighted`) and solved by MOSEK and Gurobi
  (`AbstractBSCfg-trots.txt`). A case takes hours and some 25 GB of
  memory, hence the battery is run by hand and is not among the tests of
  the suite.
