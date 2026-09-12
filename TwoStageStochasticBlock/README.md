# test/TwoStageStochasticBlock

A tester which provides initial tests for `TwoStageStochasticBlock`,
the SMS++ Block that wraps a deterministic-equivalent two-stage
stochastic program around an inner Block cloned once per scenario,
with a `DiscreteScenarioSet` attached and a set of "here-and-now"
non-anticipativity constraints linking the first-stage variables
across scenarios, as well as for `LagrangianDualSolver`,
`LagBFunction`, any `CDASolver` able to handle `C05Function` in the
`Objective` (such as `BundleSolver`), any `CDASolver` able to handle
Linear Programs (such as `MILPSolver` and its derived classes
`CPXMILPSolver` and `SCIPMILPSolver`), and for quite a lot of the
mechanics of the "core" SMS++ library.

This executable, given the filename of a netCDF file containing the
description of a `TwoStageStochasticBlock`, solves its deterministic
equivalent with a `:MILPSolver` and with a `LagrangianDualSolver`
using `BundleSolver` as the inner Solver, comparing the results
against each other (and against an optional reference objective value
passed on the command line). The running times are printed. The
relative tolerance for the comparison is fixed at `1e-5`.

The usage of the executable is the following:

       ./TSSB_test TSSB-file [BSC-file ws ref]
       BSC-file: BlockSolverConfig description [BSPar-2S.txt]
       ws:       0 = LagrangianDualSolver, 1 = reserved [0]
       ref:      reference objective value to compare against [none]

The inner Block cloned per scenario can be any SMS++ Block: the
tester is problem-agnostic. The instances shipped under `batches/`
happen to embed a `UCBlock` (for energy-community-type applications),
but nothing in the executable assumes a specific inner Block type —
any two-stage stochastic problem that can be expressed as a
`TwoStageStochasticBlock` is in scope.

A makefile is also provided that builds the executable including the
`TwoStageStochasticBlock`, `LagrangianDualSolver`, `BundleSolver`,
`MILPSolver` modules and the core SMS++ library, together with the
inner-Block module needed by the instances in `batches/` (currently
`UCBlock`).

## Configuration files

- `BSPar-2S.txt` — outer `BlockSolverConfig` registering `:MILPSolver`
  (default `GRBMILPSolver`) + `LagrangianDualSolver`. The
  `LagrangianDualSolver` parameters
  (`intPushCostToOwner=1`, `intDoEasy=1`, `dbltStar=-1`, etc.) are set
  so that the Lagrangian relaxation behaves the same way as in the
  standalone `tssb_solver` tool.
- `LPBSCfg.txt` — `BlockSolverConfig` for the `LagBFunction` instances
  produced by `LagrangianDualSolver` (CPLEX with LP relaxation
  enabled).
- `BSCfg.txt` — alternative LP/QP `BlockSolverConfig` (HiGHS with IPM)
  that may be referenced from `LPBSCfg.txt` when a deterministic LP
  oracle is required.
- `BSPar-2S-LD.txt` — outer `BlockSolverConfig` of the nested chain, in
  which each scenario sub-problem is solved by an inner
  `LagrangianDualSolver` (`LPBSCfg-LD.txt`) instead of a `:MILPSolver`.
  A component of the outer Lagrangian Dual is there an entire inner one,
  which is why every component is evaluated at each iteration
  (`dblMinNrEvls=-1`).
- `LPBSCfg-LD.txt` — the inner `LagrangianDualSolver` of that chain,
  whose components are the units of the scenario (`InnerBSCfg.txt`).
  `LPBSCfg-LD-noeasy.txt` is the same with `intDoEasy=0`, which the
  instances with no installable asset need: there every unit is "easy",
  and a Lagrangian Dual all of whose components are easy is not
  supported.


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
