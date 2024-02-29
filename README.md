# SMS++ System Tests

A set of system tests for the SMS++ core library and several other
modules.

Since most of the tests we devised for the SMS++ project require multiple
modules, shipping them with a single module would add unnecessary requirements
to that module. For this reason, we ship them in a separate repository.

The following tests are provided:

- [`BendersBFunction`](BendersBFunction): a test of the `BendersBFunction`
  component on a "hand-made" `Block` for Capacitated Facility Location
  (CFL) problems.

- [`BinaryKnapsackBlock`](BinaryKnapsackBlock): a tester of the eponymous
  `Block` for (mixed-integer) binary knapsack problems and their specialised
  `Solver` (`DPBinaryKnapsackSolver`) against a standard `MILPSolver`.   

- [`BoxSolver`](BoxSolver), a tester which provides very
  comprehensive tests for `BoxSolver` (a very simple `CDASolver` for
  extremely simple problems where each `ColVariable` can
  be dealt with separately subject only to bound and integrality
  constraints and a linear or quadratic `Objective`, ignoring any other
  kind of `Constraint` if they are there) as well as to any `CDASolver`
  able to handle Linear Programs (such as `MILPSolver` and its derived
  classes `CPXMILPSolver` and `SCIPMILPSolver`), and for some of the
  mechanics of the SMS++ core library.

- [`CapacitatedFacilityLocation`](CapacitatedFacilityLocation), a tester
  that can be used to test several things together within a slope scaling
  approach to the Capacitated Facility Location (CFL) problem where the
  continuous relaxation can be solved with either standard LP tools (a
  `MILPSolver`), or via a Minc-Cost Flow relaxation casted as a `MCFBlock`
  and using custom `MCFSolver`, or, finally, via a Lagrange-friendly
  reformulation as a bunch of `BinaryKnapsackBlock`, so that a
  `LagrangianDualSolver` can be used to compute a stronger bound.

- [`compare_formulations`](compare_formulations),  very simple tester for
  testing different formulations of some problem obtained by
  `BlockConfig`-uring in two different ways two copies of the same `:Block`
  and solving them with two copies of the same `:Solver`.

- [`LagBFunction`](LagBFunction), a tester which provides very
  comprehensive tests for `LagBFunction`, `PolyhedralFunctionBlock`,
  `PolyhedralFunction`, any `CDASolver` able to handle `C05Function` in the
  objective (such as `BundleSolver`, for which some specific provisions are
  made), any `CDASolver` able to handle Linear Programs (such as `MILPSolver`
  and its derived classes `CPXMILPSolver` and `SCIPMILPSolver`), as well as
  for quite a lot of the mechanics of the SMS++ core library.

- [`LagrangianDualSolver_Box`](LagrangianDualSolver_Box), a tester
  which provides very comprehensive tests for `LagrangianDualSolver`,
  `LagBFunction`, `BoxSolver`, any `CDASolver` able to handle `C05Function`
  in the `Objective`, any `CDASolver` able to handle Linear Programs (such
  as `MILPSolver` and its derived classes `CPXMILPSolver` and
  `SCIPMILPSolver`), as well as for quite a lot of the mechanics of the
  SMS++ core library.

- [`LagrangianDualSolver_MMCF`](LagrangianDualSolver_MMCF),
  a tester which provides  initial tests for `LagrangianDualSolver`,
  `LagBFunction`, any `CDASolver` able to handle `C05Function` in the
  `Objective` (such as `BundleSolver`), any `CDASolver` able to handle
  Linear Programs (such as `MILPSolver` and its derived classes
  `CPXMILPSolver` and `SCIPMILPSolver`), `MMCFBlock` and `MCFBlock`,
  as well as for quite a lot of the mechanics of the SMS++ core library.

