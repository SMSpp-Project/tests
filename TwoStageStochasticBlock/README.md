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

A second tester, `TSSB_BDS_test`, puts the three ways of solving
the same two-stage stochastic investment problem one against the
other on the same data: the extensive form given to a `:MILPSolver`,
the generic Benders decomposition of `BendersDecompositionSolver`,
whose master carries the design Variable and whose subproblems are the
scenarios, and the ad hoc one an `InvestmentBlock` over the whole
`TwoStageStochasticBlock` carries, i.e., an `InvestmentFunction` whose
value is the entire stochastic problem and which therefore yields one
aggregated linearization per iteration. Being three formulations of
one problem the optima must coincide, which is what is checked; the
iterations and the times are printed. It is built only where
`BendersDecompositionSolver` and `InvestmentBlock` are in the build,
the rest of the suite running without them.

The same tester also takes an instance written in its extensive form,
i.e., with the design replicated in the scenarios and tied by the
non-anticipativity `Constraint`: no file format carries the structure
the Benders `Solver` asks for, since `AbstractBlock` only deserializes
a .lp/.mps model and a model of its own cannot name the `Variable` of
a sub-`Block`, so that structure is assembled around the `Block` the
file gives, one copy of the here-and-now `Variable` in the root and
one wrapper per scenario carrying the coupling. Whatever `Solver` the
`BlockSolverConfig` names are then cross-checked on the `Block` that
comes out, a `:MILPSolver` reading it whole being the extensive form.

The usage of the second executable is the following:

       ./TSSB_BDS_test [TSSB-file] [-S BSC-file] [-B BC-file] [-r ref]
       TSSB-file: instance to read [tssb_investment.nc4]
       BSC-file:  BlockSolverConfig; naming one is what asks for the
                  cross-check on the instance read from file
       BC-file:   BlockConfig applied to the instance [none]
       ref:       reference objective value to compare against [none]

`batches/batch-bds` repeats the three-form comparison on instances of
growing size, from 3 to 100 scenarios and from 24 to 96 time steps,
which is where the forms part ways: the generic one takes many more
iterations, each of which is one LP per scenario, while the ad hoc one
takes few, each of which is the whole stochastic problem. The
instances are written by `gen_investment.py`, which needs a Python
with `netCDF4`, and are thrown away at the end.

`batches/batch-bds-pypsa` runs the cross-check on the same stochastic
PyPSA instances `batches/batch-pypsa` solves, against the same
reference objective values.

A makefile is also provided that builds the executable including the
`TwoStageStochasticBlock`, `LagrangianDualSolver`, `BundleSolver`,
`MILPSolver` modules and the core SMS++ library, together with the
inner-Block module needed by the instances in `batches/` (currently
`UCBlock`).

## Configuration files

- `BSPar-BDS.txt` — `BlockSolverConfig` attaching
  `BendersDecompositionSolver` to the structured form, in the convex
  regime: the master is solved by the bundle `BDSMCfg.txt`
  names, whose `MasterProblemBlock` is configured by `MPBCfg-BDS.txt`,
  and each scenario subproblem by the `:MILPSolver` of
  `BDSSCfg.txt`, which is also what solves the extensive form
  the comparison is checked against.
- `BSPar-Inv.txt` — `BlockSolverConfig` of the ad hoc form, i.e., a
  bundle over the `InvestmentBlock`, and `InvBCfg.txt` the
  `BlockConfig` of that Block, which is what fixes the design in every
  scenario rather than mapping it into the right-hand side.
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
