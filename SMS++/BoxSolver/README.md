# test/BoxSolver

A tester which provides very comprehensive tests for `BoxSolver`, a
`CDASolver` for extremely simple problems where each `ColVariable` can
be dealt with separately subject only to bound and integrality
constraints and a linear or quadratic `Objective`. In fact, is there are
any other kind of `Constraint` in the `Block`, `BoxSolver` plainly
ignores them and solves the corresponding separable relaxation. However,
an interesting feature of `BoxSolver` is that it at the same time
minimizes and maximizes the `Objective`. The tester also provides some
tests for any `CDASolver` able to handle Linear Programs (such as
`MILPSolver` and its derived classes `CPXMILPSolver` ,
`SCIPMILPSolver` and `GRBMILPSolver`), as well as for some of the mechanics
of the "core" SMS++ library.

Given the number of variables, a single random "box-only" `AbstractBlock`
with separable `Objective` (a `FRealObjective` with either a
`LinearFunction` or a `DQuadFunction` inside) is constructed and solved
with a `BoxSolver`. The `AbstractBlock` has the following properties,
each with fifty-fifty chances:

- a min or max `Objective`;

- a fully linear `Objective` or one with also quadratic coefficients,
  in which case the `Objective` is not necessarily convex for a min
  problem or concave for a max one, as `BoxSolver` can deal with it;

- all continuous variables or some integer ones; however, when there
  are integer variables and `DIRECTION_TEST = 0` (see below) the
  `Objective` is selected to be convex for a min problem and concave
  for a max one, as a  `MILPSolver` may argue otherwise);

- guaranteed to be always feasible or not;

- guaranteed to be always bounded or not.

According to the value of the macro `DIRECTION_TEST`, we then:

- either  compare the `get_opposite_value()` with the `get_var_value()`
  obtained by reversing the sign of the `Objective` (that must be equal);

- or compare with the results of an appropriate `Solver` (e.g.,
a `:MILPSolver`.

The `AbstractBlock` is then repeatedly randomly modified (on bounds and
objective, as there is nothing else) and re-solved several times,
the results are compared.

The usage of the executable is the following:

       ./BoxSolver_test seed [wchg nvar #rounds #chng %chng]
       wchg: what to change, coded bit-wise [3]
             0 = bounds, 1 = objective 
       nvar: number of variables [10]
       #rounds: how many iterations [100]
       #chng: number changes [10]
       %chng: probability of changing [0.6]

A batch file is provided that runs a large set of tests (which are anyway
very quick, in particular if `DIRECTION_TEST > 0`)  with different sizes
and seeds of the random generator; all these passing is a good sign that
no regressions have been done for the tested modules, and in particular
for `BoxSolver`.

A makefile is also provided that builds the executable including the
`MILPSolver` module and all its dependencies (hence, obviously, the core
SMS++ library), although the latter could be switched away when testing
with `DIRECTION_TEST > 0`.

## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