- [`LagrangianDualSolver_UC`](LagrangianDualSolver_UC), a tester
  which provides initial tests for `LagrangianDualSolver`, `LagBFunction`,
  any `CDASolver` able to handle `C05Function` in the `Objective` (such as
  `BundleSolver`), any `CDASolver` able to handle Linear Programs (such as
  `CPXMILPSolver` and `SCIPMILPSolver`), the `UCBlock` set of `Block`for
  Unit-Commitment problems, as well as for quite a lot of the mechanics
  of the SMS++ core library.

- [`LukFiBlock`](LukFiBlock): a very simple main for running tests with
  [LukFiBlock](https://gitlab.com/smspp/lukfiblock). It just creates one
  and loads it from a stream; little more than a compilation check.

- [`MCF_MILP`](MCF_MILP): solve a `MCFBlock` with both a `MILPSolver` and a
  `MCFSolver` and compare the results. This is a test for `MCFBlock`,
  `MCFSolver`, `MILPSolver` and its derived classes (`CPXMILPSolver` and
  `SCIPMILPSolver`), as well as for some of the mechanics of the SMS++
  core library.

- [`MMCFBlock`](MMCFBlock), a tester which provides initial tests
  for `MMCFBlock` (in particular, a way to retrieve/generate some sets of
  Multicommodity Min-Cost Flow instances) and any `Solver` able to handle
  Linear Programs (such as `MILPSolver` and its derived classes
  `CPXMILPSolver` and `SCIPMILPSolver`), as well as for a few of the
  mechanics of the SMS++ core library.

- [`PolyhedralFunction`](PolyhedralFunction), a tester which
  provides very comprehensive tests for `PolyhedralFunction` and some tests
  for any `CDASolver` able to handle `C05Function` in the objective (such as
  `BundleSolver`) and any `CDASolver` able to handle Linear Programs (such
  as `MILPSolver` and its derived classes `CPXMILPSolver` and
  `SCIPMILPSolver`), as well as for some of the mechanics of the SMS++
  core library.

- [`PolyhedralFunctionBlock`](PolyhedralFunctionBlock), a tester
  which provides very comprehensive tests for `PolyhedralFunction` and
  especially `PolyhedralFunctionBlock`, plus quited some tests for any
  `CDASolver` able to handle multiple `C05Function` in the objective (such
  as `BundleSolver`) and any `CDASolver` able to handle Linear Programs
  (such as `MILPSolver` and its derived classes `CPXMILPSolver` and
  `SCIPMILPSolver`), as well as for some of the mechanics of the SMS++
  core library.

- [`ThermalUnitBlock_Solver`](ThermalUnitBlock_Solver), a tester for the
  `ThermalUnitDPSolver` specialised Dynamic Programming `:Solver` for
  `ThermalUnitBlock` as compared with a `:MILPSolver` on some of the (many)
  different formulations supported by `ThermalUnitBlock`.

- [`Write-Read`](Write-Read), a tester for the function
  `AbstractBlock::read_mps` and some tests for any  `CDASolver` able 
  to handle Linear Programs (such as `MILPSolver` and its derived classes
  `CPXMILPSolver` , `SCIPMILPSolver` , `GRBMILPSolver` and
  `HiGHSMILPSolver`), as well as for some of the mechanics of the "core" 
  SMS++ library. A random MILP is constructed in an `AbstractBlock` and
  saved to a `.mps` file. A new `AbstractBlock` is created and read back
  to the file, two `:Solver` are attached to the two `AbstractBlock` and�
  the results are compared. The first `AbstractBlock` is randomly chamged
  many times and the process is repeated.

The tests run as traditional command line executables. Most of the tests
can also run as a
[CTest](https://cmake.org/cmake/help/latest/manual/ctest.1.html) suites.


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
make
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


## Getting help

If you need support, you want to submit bugs or propose a new feature, you can
[open a new issue](https://gitlab.com/smspp/tests/-/issues/new).


## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of
conduct, and the process for submitting merge requests to us.


## Authors

### Current Lead Authors

- **Enrico Calandrini**  
  Dipartimento di Informatica  
  Universita' di Pisa

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

- **Rafael Durbano Lobato**  
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
