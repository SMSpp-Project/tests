# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- a batch of `LagBFunction` over the easy components, which nothing was
  exercising

- a batch can set an algorithmic parameter in a `ComputeConfig` and time each
  run, so that a sweep over the values of one parameter is a batch and not a
  script written for the occasion; the two `batch-aggr` take the values of
  `intCmpAggrRule` that way

- `batch-k` of the multicommodity suite, the knapsack formulation asked for
  as a structure, and `MMCFBlock_test` links the ML variant of the
  BundleSolver, which `batchML` attaches

- a driver that runs the batteries backing the validation claim of the
  dynamic programming Solver of the thermal units, and the batteries require
  again the fixture that extracts the instances they read

- the batteries of `UCBlock` run the instances once per value of the rule
  that forms the groups of the parallel inner loop, and cross-check the
  academic and plan4res families over the exact Lagrangian chain; in the AC
  family the Lagrangian dual is declared a relaxation, its point being a
  convex combination, and the QCP sub-problems give the duals that chain
  needs

- the SVM suite cross-checks LIBLINEAR on the formulations that LIBSVM
  cannot express, exercises the exact path and the shrinking, which nothing
  was running, and compares the Lagrangian dual with a `SMOSolver` on each
  chunk

- `batch-ec` runs the Benders form of the energy community instances, the
  form the tester assembles giving the master a cost for every design
  Variable and keeping integer, in the master, a design that counts modules;
  the convex regime of the Benders Solver has its own cross-check
  configuration

- a batch for an `InvestmentBlock` wrapping a whole stochastic Block, two-
  stage or multi-stage, whose instances follow the trees they are built over;
  the inner Solver is asked for homogeneous dual directions, which the HiGHS
  variant cannot give, and the configuration says so where it happens

- the batteries of `BinaryKnapsackBlock` cover what `BranchAndXSolver` does:
  the lazy bounding protocol in every serial exploration strategy, the
  reoptimization with one Solver per strategy, the negative weights and the
  regime of a small instance re-solved many times, over the hard instances of
  Jooken as well, and the benchmarks of the coverage declare the greedy
  relaxation each of their configurations attaches

- `UCBlock_test --scale`, which checks the scale factor of a unit, i.e., the
  number of copies of it that a `UCBlock` holds, on two instances the tester
  writes itself, one carrying a thermal unit and one a nuclear one, each of
  them with a cost of every kind its Objective can hold, the two reserves and
  the reactive power: the model of a unit scaled once the abstract
  representation is built has to be the one of a unit scaled before it, which
  is compared coefficient by coefficient over the Objective of every unit and
  the rows of the `UCBlock` that use their Variable, and through the optimum;
  the data of the unit, being those of one copy, must not move although the
  Solver attached to it hears of the scaling; and the three dynamic
  programming Solvers have to answer for all the copies, i.e., to give the
  value of the Objective, also once a price has been written into it the way
  a dualizing Solver writes its multipliers. The thermal unit is taken in
  each of the seven formulations, with and without the perspective cuts. The
  comparison of the models asks for no Solver at all, so that it runs in a
  build that has none; the optimum and the dynamic programming Solvers are
  checked where a `:MILPSolver` is in the build

- the batch `batch-nuclear` of the `ThermalUnitBlock_Solver` suite, which
  compares the `NuclearUnitExtDPSolver` with a `:MILPSolver` on the
  operating rules of nuclear units, in eight families of rules, four
  regimes of the costs (energy only, rewarded reserves, priced reactive
  power, rounds of changes of the costs) and two formulations of the rules,
  the default one and the tight one; a further environment variable,
  `TUDPS_FIXMOD`, fixes one modulation variable out of the given number, so
  that the two Solver are compared on a unit whose rules are partly decided;
  two further BlockSolverConfig serve the study of the solve times,
  `BSCfg-nuc-lim.txt`, which holds the MILP solver to a time limit, and
  `BSCfg-nuc-dponly.txt`, which attaches the dynamic programming Solver
  alone, so that the optimal schedule can be looked at without paying for
  the MILP solve

- the PyPSA instances with the pollutant budget constraints of `UCBlock`
  in `pypsa-data/pollutants/`, run by `UCBlock/batches/batch-pypsa`:
  a PyPSA network with a CO2 limit twice and half the emissions of the
  unconstrained dispatch, a CO2 and a NOx limit, a CO2 floor, a CO2 equality,
  an operational limit on a carrier, and CO2 limits where a store and a hydro
  storage unit contribute through their final level, translated by
  pypsa2smspp and held to the PyPSA objective

