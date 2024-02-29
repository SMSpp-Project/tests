# test/LagrangianDualSolver_MMCF

A tester which provides initial tests for `LagrangianDualSolver`,
`LagBFunction`, any `CDASolver` able to handle `C05Function` in the
`Objective` (such as `BundleSolver`), any `CDASolver` able to handle
Linear Programs (such as `MILPSolver` and its derived classes
`CPXMILPSolver`, `SCIPMILPSolver` and `GRBMILPSolver`), `MMCFBlock` 
and `MCFBlock`, as well as for quite a lot of the mechanics of the 
"core" SMS++ library.

This executable, given the filename and (optionally) filetype of one
Multicommodity Min-Cost Flow (MMCF) in one of the several supported file
formats, reads the instance in a `MMCFBlock` and solves it with the two
`Solver` specified by the `BlockSolverConfig` described by `BSPar.txt`,
thought to be a `:MILPSolver` and a `LagrangianDualSolver`, comparing
the results (and printing the running time).

The usage of the executable is the following:

       ./LDS_MMCF_test file_name [typ]
        typ = s*, c, p, o, d, u, m (lower or uppercase)

A batch file is provided that runs the test on a largish set of
MMCF instances (but not very large ones, so that the tests does end
in reasonable time) taken among those of

http://groups.di.unipi.it/optimize/Data/MMCF.html

These instances are supposed to be in the `data/` folder, the idea being
that it is a symlink (or copy) of that of the
[`MMCFBlock`](--//MMCFBlock/README.md) tester, as obtained by

    ln -s ../MMCFBlock/data 

See the original repo for instructions about how to generate/download the
instances.

A makefile is also provided that builds the executable including the
`LagrangianDualSolver` module, the `BundleSolver` module and all its
dependencies, in particular `MILPSolver` together of course with the
core SMS++ library, and the `MMCFBlock` module with its dependency,
the `MCFBlock` module.

All the tests passing confirms that no regressions have been done for
the tested modules, in particular for the used `Solver`.


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
