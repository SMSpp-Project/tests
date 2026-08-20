# test/MMCFBlock

This directory hosts two testers for `MMCFBlock`, sharing the same instance
data (retrieved / generated as described below):

- `MMCF_test`, which loads an instance in a `MMCFBlock`, solves it and
  cross-checks the result against the entirely different `MMCFCplex` solver;
- `MMCFBlock_test`, which loads the same instance and cross-checks every
  `Solver` of its `BlockSolverConfig` (a `:MILPSolver` and a
  `LagrangianDualSolver`) against each other.

## `MMCF_test` (vs the `MMCFCplex` reference)

A tester which provides initial tests for `MMCFBlock` and any `Solver`
able to handle Linear Programs (such as `MILPSolver` and its derived
classes `CPXMILPSolver`, `SCIPMILPSolver` and `GRBMILPSolver`), as 
well as for a few of the mechanics of the "core" SMS++ library.

This executable, given the filename and (optionally) filetype of one
Multicommodity Min-Cost Flow (MMCF) in one of the several supported file
formats, reads the instance in a `MMCFBlock` and solves it with a
`:MILPSolver` (or whatever appropriate solver the `BlockSolverConfig`
described by `BSPar-ref.txt` dictates). It then loads the same problem with
the entirely different solver `MMCFCplex` and again solves it, comparing
the results (and printing the running time).

The usage of the executable is the following:

        ./MMCF_test file_name [typ]
        typ = s*, c, p, o, d, u, m (lower or uppercase)

Its batch is `batch-ref`. A batch file is provided that runs the test on a largish set of
MMCF instances (but not very large ones, so that the tests does end
in reasonable time). These instances are supposed to be in the `data/`
folder, but they need to be downloaded / generated before. The `gen/`
folder contains a `genbatch` which curls the instances from the
[COMMALAB site](https://commalab.di.unipi.it/datasets/mmcf) and
generates another set with the included Mnetgen random generator
(also available at that page with instructions for generating even
larger ones if required). The download (but not the generation) is
also automatically done when installing the repo with CMake.

All the tests passing confirms that `MMCFBlock` correctly loads the
MMCF instances from file, and that no regressions have been done for
the tested modules, in particular for the used `CDASolver`.

## `MMCFBlock_test` (solver cross-check)

A tester which provides initial tests for `LagrangianDualSolver`,
`LagBFunction`, any `CDASolver` able to handle `C05Function` in the
`Objective` (such as `BundleSolver`), any `CDASolver` able to handle
Linear Programs (such as `MILPSolver` and its derived classes
`CPXMILPSolver`, `SCIPMILPSolver` and `GRBMILPSolver`), `MMCFBlock`
and `MCFBlock`, as well as for quite a lot of the mechanics of the
"core" SMS++ library.

This executable, given the filename and (optionally) filetype of one MMCF
instance, reads it in a `MMCFBlock` and solves it with the two `Solver`
specified by the `BlockSolverConfig` described by `BSPar.txt`, thought to be
a `:MILPSolver` and a `LagrangianDualSolver`, comparing the results (and
printing the running time).

        ./MMCFBlock_test file_name [typ]
        typ = s*, c, p, o, d, u, m (lower or uppercase)

Its batches are `batch`, `batch-c`, `batch-m` and `batchML`, over the same
`data/` set. Its makefile is `makefile-xcheck`.

All the tests passing confirms that no regressions have been done for the
tested modules, in particular for the used `Solver`.


## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

- **Enrico Gorgone**  
  Dipartimento di Matematica ed Informatica  
  Università di Cagliari


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
