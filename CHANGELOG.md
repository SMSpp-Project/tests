# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added 

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
  the algorithm they belong to; the LagBFunctions solve the sub-Block with
  the same BlockSolverConfig the LagrangianDualSolver gives them, rather
  than with the plain relaxation of every one of them

- UCBlock cross-checks every Solver of its BlockSolverConfig at once,
  rather than selecting one of them from the command line: the meta-
  batches are gone and each batch is a ctest test of its own

### Fixed 

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

### Fixed

- LagrangianDualSolver_UC/test.

### Removed

- GoogleTest-based test for DPThermalUnitBlock.

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
