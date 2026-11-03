# tests/MILPSolver/ConvexMIQP

The tester `MILPSolver_ConvexMIQP_test` generates a strictly convex
Mixed-Integer Quadratic Program from a seed, builds it as an `AbstractBlock`
(a `QuadFunction` in a `FRealObjective`, a `LinearFunction` in each
`FRowConstraint`, the bounds as `BoxConstraint`), registers to it the
`Solver` of a `BlockSolverConfig` and cross-checks them with `SolveAll()`;
the solution that each of them writes in the `ColVariable` is then checked
on the data, i.e., it has to satisfy the rows, the bounds and the
integrality, and its value has to be the one the `Solver` declares. The
number of nodes of each `:MILPSolver` is printed after the cross-check.

The instances are those of section 4 of "A Feasible Active Set Method with
Reoptimization for Convex Quadratic Mixed-Integer Programming" (SIAM J.
Optim. 26(3), 2016): the objective is `x' Q x + c' x`, with `Q` the sum of
`l_i v_i v_i'` over `n` eigenvalues `l_i` uniform in `[0, 1]` and as many
vectors `v_i` orthonormalized out of entries uniform in `[-1, 1]`, `c`
uniform in `[-1, 1]`, the first `n1 = p n` variables integer, and `m` rows
`A x <= b` either of type (a), `A` and `b` uniform in `[-1, 1]`, or of type
(b), `A` uniform in `[0, 1]` and `b` half the sum of each row. The options
add what the paper does not have: equality and ranged rows (`-q`, `-w`),
bounds on the integer variables (`-u`, or binary ones with `-b`) and on the
continuous ones (`-o`), a maximization of the negated objective (`-x`), an
infeasible relaxation (`-z 1`) and a feasible relaxation with no integer
solution (`-z 2`, which needs bounded integer variables); `-h` lists them
all.

`BSPar.txt` attaches the `FASTQPAMILPSolver` of the `MILPSolver` module with
its defaults (`FQCfg.txt`) and three variants of it, written as differential
overrides of that fragment (no early pruning, no warm start, the bounds of
the integer variables as rows), and `GRBMILPSolver` and `CPXMILPSolver` as
references (`MILPCfg.txt`, a relative gap of 1e-9 and one thread); a
`Solver` that is not in the build is left out. The batteries are

- `batches/batch-paper`: both types of rows, the four fractions of integer
  variables of the paper, `n` in 10, 20, 30, `m` in 1, 5, 10 and three seeds
  each, 216 runs;

- `batches/batch-edge`: no row, no integer variable, equality and ranged
  rows, bounded and binary integer variables, bounded continuous ones, a
  maximization and the two kinds of infeasible instance, three seeds each,
  48 runs.

They are those that `ctest` runs: the second takes a few seconds, the first
about a minute and a half, most of it spent by the references on the pure
integer instances with 30 variables. A run is
also possible by hand from this directory, e.g.,

```sh
./MILPSolver_ConvexMIQP_test -S BSPar.txt -n 30 -i 0.5 -m 10 -t 0 -e 1
```
