# test/Write-Read

A tester which provides very comprehensive tests for the functions
`AbstractBlock::read_mps`and `AbstractBlock::read_lp`, along with 
some tests for any  `CDASolver` able to handle Linear Programs 
(such as `MILPSolver` and its derived classes`CPXMILPSolver` , 
`SCIPMILPSolver` , `GRBMILPSolver` and `HiGHSMILPSolver`), as well 
as for some of the mechanics of the "core" SMS++ library.

This executable, given the input parameter n, constructs a "random"
Linear Program with n `ColVariable`, a "linear objective" 
(`FRealObjective` with a `LinearFunction` inside) and "linear constraints"
(`FRowConstraint` with a `LinearFunction` inside) and represent it in an 
`AbstractBlock` (LPBlock). Moreover, the built `ColVariable` can have simple
bound constraints imposed on them.

An appropriate `CDASolver` is attached to LPBlock, which
can be any `Solver` capable of handling Linear Programs and print them out
in a `.lp` or `.mps` format (say, some derived class of `MILPSolver` such as 
`CPXMILPSolver`). The file containing all the model data is then
written and the model is solved. Note that to choose which format you
prefer to use, the global variable `TEST_FILE_TYPE` can be set inside
the test.cpp file.

At this point, a new `AbstractBlock` (SecondLPBlock) is created, 
and the previously written `.mps/.lp` file is read and loaded. As before, an 
appropriate `CDASolver` is attached to this Block and the new Block
is solved.

After all this is done, the results (termination status and objective
value, if applicable) are compared.

The LP is then repeatedly randomly modified and re-solved several times; 
each time the same procedure is applied and the results of the two 
`Solver` are compared.

The usage of the executable is the following:

       ./writeandread_test seed [wchg nvar dens #rounds #chng %chng]
       wchg: what to change, coded bit-wise [31]
             0 = add rows, 1 = delete rows 
             2 = modify rows, 3 = modify constants
             4 = change global lower/upper bound
       nvar: number of variables [10]
       dens: rows / variables [4]
       #rounds: how many iterations [40]
       #chng: number changes [10]
       %chng: probability of changing [0.5]

A batch file is provided that runs a not-so-large set of tests with
different sizes and seeds of the random generator; all these passing is a
good sign that no regressions have been done for the tested modules, and
in particular for `AbstractBlock::read_mps` and `AbstractBlock::read_lp`.

A makefile is also provided that builds the executable including 
and all its dependencies, in particular `MILPSolver` (and, obviously, 
the core SMS++ library).

## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

- **Enrico Calandrini**  
  Dipartimento di Informatica  
  Università di Pisa


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
