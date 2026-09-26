# tests/SMS++/AbstractBlock

The four testers posed on an `AbstractBlock`, i.e., on the Block that is
nothing but its own abstract representation: the `LagrangianDualSolver` on
the box-structured one, the copy of the abstract representation that
`AbstractBlock::mirror()` makes, the round trip of a linear program through
a file, and the Benders decomposition of a two-stage linear program. The
first three are run with their own batch in [batches](batches), the fourth
runs all its cases in a single call.


## The box-structured Block and its Lagrangian dual

`LagrangianDualSolver_Box_test` provides very comprehensive tests for
`LagrangianDualSolver`, `LagBFunction`, `BoxSolver`, any `CDASolver` able to
handle `C05Function` in the `Objective`, any `CDASolver` able to handle Linear
Programs (such as `MILPSolver` and its derived classes `CPXMILPSolver`,
`SCIPMILPSolver` and `GRBMILPSolver`), as well as for quite a lot of the
mechanics of the "core" SMS++ library.

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

    ./LagrangianDualSolver_Box_test seed [wchg nvar nson dens #rounds #chng %chng]
       wchg: what to change, coded bit-wise [17]
             0 = bounds, 1 = objective
             2 = linking coefficients, 3 = linking lhs/rhs
             6 = a linking constraint is added or removed
       nvar: number of variables [10]
       nson: number of sub-Block [2]
       dens: number of constraints, fraction of nvar * nson [0.1]
       #rounds: how many iterations [40]
       #chng: number changes [10]
       %chng: probability of changing [0.5]

Bit 6 is the one the batch does not turn on, and on purpose: a linking
constraint that is born after the Solver has been attached is a case nothing
else in the suite covers, and the way down to it is not walkable yet. With a
LagrangianDualSolver posed on the Block and a BundleSolver inside it, the
Lagrangian Dual grows one multiplier, the BundleSolver meets a `BlockModAD`
it answers with "not handled (yet)", and past that the master problem answers
`add_vars()` with "structural resize not implemented yet". The round is here
so that whoever fills those in has the case that exercises them, and asking
for it is enough to see where the way stops.

[batches/batch-box](batches/batch-box) runs a largish (but typically
terminating within half an hour) set of tests with different sizes and seeds
of the random generator, in either of two regimes: `LDS`, the default, where
the Lagrangian dual is computed and cross-checked against the Linear Program,
and `PPH`, where what is exercised is the primal proximal heuristic. All of
them passing is a good sign that no regressions have been done for the tested
modules.


## The copy of the abstract representation

`AbstractBlock_mirror_test` is a tester for `AbstractBlock::mirror()`, i.e.,
for the copy of the abstract representation of a Block that any Block has
without having written a line for it.

The tester builds the copy of two Block and checks three things: that the
copy is the same problem as the original, by solving the two with the same
`:MILPSolver` and comparing the optimal values; that solution information
moves from the copy back to the original; and that the copy follows the
original when this changes, which an `UpdateSolver` attached to the latter
does by mapping every `Modification` forward.

The two originals are an `AbstractBlock` carrying one object of each kind the
mirror knows, an inner Block whose `Constraint` is written in the `Variable`
of the father comprised, and a `BinaryKnapsackBlock`, which has a physical
representation of its own and generates its abstract one.

    ./AbstractBlock_mirror_test [ <solver> ]

with `<solver>` the name of the `:MILPSolver` to use, `CPXMILPSolver` by
default. [batches/batch-mirror](batches/batch-mirror) runs the tester over
every `:MILPSolver` that is in the build.


## The round trip through a file

`AbstractBlock_readwrite_test` provides very comprehensive tests for the
functions `AbstractBlock::read_mps` and `AbstractBlock::read_lp`, along with
some tests for any `CDASolver` able to handle Linear Programs (such as
`MILPSolver` and its derived classes `CPXMILPSolver`, `SCIPMILPSolver`,
`GRBMILPSolver` and `HiGHSMILPSolver`), as well as for some of the mechanics
of the "core" SMS++ library.

This executable, given the input parameter n, constructs a "random"
Linear Program with n `ColVariable`, a "linear objective"
(`FRealObjective` with a `LinearFunction` inside) and "linear constraints"
(`FRowConstraint` with a `LinearFunction` inside) and represent it in an
`AbstractBlock` (LPBlock). Moreover, the built `ColVariable` can have simple
bound constraints imposed on them.

An appropriate `CDASolver` is attached to LPBlock, which can be any `Solver`
capable of handling Linear Programs and print them out in a `.lp` or `.mps`
format (say, some derived class of `MILPSolver` such as `CPXMILPSolver`). The
file containing all the model data is then written and the model is solved.
Note that to choose which format you prefer to use, the global variable
`TEST_FILE_TYPE` can be set inside the `test_readwrite.cpp` file.

At this point, a new `AbstractBlock` (SecondLPBlock) is created, and the
previously written `.mps/.lp` file is read and loaded. As before, an
appropriate `CDASolver` is attached to this Block and the new Block is solved.

After all this is done, the results (termination status and objective
value, if applicable) are compared.

The LP is then repeatedly randomly modified and re-solved several times;
each time the same procedure is applied and the results of the two
`Solver` are compared.

The usage of the executable is the following:

       ./AbstractBlock_readwrite_test seed [wchg nvar dens #rounds #chng %chng]
       wchg: what to change, coded bit-wise [31]
             0 = add rows, 1 = delete rows
             2 = modify rows, 3 = modify constants
             4 = change global lower/upper bound
       nvar: number of variables [10]
       dens: rows / variables [4]
       #rounds: how many iterations [40]
       #chng: number changes [10]
       %chng: probability of changing [0.5]

[batches/batch-readwrite](batches/batch-readwrite) runs a not-so-large set of
tests with different sizes and seeds of the random generator.

## The Benders decomposition of a two-stage linear program

`AbstractBlock_BDS_test` is a tester for `BendersDecompositionSolver`. It
writes the instance it runs on out of `AbstractBlock`, so that it needs no
Block but the ones of the core library: a small two-stage linear program, in
two equivalent models. One is monolithic, the first-stage `Variable` and the
second-stage ones in a single `AbstractBlock`, which a `:MILPSolver` solves to
give the reference optimum; the other is structured, a master `Block` carrying
the first-stage `Variable` and one nested sub-`Block` per scenario carrying
the second-stage ones, whose `Constraint` couple the master, which
`BendersDecompositionSolver` solves. What is checked is that the two optima
agree within a relative tolerance, whichever way the Solver is asked to work.

Every way the Solver has of writing a cut is run on that instance, one
`BlockSolverConfig` each: the multi-cut and the single-cut master, the
cutting-plane and the bundle one, the Pareto-optimal cut, the two
normalizations of the feasibility cut and its phase one, the combinatorial cut
of a binary master, the cut that serves for both feasibility and optimality,
what keeping a subproblem in the master costs, and what the Block is given
back as at the end of a `compute()`. The instances that make each of them
matter are built on purpose, i.e., with the subproblems feasible everywhere
for the optimality cuts and infeasible at some first-stage point for the
feasibility ones, and in the nested shape a subproblem has when it is a model
of its own rather than a bare set of rows.

The configurations are the `BSPar-BDS*.txt` files, one per way of writing a
cut (`BSPar-BDS.txt` the convex regime, whose master is a `BundleSolver`,
`BSPar-BDS-MILP.txt` the MILP regime, whose master is a `:MILPSolver`, and
the others each a variant of the latter named after what it changes); the
master is configured by `BDSMCfg.txt` in the convex regime (with `MPBCfg.txt`
for the master problem of the bundle) and by `BDSMCfg-MILP.txt` in the MILP
one, and the subproblems by `BDSSCfg.txt`, which also gives the reference
optimum of the monolithic model. The tester takes no argument:

    ./AbstractBlock_BDS_test


A makefile is also provided that builds the four executables, including the
`LagrangianDualSolver` module, the `BundleSolver` module, the
`BendersDecompositionSolver` module, `BinaryKnapsackBlock` and all their
dependencies, in particular `MILPSolver`, together of course with the core
SMS++ library.


## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

- **Enrico Calandrini**  
  Dipartimento di Informatica  
  Università di Pisa

- **Donato Meoli**  
  Dipartimento di Informatica  
  Università di Pisa


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