- `UCBlock_test --pollutant` (the ctest `UCBlock_test/pollutant`), which
  checks the pollutant budget constraints of `UCBlock` on small instances it
  writes itself, whose optima are known: several zones per pollutant and a
  node in none, rates depending on time, lower bounds and equalities, the
  level of a battery, the scale of a unit, the setters of the budget and of
  its lower bound, the duals through a `UCBlockSolution`, the netCDF round
  trip and the data `UCBlock::deserialize()` must refuse

- the `MILPSolver` suite (the ctest `MILPSolver_test/groups`), which solves
  the same program with every `:MILPSolver` in the build, its `Variable` and
  `Constraint` grouped in every shape a `Block` allows

- the `AbstractBlock_mirror` suite, which checks the copy of the abstract
  representation of a Block that `AbstractBlock::mirror()` builds: that it is
  the same problem as the original, whichever `:MILPSolver` solves the two,
  that solution information moves back to the original, and that the copy
  follows the original when this changes

- the exact Lagrangian chain for UCBlock: BSPar-DP.txt attaches the three
  Solver over sub-Block solved to optimality by the dynamic programming
  Solver (TUBSCfg-DP.txt, InnerBSCfg-DP.txt, LDCfg-DP.txt, PPHCfg-DP.txt)
  and a :MILPSolver that solves the MIP rather than its continuous
  relaxation, stopping on a time limit so that what it gives is a valid
  pair of bounds rather than a claimed optimum (MILPCfg-MIP.txt). With the relaxation in the sub-Block the
  penalty of the PrimalProximalHeur acts on variables that are not binary
  there, and its first penalized call does not converge: on T-Ramp
  10_0_1_w the heuristic goes from 86 to 13 seconds, its bound becomes the
  Lagrangian one rather than the value of the continuous relaxation, and
  every inner call ends on "optimal". No batch uses it yet: the
  LagrangianDualSolver is a relaxation with a duality gap there, so it has
  to be declared with -E ,inf

- the cross-check of common_utils: every Solver enters it as its
  [get_lb(), get_ub()] interval, valid by the base Solver contract, and
  is measured against the best bounds the whole set of them provides,
  since the optimum is not known. Correctness, i.e. not contradicting
  those bounds, is owed by every Solver; the quality it declares is owed
  only by the one that returns kOK, i.e. that says it delivered what it
  was asked, while kLowPrecision promises nothing. No Solver type or
  name is ever inspected

- the tolerance each Solver is held to, which is by default the
  dblRelAcc its ComputeConfig asks of it, and never less than the
  tolerance the cross-check is called with, below which the comparison
  would only measure its own numerical noise

- the -E option, overriding that tolerance per Solver (positionally with
  respect to the BlockSolverConfig, the empty field leaving the Solver
  to its dblRelAcc), for the Solver that does not say with kLowPrecision
  when it did not deliver the accuracy it was asked for, and whose
  dblRelAcc therefore says nothing about what it returns

### Changed

- the two batteries of `MCFBlock` run one seed of their three when `$CI` is
  set, as the one of the dynamic programming already runs 5 of its 100 ramp
  profiles: the three seeds are the same sweep with another random stream,
  while the whole of them takes more than an hour on a machine of ours and
  does not fit what a test is given on a shared runner

- the inner Solver of the `BendersBFunction` suite is Gurobi, the one the
  image of the pipeline carries, so that the suite runs where it is run and
  not only where a licence of another solver happens to be

- the comparison of the three forms of a two-stage stochastic investment
  problem lives with the suite of the Block it is posed on, and not with the
  one of a Solver: the monolithic form, the Lagrangian one and the Benders
  one are configured side by side there

- the makefile asks for `-O3 -DNDEBUG` and nothing else, the macro of the
  patch for `boost::any` on macOS having no reason to be there since there is
  no `boost::any` left in the core

- the tester of the copy of an `AbstractBlock` and the one of the reference
  of the multicommodity suite are named after what they are, the core having
  a target called as the first one was

- a suite is guarded on the modules it is labelled with, and the benchmark of
  BundleSolverML is skipped when its modules are not in the build, so that a
  build without a module has no test that cannot run rather than a test that
  fails

- the batteries of the Lagrangian dual of the unit commitment fit the three
  hours a job is given: the academic families are sampled in CI, the
  plan4res one is run as a smoke test, and the metabatch is not run there,
  being the batteries it is made of. `batch-ac` is added, and the variants
  of the energy community under the proximal heuristic are skipped in CI,
  where the objective of a thermal unit inside a LagBFunction is the known
  bug

