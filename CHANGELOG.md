# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added 

### Changed 

### Fixed 

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

[Unreleased]: https://gitlab.com/smspp/tests/-/compare/0.5.4...develop
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
