# test/MasterProblemBlock

A tester of the `Solver` of a `MasterProblemBlock` on synthetic master
problems of a bundle method, which compares the `Solver` of `MPBSolvers.txt`
(the `GRBMILPSolver`, the `DAQPMILPSolver` and the `BDMinQuadSolver` of the
masters of [PolyhedralFunctionBlock](../PolyhedralFunctionBlock), those that
the build has) with the first one, in the status, in the value of the master
problem and in the step, while the master problem is changed as a bundle
method changes it (cuts added and removed, serious steps, t, the box, the
lower bounds, the level and the linear part); what is generated and what is
compared is described at the head of `test.cpp`.

The usage of the executable is the following:

       ./MasterProblemBlock_test -S MPBSolvers.txt [options]
       -N, --nvar <n>      variables [10]
       -K, --ncmp <n>      hard components [1]
       -M, --ncut <n>      initial cuts of each component [5]
       -V, --pvert <p>     probability that a cut is vertical [0]
       -L, --lb <0|1>      a lower bound on each component [0]
       -l, --level <0|1>   the doubly stabilized master [0]
       -x, --box <0|1|2>   box on no, half, all the variables [0]
       -b, --linear <0|1>  a linear part [0]

besides the options -e, -k, -n, -m and -q of the rounds of changes.

`batch` sweeps all these axes, with short cuts (4 variables, 20 cuts) and
long ones (100 variables, 5 cuts) and with one component or five, one seed
each, and is the test of the CI; `batch-long` runs the same sweep on larger
master problems (up to 1000 variables or 200 cuts), over 5 seeds and 50
rounds, to be run by hand.

A makefile is also provided that builds the executable including the
`MILPSolver` and `BDMinQuadSolver` modules (and, obviously, the core SMS++
library).

## Authors

- **Donato Meoli**  
  Dipartimento di Informatica  
  Università di Pisa

## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