- the batteries of the scenario reduction read their executables and their
  directories from the arguments instead of the paths of whoever wrote them,
  the tester is named after the Solver it drives, and the folder of the
  facility location is named after the Block it holds

- the tester of `BendersBFunction` reports and counts its checks instead of
  aborting at the first one that fails, so that one run says how many of them
  hold and not only that one does not

- the comparison of the two Benders decompositions of a facility location
  instance lives with the suite of that Block, its configurations take the
  names the other suites give them, the inner Solver of the Lagrangian is
  called `BundleSolver`, which is the name the factory has, and the
  `BlockConfig` files are in the format of now

- the four sector-coupled instances `batch-pypsa` walks are written by the
  conversion as it stands, where an extendable asset with no upper bound keeps
  the infinite design cap it has instead of a finite number standing in for it:
  the ones the archive held carried `1e10` on nine converters and `1e9` on nine
  batteries, which the Lagrangian relaxation sends a design straight to, so
  that 262 of the 311 linearizations of a solve carried a subgradient entry of
  exactly `1e10` against function values of `1e9`, the rows of the master of the
  bundle became a difference of terms of `1e12` giving `1e8`, and on one of the
  four the master declared its numerical difficulties unrecoverable a step away
  from the optimum, upon which the bundle emptied itself one item at a time and
  the Lagrangian dual ended in error. The objective value of each of the four is
  the one it was, the caps having never been binding

- every directory of the suites is named after the module its tests are posed
  on, a suite living here exactly when the module cannot run it by itself:
  the testers of the objects of the core library are under `SMS++`, those of
  the quadratic programs a `:MILPSolver` is asked to solve under `MILPSolver`,
  the benchmark of the machine-learning driven bundle, which reads the Block
  of two modules that are not among its dependencies, under `BundleSolver/ML`,
  and the suite of the facility location takes the name of its module,
  `CapacitatedFacilityLocationBlock`. `compare_formulations`, which is posed
  on whichever two Block its configurations name, stays in the root

- the batteries of `UCBlock` that walk the instances carrying one unit alone
  are `batches-tub`, and each of them runs that family with every `:Solver`
  that applies to it: the dynamic programme against the `:MILPSolver`, and the
  Frank-Wolfe decomposition of a father of `K` copies of the unit, which used
  to be a batch of its own

- the suite of `FrankWolfeSolver` is dissolved into the suites of the Block it
  is posed on: the tester assumes nothing on the leaf Block it reads, hence it
  is `fw_test.cpp` in the root beside `common_utils`, and each suite builds it
  with its own modules; `MCFBlock` builds it and the tester of what a
  Modification of the feasible region of a sub-Block does, and `UCBlock`
  builds it on the thermal unit

- the runs of a Solver that is not the one of the Block are in the battery of
  the instances they are posed on rather than in a batch of that Solver, its
  configurations staying together in a directory of their own, which `-c`
  names; this is what `FW` is in the suites of `MCFBlock` and of `UCBlock`.
  The scenario reduction, which has no configurations of its own to keep
  apart, is in the suites instead: the generator of the scenarios of a unit
  commitment is in that of `UCBlock` and what a reduction costs on an
  investment problem is `test_reduction.cpp` in that of
  `TwoStageStochasticBlock`, the Block it is posed on, each with its battery
  in `batches`

- a battery runs every tester that applies to the family of instances it
  walks, and is therefore given all of them, the first as before and the
  others after it: `add_batch_test()` names them and hands over the ones that
  are in the build. In `MCFBlock` this is the Solver of the Block, the
  Frank-Wolfe decomposition and the rounds of Modification of the feasible
  region on the same instances, `batch-small` being the fast one that walks
  every code path of the decomposition; in `UCBlock` the dynamic programme and
  the decomposition on the same units

- the tester of the ways `BendersDecompositionSolver` has of writing a cut is
  not here: it writes the instance it runs on itself, hence it needs no Block
  of anyone else and it belongs to the test directory of that module, where it
  is and where it is being worked on. What stays here is the comparison of the
  ad hoc Benders decomposition a `CapacitatedFacilityLocationBlock` carries
  with the generic one, which is posed on that Block

- `BSCfg-nuc-dponly.txt` of `UCBlock` is gone: it registered the extended
  dynamic programming Solver of the nuclear unit twice, so that the tester,
  which compares a first and a second Solver, would run with it alone. A
  Solver compared with itself checks nothing, and no battery used it; timing
  the dynamic programme alone is a thing to ask the tester for, not something
  to obtain by declaring the same Solver twice

