# SMS++ System Tests

A set of system tests for the SMS++ core library and several other
modules.

Since most of the tests we devised for the SMS++ project require multiple
modules, shipping them with a single module would add unnecessary requirements
to that module. For this reason, we ship them in a separate repository.

The following tests are provided:

Each directory is named after the module the tests inside it are posed on,
since a suite lives here when the module cannot run it by itself: either it
needs the cross-check machinery of [`common_utils`](common_utils.h), which
solves a Block with every `:Solver` a `BlockSolverConfig` attaches and
compares what they return, or it needs a module that is not among that
module's dependencies. The testers of the objects of the core library are
therefore grouped under [`SMS++`](SMS++), and those posed on a Block under the
name of its module; the unit tests of a module, which need neither, live in
the `test` directory of the module itself.

### The core library ([`SMS++`](SMS++))

- [`AbstractBlock`](SMS++/AbstractBlock), the three testers posed on an
  `AbstractBlock`: the box-structured Block of `k` sub-`AbstractBlock` with box
  constraints and a separable `Objective` whose Lagrangian dual is computed by
  a `LagrangianDualSolver` (with `LagBFunction` and `BoxSolver`) and
  cross-checked against a `:MILPSolver`; `AbstractBlock::mirror()`, i.e., the
  copy of the abstract representation that every Block has without a line
  written for it, checked to be the same problem as the original, to take the
  solution back to it and to follow it when it changes; and
  `AbstractBlock::read_lp()` / `AbstractBlock::read_mps()`, a random linear
  program being written to file by the `:MILPSolver` attached to it, read back
  into a second `AbstractBlock` and solved again, the two optima having to
  agree. All three then change the instance at random and re-solve many
  times.

- [`BoxSolver`](SMS++/BoxSolver), a tester which provides very
  comprehensive tests for `BoxSolver` (a very simple `CDASolver` for
  extremely simple problems where each `ColVariable` can
  be dealt with separately subject only to bound and integrality
  constraints and a linear or quadratic `Objective`, ignoring any other
  kind of `Constraint` if they are there) as well as to any `CDASolver`
  able to handle Linear Programs (such as `MILPSolver` and its derived
  classes `CPXMILPSolver` and `SCIPMILPSolver`), and for some of the
  mechanics of the SMS++ core library.

- [`LagBFunction`](SMS++/LagBFunction), a tester which provides very
  comprehensive tests for `LagBFunction`, `PolyhedralFunctionBlock`,
  `PolyhedralFunction`, any `CDASolver` able to handle `C05Function` in the
  objective (such as `BundleSolver`, for which some specific provisions are
  made), any `CDASolver` able to handle Linear Programs (such as `MILPSolver`
  and its derived classes `CPXMILPSolver` and `SCIPMILPSolver`), as well as
  for quite a lot of the mechanics of the SMS++ core library.

- [`BendersBFunction`](SMS++/BendersBFunction): a test of the `BendersBFunction`
  component on a "hand-made" `Block` for Capacitated Facility Location
  (CFL) problems.

- [`PolyhedralFunction`](SMS++/PolyhedralFunction), a tester which
  provides very comprehensive tests for `PolyhedralFunction` and some tests
  for any `CDASolver` able to handle `C05Function` in the objective (such as
  `BundleSolver`) and any `CDASolver` able to handle Linear Programs (such
  as `MILPSolver` and its derived classes `CPXMILPSolver` and
  `SCIPMILPSolver`), as well as for some of the mechanics of the SMS++
  core library.

- [`PolyhedralFunctionBlock`](SMS++/PolyhedralFunctionBlock), a tester
  which provides very comprehensive tests for `PolyhedralFunction` and
  especially `PolyhedralFunctionBlock`, plus quite a few tests for any
  `CDASolver` able to handle multiple `C05Function` in the objective (such
  as `BundleSolver`) and any `CDASolver` able to handle Linear Programs
  (such as `MILPSolver` and its derived classes `CPXMILPSolver` and
  `SCIPMILPSolver`), as well as for some of the mechanics of the SMS++
  core library.

### The Solver

