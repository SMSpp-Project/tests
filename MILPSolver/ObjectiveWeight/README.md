# tests/MILPSolver/ObjectiveWeight

The weights of a Block in the `:MILPSolver`: that of the `Objective` [see
`Block::set_objective_weight()`], the factor by which a `Solver` attached to
an ancestor of the Block multiplies its `Objective`, the product of the
weights along the path from the Block up to the ancestor, the latter
excluded, and that of the Variable [see `Block::set_variable_weight()`], the
factor by which it multiplies the coefficients of the `ColVariable` of the
Block in the rows of an ancestor.

`MILPSolver_ObjectiveWeight_test` builds a random tree of `AbstractBlock`
three levels deep twice out of the same data: once with a random weight on
the `Objective` and one on the Variable of each Block, the root comprised,
and once, the twin, with no weight, with the coefficients and the constant
of each `Objective` already multiplied by the weights along the path up to
the root, and with the coefficient of a `ColVariable` of a son in a row of
its father already multiplied by the weight of the Variable of the son. The `Solver`
of the `BlockSolverConfig` given with `-S` are attached to both roots and to
the first Block of the middle level of both trees, and each weighted Block is
checked against its twin: at the root the two have the same optimal value, at
the middle level that of the twin is that of the weighted Block times its own
weight, which a `Solver` attached to it does not see. Then, for a number of
rounds, both trees are changed in the same way and solved again.

    ./MILPSolver_ObjectiveWeight_test -S <BlockSolverConfig> [-k wchg]
                                      [-N nvar] [-s nson] [-e seed]
                                      [-n rounds] [-m changes] [-q prob]

Every Block has `nvar` `ColVariable` in a box which contains 0 and rows among
its own `ColVariable` and those of its sons, which 0 satisfies. Its
`Objective`, to be minimized, is a `LinearFunction` or a convex
`DQuadFunction`, half and half, with a constant. What is changed is coded
bit-wise in `wchg`: 1 linear coefficients, 2 the constant, 4 the linear and
quadratic coefficients of a `DQuadFunction`, 8 a `ColVariable` taken out of
an `Objective` or put back into it, 64 the coefficients of the rows, 128 the
weights of a Block (that of the Variable, and that of the `Objective` of a
leaf) while the `Solver` are attached, which they have to handle [see
`Solver::handles_weight_change()`]; with 16 the first leaf has a convex
`QuadFunction`, with 32 all the `Objective` are linear. With `-v` the changes
are printed. A change of the weights is also checked to be refused, with
nothing changed, while a `Solver` that cannot handle it is attached to an
ancestor.

[batches/batch-weight](batches/batch-weight) runs it on 27 trees of
different sizes and seeds in each of three settings: the `Objective` partly
quadratic with every change (`-k 207`) and with the `QuadFunction` too
(`-k 223`), with the `Solver` of `BSPar.txt` (`GRBMILPSolver`,
`CPXMILPSolver`, `SCIPMILPSolver`, `MOSEKMILPSolver`, `DAQPMILPSolver`), and
all linear (`-k 235`) with those of `BSPar-LP.txt`, which adds
`HiGHSMILPSolver`: the active set QP solver of HiGHS (up to 1.15.1 at least)
calls unbounded some of these convex QPs, whose variables are all bounded.
A `Solver` that the build has not is left out.
