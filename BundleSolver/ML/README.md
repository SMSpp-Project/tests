# BundleSolverML tests

Training and comparison harness for BundleSolverML, the bundle solver that
predicts the proximal parameter t with a small neural network.

## Data

The instances are not in the repository: they are a separate download and
have to stay such. Put them where the `<data-dir>` argument points to.

The train/validation/test splits used in the experiments are kept here,
since they are small and are what makes a run reproducible:

  * `MMCFBlock/splits-mnetgen/`  Mnetgen, 172/21/23
  * `UCBlock/splits/`            UCBlock, 33/4/5

## Build

    cmake --build . --target BundleSolverML_bench --config Release

Built only when Torch is found, since BundleSolverML itself is only built
into BundleSolver in that case.

## Run

Train a network on the training split and write the weights:

    BundleSolverML_bench train <split> <data-dir> <block-cfg> <ml-cfg> \
                               -o <weights> [-e <epochs>]

Compare two solver configurations over the same split:

    BundleSolverML_bench compare <split> <data-dir> <block-cfg> \
                                 <cfg-A> <cfg-B> [-r <weights>] \
                                 [-o <results.csv>]

Options:

    -c <dir>   prefix for the configuration files
    -t <c>     text instance format, as -t in tests/MMCFBlock
               ('m' for Mnetgen, 'p' for JLF); omit for netCDF
    -r <file>  in compare: weights for the B side, written by a training
               run; without it B runs untrained

## Configurations

  * `MMCFBlock/BSPar-ML-K.txt`  BundleSolver with the ML component
  * `MMCFBlock/BSPar-K.txt`     the same setup without it

Passing these two as `<cfg-A>` and `<cfg-B>` is how the effect of the ML
component is measured.
