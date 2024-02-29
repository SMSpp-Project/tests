# test/MMCFBlock

A tester which provides initial tests for `MMCFBlock` and any `Solver`
able to handle Linear Programs (such as `MILPSolver` and its derived
classes `CPXMILPSolver`, `SCIPMILPSolver` and `GRBMILPSolver`), as 
well as for a few of the mechanics of the "core" SMS++ library.

This executable, given the filename and (optionally) filetype of one
Multicommodity Min-Cost Flow (MMCF) in one of the several supported file
formats, reads the instance in a `MMCFBlock` and solves it with a
`:MILPSolver` (or whatever appropriate solver the `BlockSolverConfig`
described by `BSPar.txt` dictates). It then loads the same problem with
the entirely different solver `MMCFCplex` and again solves it, comparing
the results (and printing the running time).

The usage of the executable is the following:

        ./MMCF_test file_name [typ]
        typ = s*, c, p, o, d, u, m (lower or uppercase)

A batch file is provided that runs the test on a largish set of
MMCF instances (but not very large ones, so that the tests does end
in reasonable time). These instances are supposed to be in the `data/`
folder, but only a small subset of them is there in the repo (properly
gzipped). The `gen/` folder contains a `genbatch` which curls some other
sets of instances from

http://groups.di.unipi.it/optimize/Data/MMCF.html

and generates another set with the included Mnetgen random generator
(also available at that page with instructions for generating even
larger ones if required).

All the tests passing confirms that `MMCFBlock` correctly loads the
MMCF instances from file, and that no regressions have been done for
the tested modules, in particular for the used `CDASolver`.


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