- [`QuadFunction`](MILPSolver/QuadFunction), a tester which provides very
  comprehensive tests for any `CDASolver` able to handle Quadratic Programs
  (such as `MILPSolver` and its derived classes `CPXMILPSolver` ,
  `SCIPMILPSolver` , `GRBMILPSolver` and `HiGHSMILPSolver`).

- [`BundleSolver/ML`](BundleSolver/ML), the benchmark of the machine-learning
  driven `BundleSolver` against the plain one, over a split of the instances
  of `MMCFBlock` and of `UCBlock`: it is here, and not in the `test` of its
  module, because those two Block are not among the dependencies of
  `BundleSolver`.

### The Block

- [`BinaryKnapsackBlock`](BinaryKnapsackBlock): a tester of the eponymous
  `Block` for (mixed-integer) binary knapsack problems that cross-checks all
  its equivalent `Solver` (the core DP, the `BranchAndXSolver` in each
  exploration mode, with the greedy relaxation bracketing) against a standard
  `MILPSolver`, both on random instances and against the published optima of
  the curated Pisinger benchmark. `batch-pisinger` also runs the generic
  Frank-Wolfe tester on `K` copies of a knapsack, each with its dynamic
  programme as the Linear Minimization Oracle: no compact formulation
  describes the convex hull of a knapsack, so what the decomposition computes
  is a lower bound on the monolithic optimum and not the same number, which is
  what that battery checks.

- [`CapacitatedFacilityLocationBlock`](CapacitatedFacilityLocationBlock), a tester
  that can be used to test several things together within a slope scaling
  approach to the Capacitated Facility Location (CFL) problem where the
  continuous relaxation can be solved with either standard LP tools (a
  `MILPSolver`), or via a Min-Cost Flow relaxation cast as a `MCFBlock`
  and using custom `MCFSolver`, or, finally, via a Lagrange-friendly
  reformulation as a bunch of `BinaryKnapsackBlock`, so that a
  `LagrangianDualSolver` can be used to compute a stronger bound, and a
  second one that puts the ad hoc Benders decomposition the Block carries
  against the generic one of `BendersDecompositionSolver` on the same
  instance.

- [`MCFBlock`](MCFBlock): solve a `MCFBlock` with both a `MILPSolver` and a
  `MCFSolver` and compare the results. This is a test for `MCFBlock`,
  `MCFSolver`, `MILPSolver` and its derived classes (`CPXMILPSolver` and
  `SCIPMILPSolver`), as well as for some of the mechanics of the SMS++
  core library. The same suite hosts the generic Frank-Wolfe
  tester (`fw_test.cpp`): a "leaf" `Block` is read `K` times into a father
  `AbstractBlock` with a random `FRealObjective`, which is then solved both by
  a `FrankWolfeSolver` (using the `:Solver` registered to each sub-`Block` as a
  Linear Minimization Oracle) and by a monolithic `:MILPSolver`, cross-checking
  the two optima; here the leaves are `MCFBlock` and their oracle a
  `MCFSolver`, and a second tester runs the same comparison while the feasible
  region of a sub-`Block` changes. Their configurations are the `Father*` ones
  of the suite, flat beside those of every other `:Solver`, and their runs are
  in the batteries of the instances they are posed on, `batch-small` being the
  fast one, which walks every code path of the decomposition on the small
  instances of `MCFClassSolver`. A run attaches the reference and one
  `FrankWolfeSolver` per variant of the decomposition at once, and cross-checks
  the variants against one another as well.

- [`MMCFBlock`](MMCFBlock),
  a tester which provides  initial tests for `LagrangianDualSolver`,
  `LagBFunction`, any `CDASolver` able to handle `C05Function` in the
  `Objective` (such as `BundleSolver`), any `CDASolver` able to handle
  Linear Programs (such as `MILPSolver` and its derived classes
  `CPXMILPSolver` and `SCIPMILPSolver`), `MMCFBlock` and `MCFBlock`,
  as well as for quite a lot of the mechanics of the SMS++ core library. The
  suite holds a second tester, which provides initial tests for `MMCFBlock`
  (in particular, a way to retrieve/generate some sets of Multicommodity
  Min-Cost Flow instances) and any `Solver` able to handle Linear Programs,
  as well as for a few of the mechanics of the SMS++ core library.

