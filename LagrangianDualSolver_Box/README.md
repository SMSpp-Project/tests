# test/LagrangianDualSolver_Box

A tester which provides very comprehensive tests for
`LagrangianDualSolver`, `LagBFunction`, `BoxSolver`, any `CDASolver` able to
handle `C05Function` in the `Objective`, any `CDASolver` able to handle Linear
Programs (such as `MILPSolver` and its derived classes `CPXMILPSolver`,
`SCIPMILPSolver` and `GRBMILPSolver`), as well as for quite a lot of the mechanics 
of the "core" SMS++ library.

This executable, given three input parameters n, k and m, constructs a
"very simple structured" `AbstractBlock` formed of k sub-`AbstractBlock`
with n variables each, only box constraints and separable `Objective`
(FRealObjective with a `LinearFunction` or `DQuadFunction`). m * n * k
linking constraints are constructed in the father, which has no `Variable`
and no `Objective` of its own. Two different Solver are registered to the
`AbstractBlock`, the second of which is assumed to be a
`LagrangianDualSolver` (which does not `BlockSolverConfig`-ure the
sub-`AbstractBlock` because the main directly registers `BoxSolver` to them),
whereas the second is any `CDASolver` able to handle Linear Programs. The
`AbstractBlock` is solved by both `Solver` and the results are compared.

The `AbstractBlock` is then repeatedly randomly modified and re-solved
several times, the results are compared.

The usage of the executable is the following:

    ./LDS_Box_test seed [wchg nvar nson dens #rounds #chng %chng]
       wchg: what to change, coded bit-wise [17]
             0 = bounds, 1 = objective
             2 = linking coefficients, 3 = linking lhs/rhs
       nvar: number of variables [10]
       nson: number of sub-Block [2]
       dens: number of constraints, fraction of nvar * nson [0.1]
       #rounds: how many iterations [40]
       #chng: number changes [10]
       %chng: probability of changing [0.5]

A batch file is provided that runs a largish (but typically terminating
within half an hour) set of tests with different sizes and seeds of the
random generator; all these passing is a good sign that no regressions
have been done for the tested modules.

A makefile is also provided that builds the executable including the
`LagrangianDualSolver` module, the `BundleSolver` module and all its
dependencies, in particular `MILPSolver`, together of course with the
core SMS++ library.


## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
