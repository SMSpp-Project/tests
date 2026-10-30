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
  presolve of `MOSEKCfg.txt`);

- the settings are three `ComputeConfig`: `HQPCfg.txt`, with the rows of the
  lower levels relaxed and their slacks fixed and the center of the
  regularization at the solution of the previous level; `HQPCfg-free.txt`,
  with the rows of the lower levels in the model and their slacks free,
  which makes the Hessian singular; `HQPCfg-prox0.txt`, with the center of
  the regularization at 0;

- the last two are DAQP and qpOASES in the first setting with each solve
  from scratch (`InnerBSCfg-daqp-cold.txt`, `InnerBSCfg-qpoases-cold.txt`,
  with `intDAQPWarmStart 0` and `intQPOASESHotStart 0`) rather than from the
  previous solve of the same level, which the inner Solver of each level of
  an `HQPSolver` re-optimizes from step to step;

while the `HQPDistBlock`, where each hierarchy built by the tester is split
among 3 owners of consecutive blocks of columns, is solved once for each
formulation of `HQPDistForms.txt`, given with `-F`, a pair of a
`BlockConfig`, which gives the structure, and a `BlockSolverConfig`:

- F0, the structure `kConsensus` (`HQPDistBCfg-cons.txt`), with copies in
  the sub-Blocks and consensus rows in the root, and the `HQPSolver` of
  `HQPDistBSCfg.txt`, each one solving the whole tree, i.e., the
  centralized hierarchy written that way: DAQP in the three settings and
  qpOASES in the first one;

- F1, the structure `kBenders` (`HQPDistBCfg-bds.txt`), with the coupling
  rows in the root and their parts in the sub-Blocks, and that of
  `HQPDistBSCfg-bds.txt`, DAQP on the whole tree in the first setting.

The `HQPSolver` of each formulation follow those of the `HQPBlock`, and
those of the previous formulation, in the positions of `-E` and `-I`.

An `HQPSolver` whose inner Solver is not in the build is reported and left
out, and if none of them has one the tester has nothing to do and says so
with the status 77.

The usage of the executable is the following:

       ./HQPBlock_test [options] [<file>]
       -P, --previous <file>  the instance of the previous step, also
                              changed into this one by Modification
       -I, --fewer <i>:<j>[,...]
                              the HQPSolver at position i (from 1) takes
                              fewer iterations than that at j in the
                              steps reached by Modification

plus the options that every SMS++ tester understands (`--help` lists them
all), among which `-S`, the configuration, `-F`, the formulations of the
`HQPDistBlock`, and `-E`, the tolerance on the values of the levels of each
`HQPSolver` in the order of the `BlockSolverConfig` (those of the
`HQPDistBlock` after those of the `HQPBlock`). Without a file, the
hierarchies are built by the tester: random ones, one with a level of
equalities only, one of inequalities only
and an empty one, an inequality task that cannot be satisfied at its level
and one already satisfied (whose values are known), dependent rows, weights
different from 1, a step of Model Predictive Control that changes `b`, `d`,
`A` (also out of the pattern of a row) and `C` of the same `HQPBlock`
between two solves, checked also against a new `HQPBlock` solved from
scratch, and a run of 10 small such steps; with `-F`, each of them also as
an `HQPDistBlock` in each formulation, the step out of the pattern
building its sub-Blocks anew. A file is an instance in the
netCDF format of `HQPBlock`, which carries with the `HQPBlock` the solution
of each level of the code that generated it, against which the values are
also compared. The iterations
of the inner Solvers of each `HQPSolver` in the solves of the steps reached
by Modification (those of the run of Model Predictive Control of the
hierarchies built by the tester, that from the file of `-P`) are printed,
and those of the pairs of `-I` compared.

The batteries are:

- `batches/batch-generated`, the hierarchies built by the tester; the
  interior-point inner Solvers are held to `1e-5` on the values rather than
  `1e-6`, since an inequality task satisfied exactly at its level leaves
  them a slack of the order of the square root of their accuracy, and in
  the run of Model Predictive Control DAQP and qpOASES must take fewer
  iterations than the same ones solving from scratch;

- `batches/batch-radial`, the instances that HQPBlock distributes in
  `data/nc4` (some steps of a centralized Model Predictive Control of 8
  robots crossing in the middle of a circle), downloaded from the GitLab
  Package Registry when the battery is run; each step that follows the
  previous one in the run is also reached by Modification of the
  `HQPBlock` of the previous step, as the Model Predictive Control does,
  and the iterations of that step are printed (not compared, since a step
  may need no more than one iteration per level even from scratch).