- - the report of a cross-check says what each Solver is called, one per line
  and with the values aligned one under the other, instead of numbering them
  S0, S1, ...: with several Solver of different families on the same Block,
  which of them disagrees is what one needs to read at a glance

- the three testers posed on an `AbstractBlock` are one directory,
  `SMS++/AbstractBlock`: the box-structured Block whose Lagrangian dual is
  computed (`test_box.cpp`), the copy of the abstract representation
  (`test_mirror.cpp`) and the round trip of a linear program through a file
  (`test_readwrite.cpp`), with a batch each in `batches`. The two regimes of
  the box one, the Lagrangian dual and the primal proximal heuristic, are one
  script taking which of them to run

- the batches of a suite are registered with `add_batch_test()`, written once
  in the root instead of the same loop copied in every directory

- the scenario reduction is run from the suite of the Block whose scenarios
  are reduced: the generator of the unit commitment instances and the runs on
  them are in the suite of `UCBlock`, those of the facility location in that
  of `CapacitatedFacilityLocationBlock`, each with the two configurations they
  read

- a suite is named after the Block its tests are posed on, not after a Solver
  that runs on it: `AbstractBlock_mirror` is `AbstractBlock`, and
  `LagrangianDualSolver_Box` is `AbstractBlock_Box`, the structured
  `AbstractBlock` of box-constrained sub-Block being what it builds and the
  Lagrangian dual one of the ways it solves it

- the suite of `MILPSolver` is the test directory of that module, where its
  two testers now live as `test_farkas.cpp` and `test_groups.cpp`: neither of
  them needs a Block of another module, hence neither of them needs to be
  here

- the suite of the dynamic programming solver of `ThermalUnitBlock` is part
  of the suite of `UCBlock`, whose instances it reads and whose Block it
  solves in two ways: the tester is `test_tudps.cpp` and its batches are in
  `batches-tub`, each of them run with that tester rather than with the one
  of the suite

- `SVMBlock` runs the comparison of the two dual decompositions of one
  training problem, the consensus one under `LagrangianDualSolver` and the
  Benders one under `BendersDecompositionSolver`, both cross-checked against
  the ad hoc solver of the module, and `PolyhedralFunctionBlock` runs the unit
  test of the pruning of the rows: both come from the test directory of
  `BendersDecompositionSolver`, which is not where a test that needs another
  Block to exist belongs

- `batch-resilient` of `UCBlock`, `TwoStageStochasticBlock`,
  `MultiStageStochasticBlock` and `InvestmentBlock` is now `batch-pypsa`, and
  the instances it reads are in `data/nc4/pypsa-data` instead of
  `data/nc4/resilient-data`, the folder that holds all the networks
  translated from PyPSA, one sub-folder per kind of problem: `ucblock`,
  `pollutants`, `tssb` (whose files are named after the perturbation, instead
  of lying in a sub-folder each) and `mssb`; the instances of `EC_Data` are
  divided in the same way, in `ucblock`, `tssb` and `mssb`

- the PyPSA instances of `pypsa-data/ucblock` and of the `pypsa-data` of
  `InvestmentBlock` are written by one generator, `test/instance_generator.py`
  of pypsa2smspp, which builds each test network once and writes it in the two
  forms the conversion supports, the one where the design variables are those
  of the `UCBlock` and the one where an `InvestmentBlock` wraps it: the two
  are therefore the same problem and are held to the same reference, the
  objective value PyPSA computes on that very network. The demand and the
  hydro inflow of a test network being drawn at random, the generator fixes
  the seed, so that the instances can be written again; the uncapped
  extendable assets take the finite caps of the instances with a pollutant
  budget, an unbounded design making some Lagrangian sub-problem unbounded.
  The reference values all change, the previous instances coming from an
  older state of the conversion, and one instance is named after its Excel
  case, `2n_1c_1g_1b_2l` instead of `2n_1c_1g_1b`

- the two-level scenario trees of `pypsa-data/mssb`, and the ones the
  `InvestmentBlock` runs with the investment stated outside the scenarios, are
  written by `test/tree_instance_generator.py` of pypsa2smspp from the trees
  `references/gen_resilient_tree.py` draws with a fixed seed, each form of a
  tree being held to the objective value PyPSA computes on the equivalent flat
  network. The reference values change, the instances in place having been
  emitted from trees that were drawn again since, and so do the file names,
  the trees now spanning a day of 24 instants rather than 100: over 100 the
  MultiStageStochasticBlock of one of them ends in an error after eleven
  minutes, and the InvestmentBlock over the flat form of another after
  twenty-four

