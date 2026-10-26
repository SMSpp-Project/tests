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

Every `HQPSolver` that the `BlockSolverConfig` given with `-S` registers to
the `HQPBlock` solves the same hierarchy in the same run, and each of them is
compared level by level with an independent implementation of the classical
approach, which solves each level in the null space of the equality tasks of
the higher ones with a small dense active-set method (`reference.h`): the
values of the levels, which are unique, always, and the solution `x` after
each level when the `HQPSolver` solves exactly the problem of the reference
(`int_HQP_Prox 1`). Since all of them are compared with the same reference,
they are compared with each other as well. `BSPar.txt` attaches one
`HQPSolver` for each inner Solver and setting:

- the inner Solvers are DAQP and qpOASES (dense, active set) and MOSEK and
  Clarabel (sparse, interior point), each one in a `BlockSolverConfig` of
  its own (`InnerBSCfg-<solver>.txt`, MOSEK with the tolerances of
  `MOSEKCfg.txt`);

- the settings are three `ComputeConfig`: `HQPCfg.txt`, with the rows of the
  lower levels relaxed and their slacks fixed and the center of the
  regularization at the solution of the previous level; `HQPCfg-free.txt`,
  with the rows of the lower levels in the model and their slacks free,
  which makes the Hessian singular; `HQPCfg-prox0.txt`, with the center of
  the regularization at 0.

An `HQPSolver` whose inner Solver is not in the build is reported and left
out, and if none of them has one the tester has nothing to do and says so
with the status 77.

The usage of the executable is the following:

       ./HQPBlock_test [options] [<file>]
       -P, --previous <file>  the instance of the previous step, also
                              changed into this one by Modification

plus the options that every SMS++ tester understands (`--help` lists them
all), among which `-S`, the `BlockSolverConfig`, and `-E`, the tolerance on
the values of the levels of each `HQPSolver` in the order of the
`BlockSolverConfig`. Without a file, the hierarchies are built by the tester:
random ones, one with a level of equalities only, one of inequalities only
and an empty one, an inequality task that cannot be satisfied at its level
and one already satisfied (whose values are known), dependent rows, weights
different from 1, and a step of Model Predictive Control that changes `b`,
`d`, `A` (also out of the pattern of a row) and `C` of the same `HQPBlock`
between two solves, checked also against a new `HQPBlock` solved from
scratch. A file is an instance in the netCDF format of `HQPBlock`, which
carries with the `HQPBlock` the solution of each level of the code that
generated it, against which the values are also compared.

The batteries are:

- `batches/batch-generated`, the hierarchies built by the tester; the
  interior-point inner Solvers are held to `1e-5` on the values rather than
  `1e-6`, since an inequality task satisfied exactly at its level leaves
  them a slack of the order of the square root of their accuracy;

- `batches/batch-radial`, the instances that HQPBlock distributes in
  `data/nc4` (some steps of a centralized Model Predictive Control of 8
  robots crossing in the middle of a circle), downloaded from the GitLab
  Package Registry when the battery is run; each step that follows the
  previous one in the run is also reached by Modification of the
  `HQPBlock` of the previous step, as the Model Predictive Control does.
