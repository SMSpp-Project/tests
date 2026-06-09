# test/CapacitatedFacilityLocationBlock

A tester which provides initial tests for `CapacitatedFacilityLocationBlock`,
possibly `MCFBlock` and `MCFSolver`, possibly `BinaryKnapsackBlock` and any
specialised `Solver` for it, possibly `LagrangianDualSolver`, `LagBFunction`,
`BendersBFunction`, any `CDASolver` able to handle `C05Function` in the
`Objective` (such as `BundleSolver`), any `Solver` able to handle
(Mixed Integer) Linear Programs (such as `MILPSolver` and its derived classes
`CPXMILPSolver` , `GRBMILPSolver`, etc.), as well as for quite a lot of the 
mechanics of the "core" SMS++ library.

This executable, given the filename and (optionally) filetype of one
Capacitated Facility Location (CFL) instance in one of the several supported
file formats, reads the instance in a `CapacitatedFacilityLocationBlock`. It
then creates another `Block` via the R3Block mechanism, as configured by the
file R3BCfg.txt; this can then be either another
`CapacitatedFacilityLocationBlock` or a `MCFBlock` representing the continuous
relaxation of CFL (as a Min-Cost Flow problem). The two `Block` are
`BlockConfig`-ured by the files BPar1.txt and BPar2.txt, respectively, and a
`Solver` is attached to each by the `BlockSolverConfig` read from the files
BSPar1.txt and BSPar2.txt, respectively. Also, an `UpdateSolver` is manually
registered to the first `CapacitatedFacilityLocationBlock` so that any
change on it is automatically map\_forward-ed to the other `Block` (but not
vice-versa).