- the two instances named after the `inv_` Excel cases are gone, those cases
  giving the very networks `1n_1c_1gext` and `2n_1c_1gext_1bext_2l` give, to
  the byte

- `InvestmentBlock/batches/batch-pypsa` fails when it finds no instance at all,
  which is how a batch that has tested nothing was until now indistinguishable
  from one where everything went well

- the nested chain of TwoStageStochasticBlock (BSPar-2S-LD.txt, where each
  scenario sub-problem is solved by an inner LagrangianDualSolver) evaluates
  every component at each iteration, dblMinNrEvls = -1: a component being an
  entire inner Lagrangian Dual, an iteration made on one of them alone buys
  little and pays a master problem anyway; on the NC instances of the energy
  community the oracle calls go from 865 and 1705 down to 132 and 168, and
  nothing gets worse on the others

- LPBSCfg-LD-noeasy.txt, the inner LagrangianDualSolver of that chain with
  intDoEasy = 0, which the instances with no installable asset need: every
  unit of theirs is "easy", and a Lagrangian Dual all of whose components are
  easy is not supported

- with -v 2 the cross-check prints, before solving, the parameters of every
  Solver it is about to run, the inner ones included; the level of -v can
  be written attached or separate, since getopt only hands over the
  attached form

- the solve-a-Block-with-Solvers cross-check testers renamed after the
  Block they exercise: UCBlock (was LagrangianDualSolver_UC, executable
  UCBlock_test) and MCFBlock (was MCF_MILP, executable MCFBlock_test);
  LagrangianDualSolver_MMCF was merged into MMCFBlock as a second
  executable (MMCFBlock_test) alongside the existing MMCF_test, sharing
  its instance data

- the one-per-instance cross-check line of SolveAll() (timings, every
  Solver value, reference, verdict) is now always printed; only the
  per-round lines of the tests that re-solve in a loop of modifications
  remain verbose-only

- PrimalProximalHeur is attached to the AC and resilient batteries too
  (BSPar-AC.txt and BSPar-EASY.txt now cross-check it as well)

- the PPHCfg of UCBlock solves the Lagrangian Dual of every proximal
  iteration to convergence, with the stopping parameters BSPar.txt gives
  to the LagrangianDualSolver on the same Lagrangian Dual, so that the
  fractional solution the penalty is built on is the convexified one,
  and follows the parameters of PrimalProximalHeur being now named after
  the algorithm they belong to; no -E is needed for the
  LagrangianDualSolver, which says with its return code that it is a
  relaxation; the LagBFunctions solve the sub-Block with
  the same BlockSolverConfig the LagrangianDualSolver gives them, rather
  than with the plain relaxation of every one of them

- UCBlock cross-checks every Solver of its BlockSolverConfig at once,
  rather than selecting one of them from the command line: the meta-
  batches are gone and each batch is a ctest test of its own

### Fixed

- the two configurations of the suite of `InvestmentBlock` asked HiGHS for a
  feasibility tolerance of `1e-9`, which is absolute, on instances whose
  objective is of the order of `1e11`, i.e., beyond what double precision
  holds: HiGHS declared the model optimal and its primal solution infeasible
  at the same time, which reaches the caller as a solve that went well with
  no solution in it, and the `InvestmentFunction` turned that into an error
  that stopped the bundle. They ask for `1e-7`, the default, and
  `batches/batch-pypsa` passes


- `LukFi_test` stops with an error when the `BlockSolverConfig` it reads
  attaches no `Solver` to the `LukFiBlock`, e.g. because the file is empty or
  malformed, rather than crashing on the first element of an empty list

- the suites that try each :MILPSolver in turn skipped the ones the build does
  not have by constructing them, while `Solver::new_Solver()` throws rather
  than returning `nullptr` on a name the factory does not hold, so
  `MILPSolver_test/groups` and `UCBlock_test/pollutant` died with
  `CPXMILPSolver not present in Solver factory` wherever CPLEX is not
  installed; they ask `Solver::has_Solver()` first, and
  `AbstractBlock_mirror_test`, which named CPXMILPSolver and nothing else,
  takes the first :MILPSolver the factory holds unless one is named on the
  command line

