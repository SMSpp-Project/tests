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

Its batch is `batch-ref`. A batch file is provided that runs the test on a
largish set of
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

Its batches are `batch`, `batch-c`, `batch-k`, `batch-m` and `batchML`, over
the same `data/` set. Its makefile is `makefile-xcheck`. All of them but
`batch-k` use `BPar.txt`, i.e. the flow formulation with one `MCFBlock` per
commodity. The subproblems of the `LagrangianDualSolver`, i.e., the `MCFBlock`
of the commodities, are solved by a `:MILPSolver` (`BSPar.txt`), by
`MCFSolver< MCFSimplex >` (`BSPar-2S.txt`, when `MCFClassSolver` is in the
build) or by the network simplex of `MCFLemonSolver` (`BSPar-lemon.txt`). The
last one is in no battery: the Lagrangian costs are fractional, and LEMON
requires integer data, so that on `pN35` its network simplex does not
terminate. `batch-k` is the one exercising the knapsack
formulation, with one `BinaryKnapsackBlock` per arc, which it asks for in the
structure `Configuration` of the `BlockConfig` [see `Block::set_structure()`]
since the tree of sub-`Block` of a `MMCFBlock` is entirely a modelling
choice. It only
runs a handful of named instances, the knapsack formulation being much harder
to solve than the flow one.

All the tests passing confirms that no regressions have been done for the
tested modules, in particular for the used `Solver`.


## The benchmark of the machine-learning driven bundle

`MMCFBlock_ML_bench` trains and measures `BundleSolverML`, the bundle solver
that predicts the proximal parameter `t` with a small neural network, on the
instances of this Block. The harness is the generic one,
[`ml_bench.cpp`](../ml_bench.cpp), the same the suite of `UCBlock` builds on
its own instances; it is built only where Torch is, `BundleSolverML` being
built into `BundleSolver` only in that case, and it is run by hand, being a
measurement and not a check.

The instances are not in the repository, they are a separate download and have
to stay such: `<data-dir>` is where they are. The train, validation and test
splits are here instead, in [`splits-mnetgen`](splits-mnetgen), 172 instances
to train on, 21 to validate and 23 to test, since they are small and are what
makes a run reproducible.

Train a network on the training split and write the weights:

    MMCFBlock_ML_bench train <split> <data-dir> <block-cfg> <ml-cfg> \
                   -o <weights> [-e <epochs>]

Compare two solver configurations over the same split:

    MMCFBlock_ML_bench compare <split> <data-dir> <block-cfg> <cfg-A> <cfg-B>
\
                     [-r <weights>] [-o <results.csv>]

`-c <dir>` prefixes the configuration files, `-r <file>` gives the B side the
weights a training run wrote (without it, B runs untrained), and `-t <c>`
reads the instances in the text formats of this module ('m' for Mnetgen, 'p'
for JLF) instead of netCDF.

The two configurations that measure what the network is worth are
`BSPar-ML-K.txt`, the bundle with the machine-learning component, and
`BSPar-K.txt`, the same setup without it.


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