- [`UCBlock`](UCBlock), a tester
  which provides initial tests for `LagrangianDualSolver`, `LagBFunction`,
  any `CDASolver` able to handle `C05Function` in the `Objective` (such as
  `BundleSolver`), any `CDASolver` able to handle Linear Programs (such as
  `CPXMILPSolver` and `SCIPMILPSolver`), the `UCBlock` set of `Block` for
  Unit-Commitment problems (including the pollutant budget constraints, both
  against PyPSA and on small instances with known optima), as well as for
  quite a lot of the mechanics of the SMS++ core library. The same suite runs
  `TUDPS_test`, which compares the `ThermalUnitExtDPSolver` specialised
  Dynamic Programming `:Solver` with a `:MILPSolver` on some of the (many)
  different formulations `ThermalUnitBlock` supports; its batches are in
  `batches-tub`, the batteries that walk the instances carrying one unit
  alone, thermal or nuclear. Each of them runs that family with every
  `:Solver` that applies to it: besides the dynamic programme against the
  `:MILPSolver`, the generic Frank-Wolfe tester (`fw_test.cpp`) on `K` copies
  of the unit, each with its Dynamic Programming `:Solver` as the Linear
  Minimization Oracle, against the perspective bound a `:MILPSolver` computes
  on the monolithic relaxation, with the same set of configurations, named the
  same way, as the other two suites this tester is built in. The same suite
  holds the generator of the scenarios of a unit commitment and the battery
  that reduces them, `batches/batch-scenred`, which runs it together with the
  tester of `ScenarioReductionSolver`.

- [`InvestmentBlock`](InvestmentBlock), a tester that solves the investment
  problem defined by an `InvestmentBlock` (loaded from a netCDF file) with the
  configured `:Solver`.

- [`TwoStageStochasticBlock`](TwoStageStochasticBlock), a tester that loads a
  `TwoStageStochasticBlock` from a netCDF file, attaches one or two `:Solver`
  through a `BlockSolverConfig` and compares their results, and a second one
  that puts the three ways of solving the same two-stage stochastic investment
  problem one against the other, i.e., the extensive form, the generic Benders
  decomposition of `BendersDecompositionSolver` and the ad hoc one an
  `InvestmentBlock` over the whole `TwoStageStochasticBlock` carries, on
  instances of growing size. A third one measures what a scenario
  reduction costs: the instance is solved on the whole scenario set and on the
  `K` representatives each method of `ScenarioReductionSolver` picks, and the
  first-stage decision the reduced problem finds is put back into the whole
  set, so that what is reported is both the gap of the reduced problem and the
  implementation error of its decision.

- [`MultiStageStochasticBlock`](MultiStageStochasticBlock), a tester that loads
  a `MultiStageStochasticBlock` from a netCDF file, attaches a `:Solver`
  through a `BlockSolverConfig` and compares its result against a reference
  objective value.