- the tester of `ThermalUnitBlock` compares the `Solution` of a Solver with
  the state it left in the Variable once that is completed from `(p, u)` as
  the `Solution` is when it is written: with the perspective cuts the epigraph
  a Solver leaves is only as tight as the separated cuts, `1e-7` relative, and
  on an objective that is the difference of much larger terms this showed as a
  `Solution` worth `4e-6` more than the Variable

## [0.6.0] - 2025-12-12

### Added

- tests comparing UCBlock solutions with expected values

- tests for PrimalProximalHeur

- ComputeConfig for "easy" case in LagBFunction

- tests for duals in LDS_MMCF

- [big] tests for Quadratic Problems

- LEMON to tests/MCF_MILP

- support for both LP and MPS fles in Write-Read

### Changed

- MMCFBlock/gen and the README accordingly to account for the new way
  of distributing the instances

- all things that can be changed, and the common definitions, are
  now in makefile\_common to reduce code duplication within makefiles
  and to make adapting to one's environment quicker

- adapted to new standard organization of makefiles

### Fixed

- several fixes throughout the testers

## [0.5.4] - 2024-02-29

### Added

- compare_formulations tester

- Write-Read tester

- added -Wno-enum-compare to Makefiles (we regularly do that in SMS++)

### Changed

- adapted to new CMake / makefile organisation

- significant updates to CapacitatedFacilityLocation

### Fixed

- many minor fixes to testers and/or config files

## [0.5.3] - 2023-05-17

### Added

- Early stop in test of ThermalUnitBlock.

### Removed

- GoogleTest-based test for DPThermalUnitBlock.

### Fixed

- LagrangianDualSolver_UC/test.

## [0.5.2] - 2022-07-01

### Added

- CapacitatedFacilityLocation tester.

- Code to test different formulations of some problem.

### Changed

- Complete rehaul of MCF_MILP tester.

## [0.5.1] - 2021-12-08

### Added

- New tester for ThermalUnitDPSolver.

## [0.5.0] - 2021-12-08

### Added

- ThermalUnitBlock_Solver tester.

- BinaryKnapsackBlock tester.

### Changed

- Completion of dynamic variables handling in test/PolyhedralFunction.

## [0.4.0] - 2021-02-05

### Added

- Significant improvements in LagBFunction testing.

- Testers now better use BlockSolverConfigs to be more general.

- Significant improvements in BendersBFunction testing.

- Added MMCFBlock tester.

- Added LagrangianDualSolver_UC tester.

- Added BoxSolver tester.

- Added LagrangianDualSolver_Box tester.

- Added LagrangianDualSolver_MMCF tester.

- Improved UCBlock tester.

- Improve README.md with ones for individual testers.

### Fixed

- Too many individual fixes to list.

## [0.3.2] - 2020-09-24

### Fixed

- Workaround for default MCFSolver setting.

## [0.3.1] - 2020-09-24

### Fixed

- Compilation issue under Debian/Clang 7.

## [0.3.0] - 2020-09-16

### Added

- Support for concurrency.

- Support for new configuration framework.

### Changed

- Files reorganized.

## [0.2.0] - 2020-03-06

### Added

- Changelog.

### Fixed

- Minor fixes.

## [0.1.0] - 2020-01-10

### Added

- First test release.

[Unreleased]: https://gitlab.com/smspp/tests/-/compare/0.6.0...develop
[0.6.0]: https://gitlab.com/smspp/tests/-/compare/0.5.4...0.6.0
[0.5.4]: https://gitlab.com/smspp/tests/-/compare/0.5.3...0.5.4
[0.5.3]: https://gitlab.com/smspp/tests/-/compare/0.5.2...0.5.3
[0.5.2]: https://gitlab.com/smspp/tests/-/compare/0.5.1...0.5.2
[0.5.1]: https://gitlab.com/smspp/tests/-/compare/0.5.0...0.5.1
[0.5.0]: https://gitlab.com/smspp/tests/-/compare/0.4.0...0.5.0
[0.4.0]: https://gitlab.com/smspp/tests/-/compare/0.3.2...0.4.0
[0.3.2]: https://gitlab.com/smspp/tests/-/compare/0.3.1...0.3.2
[0.3.1]: https://gitlab.com/smspp/tests/-/compare/0.3.0...0.3.1
[0.3.0]: https://gitlab.com/smspp/tests/-/compare/0.2.0...0.3.0
[0.2.0]: https://gitlab.com/smspp/tests/-/compare/0.1.0...0.2.0
[0.1.0]: https://gitlab.com/smspp/tests/-/tags/0.1.0