According to the value of the `niter` parameter, this is all: the `Solver`
are supposed to be exact (but it may well be that they are both exactly
solving the continuous relaxation, and the optimal values are compared.

Otherwise, the `Solver` attached to the R3Block is supposed to be solving
some kind of continuous relaxation and a certain number of rounds of a
simple slope scaling heuristic are ran on that `Block`: the continuous
solution is map\_back-ed to the original `CapacitatedFacilityLocationBlock`,
it is used to temporarily change the facility costs according to the standard
slope scaling idea, and then it is rounded up to obtain a feasible solution
(in this case expecting that the `CapacitatedFacilityLocationBlock` is in the
"splittable" version where continuous X[ i ][ j ] variables are feasible).
This is repeated for a maximum number of iterations, or until the solution
(value) of the relaxation stops changing. In this case the results (both
lower and upper bound) are compared with the optimal value of the other
`Solver`, but not expecting them to be equal. Note that there are at least
four significantly different implementations for the R3Block and its
`Solver`:

* The R3Block is a `CapacitatedFacilityLocationBlock` (in any formulation)
  and the `Solver` is, say, a `:MILPSolver` solving its continuous relaxation
  by standard LP tools, possibly with the dynamic generation of the strong
  forcing constraints to improve the bound.

* The R3Block is a `MCFBlock` solved by a `MCFSolver`: a much weaker bound,
  and the much worse/more fractional solution that comes with it, but
  obtained (possibly, much) faster.

* The R3Block is a `CapacitatedFacilityLocationBlock` in the KF and the
  `Solver` is a `LagrangianDualSolver`, say using `BundleSolver` to drive
  the solution of the Lagrangian Dual and `DPBinaryKnapsackSolver` to solve
  the Lagrangian subproblems: a much stronger bound, and the much better/less
  fractional solution that comes with it, but obtained at a (much) higher
  computational cost.

* The R3Block is a `CapacitatedFacilityLocationBlock` in the "Benders
  friendly" formulation (BenForm), whose hidden `BendersBFunction` separates
  Benders optimality/feasibility cuts from an inner `MCFBlock`; solved as a
  pure LP with an explicit user-cut separation loop (`MILPSolver` with
  `intRelaxIntVars == 2`), it reproduces the natural-formulation LP bound.

This shows how these completely different arrangements can be obtained with
exactly the same executable by just changing the configuration files. The
default config files (BPar1.txt, BPar2.txt, BSPar1.txt, BSPar2.txt,
R3BCfg.txt) live in this directory; each alternative arrangement keeps its
own copy of the five files in a dedicated folder: [MCF](MCF) (flow
relaxation), [cuts](cuts) (LP + user cuts), [LD](LD) and
[LDVScuts](LDVScuts) (Lagrangian dual of the knapsack relaxation), and
[Ben](Ben) (the Benders formulation). All the batch scripts live in
[batches](batches); each one `cd`-s into the folder holding the config set
it needs before running, so the same script works both via `ctest` and when
launched by hand (no per-folder executable symlink is needed).

All this is possibly repeated a number of times in a loop where data of the
CFLproblem (fixed and transportation costs, demands, capacities, facilities
being open, closed or fixed-open, and the problem being splittable or not)
are modified at random. Note that:

* changing the problem type will not go well with the slope scaling heuristic,
  since that only works with the splittable version of the problem;

* the MCF R3Block has an unavoidable issue with negative facility costs,
  that should therefore be avoided if one wants to be sure that the optimal
  values of the continuous relaxation coincide (see `NEGATIVE\_F\_COSTS` in
  [test.cpp](test.cpp));

* Many `MCFSolver` are based on the algorithms of the rather old
  [MCFClass project](https://github.com/frangio68/Min-Cost-Flow-Class), that
  have a questionable habit of requiring well-scaled absolute numerical
  tolerances to work; this may be necessary, again, when testing the
  `MCFBlock` R3Block (see `SET\_EPS` in [test.cpp](test.cpp), and please
  don't complain that this is not `Solver`-independent as it should be: it's
  the `MCFClass` solvers that are not properly implemented).

The usage of the executable is the following:

      ./CFL_test name [typ niter seed wchg #rounds #chng %chng]
      typ = [C], F, L, ignored if name ends in .nc4
      niter: how many Slope Scaling iterations [0]
      wchg: what to change, coded bit-wise 
            0 = facility cost, 1 = transportation cost
            2 = capacities, 3 = demands
            4 = close, 5 = re-open, 6 = fix-open fac.
            7 = change problem type (split/unsplit)
            8 (+256) = change abstract representation
            9 (+512) = set eps & no negative design
      #rounds: number of changing rounds [40]
      #chng: average number of elements to change [10]
      %chng: probability of any single change [0.5]

The [batches](batches) folder collects all the batch scripts; each runs the
test on a set of CFL instances taken from the `data/` folder of the
[CapacitatedFacilityLocationBlock
repo](https://gitlab.com/smspp/capacitatedfacilitylocationblock) (not the very
large ones, so that the tests end in reasonable time):

* [batch](batches/batch) / [batch-s](batches/batch-s): the default arrangement
  (natural-formulation LP vs the MCF flow relaxation), on a largish / small
  set of instances respectively;
* [batch-mcf](batches/batch-mcf): the MCF flow-relaxation arrangement;
* [batch-ben](batches/batch-ben): the Benders-formulation arrangement;
* [batch-cuts](batches/batch-cuts): the LP + user-cuts arrangement;
* [batch-ld](batches/batch-ld) / [batch-ldvscuts](batches/batch-ldvscuts): the
  Lagrangian-dual (knapsack relaxation) arrangement.

The `-s` scripts are restricted to instances small and easy enough that the
test can be run while solving them to integer optimality (required, e.g., to
test changing the problem type from splittable to unsplittable). Whenever the
R3Block is the MCF flow relaxation (the default and `batch-mcf`) the `wchg`
bitmask must include the +512 bit ("set eps & no negative design"), otherwise
the natural-LP and MCF bounds diverge after a few modification rounds. Only
the scenarios that currently pass are registered with `ctest` (see the
`CMakeLists.txt` for the per-batch status); the `batch-cuts`, `batch-ld` and
`batch-ldvscuts` arrangements are known to be broken and are kept off.

A makefile is also provided that builds the executable including the
`LagrangianDualSolver` module, the `BundleSolver` module, the `MILPSolver`
module, the `MCFBlock` module, the `BinaryKnapsackBlock` module, together of
course with the core SMS++ library.

All the tests passing give some confidence that no regressions have been
done for the involved `Block` (`CapacitatedFacilityLocationBlock`,
`MCFBlock`, and `BinaryKnapsackBlock`) and the `Solver` used in the
solution process.


## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