- [`LukFiBlock`](LukFiBlock): a very simple main for running tests with
  [LukFiBlock](https://gitlab.com/smspp/lukfiblock). It just creates one
  and loads it from a stream; little more than a compilation check.

- [`SVMBlock`](SVMBlock), a tester that cross-checks every `:Solver` that can
  train a Support Vector Machine on the same `SVMBlock`: the ad hoc
  `SMOSolver`, `LIBSVMSolver`, a `:MILPSolver` on either formulation the
  abstract representation can encode, and `LagrangianDualSolver` on the
  consensus structure, whose chunks it relaxes into one independent SVM each.
  It also changes the training problem under the `Solver` and checks that they
  keep agreeing after each change.

- [`SingleFlowDCRBlock`](SingleFlowDCRBlock), a tester of the eponymous
  `Block` for single-flow Delay-Constrained Routing problems: a random
  instance is built around a source-sink path whose delay the deadline is
  set from, so that the instance is always feasible and the deadline as
  tight as one wants it, and it is then solved by every `Solver` the
  `BlockSolverConfig` registers, typically a `:MILPSolver` on either of the
  two formulations of the problem and the `SingleFlowDCRBendersSolver`,
  cross-checking what they answer and the `Solution` each of them produces.

### Posed on no Block in particular

- [`compare_formulations`](compare_formulations),  very simple tester for
  testing different formulations of some problem obtained by
  `BlockConfig`-uring in two different ways two copies of the same `:Block`
  and solving them with two copies of the same `:Solver`.

The tests run as traditional command line executables. Most of the tests
can also run as a
[CTest](https://cmake.org/cmake/help/latest/manual/ctest.1.html) suite.


## Getting started

These instructions will let you build and run the SMS++ System Tests
on your system.

### Requirements

- See each test for its requirements.

### Build with CMake

Configure and build all the tests using CMake:

```sh
mkdir build
cd build
cmake ..
cmake --build .
```

### Build and install with makefiles

Carefully hand-crafted makefiles have also been developed for those unwilling
to use CMake. Makefiles build the executable in-source (in the same directory
tree where the code is) as opposed to out-of-source (in the copy of the
directory tree constructed in the build/ folder) and therefore it is more
convenient when having to recompile often, such as when developing/debugging
a new module, as opposed to the compile-and-forget usage envisioned by CMake.

Each of the executables in the individual folders has its own makefile which
includes the "main makefile" of the concerned modules, typically either
`makefile-c` including all necessary libraries comprised the "core SMS++" one,
or `makefile-s` including all necessary libraries but not the "core SMS++"
one (for the common case in which this is used together with other modules
that already include them). The makefiles in turn recursively include all the
required other makefiles, hence one should only need to edit the makefile
of each executable for compilation type (C++ compiler and its options) and it
all should be good to go. In case some of the external libraries are not at
their default location, it should only be necessary to create the
`../extlib/makefile-paths` out of the `extlib/makefile-default-paths-*` for
your OS `*` and edit the relevant bits (commenting out all the rest).

Check the [SMS++ installation wiki](https://gitlab.com/smspp/smspp-project/-/wikis/Customize-the-configuration#location-of-required-libraries)
for further details.


## Usage

Each tester has an executable built in the corresponding directory (or in the
corresponding directory in the copy of the directory tree in the build/ folder
if you use CMake); look at the `README.md` in the folder and/or run it for
instructions. In several cases a (bash) batch is available to run
a default sequence of tests (this may take a while).

In case you use CMake, you can see all the (bash) batch tests available by
running:

```sh
ctest -N
```

and run them all at one with:

```shell
ctest -V -C Release
```

or you can choose a specific one from the batch test list and run it with:

```sh
ctest -V -R <batch-test-name> -C Release
```

Each test is also tagged, via CTest labels, with the modules it exercises, so
you can run all and only the tests relevant to one module with:

```sh
ctest -V -C Release -L <module>
```

This is what each module's continuous integration uses to run its own tests
(and only those) without referring to any test path. The map from each test to
its modules is kept in a single place, [`cmake/TestLabels.cmake`](cmake/TestLabels.cmake);
extend it when adding a test.

## Getting help

If you need support, you want to submit bugs or propose a new feature, you can
[open a new issue](https://gitlab.com/smspp/tests/-/issues/new).


## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of
conduct, and the process for submitting merge requests to us.


## Authors

### Current Lead Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

- **Enrico Calandrini**  
  Dipartimento di Informatica  
  Universita' di Pisa

- **Rafael Durbano Lobato**  
  Dipartimento di Informatica  
  Università di Pisa

- **Donato Meoli**  
  Dipartimento di Informatica  
  Università di Pisa

### Contributors

- **Federica Di Pasquale**  
  Dipartimento di Informatica  
  Università di Pisa

- **Ali Ghezelsoflu**  
  Dipartimento di Informatica  
  Università di Pisa

- **Enrico Gorgone**  
  Dipartimento di Matematica ed Informatica  
  Università di Cagliari

- **Niccolò Iardella**  
  Dipartimento di Informatica  
  Università di Pisa


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.


## Disclaimer

The code is currently provided free of charge under an open-source license.
As such, it is provided "*as is*", without any explicit or implicit warranty
that it will properly behave or it will suit your needs. The Authors of
the code cannot be considered liable, either directly or indirectly, for
any damage or loss that anybody could suffer for having used it. More
details about the non-warranty attached to this code are available in the
license description file.
