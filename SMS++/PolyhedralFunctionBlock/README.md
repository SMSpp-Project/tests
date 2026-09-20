# test/PolyhedralFunctionBlock

A tester which provides very comprehensive tests for `PolyhedralFunction`
and especially `PolyhedralFunctionBlock`, plus quited some tests for any
`CDASolver` able to handle multiple `C05Function` in the objective (such
as `BundleSolver`) and any `CDASolver` able to handle Linear Programs
(such as `MILPSolver` and its derived classes `CPXMILPSolver` ,
`SCIPMILPSolver` and `GRBMILPSolver`), as well as for some of the mechanics 
of the "core" SMS++ library.

This executable, given the input parameters nvar and nf, constructs
abs( nf ) "random" `PolyhedralFunction` with nvar variables, each inside
one of abs( nf ) `PolyhedralFunctionBlock`. Then, another identical set
of abs( nf ) `PolyhedralFunctionBlock` is R3-Block-ed from the original one.

If abs( nf ) > 1, both sets of `PolyhedralFunctionBlock` are bunched each as
sons of two separate `AbstractBlock`, LPBlock and NDOBlock, otherwise "empty"
save for two identical set of nvar `ColVariable` (that can have simple bound
constraints imposed on them if the macro `HAVE_CONSTRAINTS` is properly set).
Otherwise, the two original `PolyhedralFunctionBlock` are used as LPBlock
and NDOBlock (which means that the `ColVariable` are directly inserted in
there, which is possible because `PolyhedralFunctionBlock` is an
`AbstractBlock`). If nf < 0, LPBlock and NDOBlock are also given two
identical linear `Objective` (a `FRealObjective` with a `LinearFunction`
inside).

Then, LPBlock is configured to use the "linearized" representation, and has
an appropriate LP `Solver` registered (say, some derived class of `MILPSolver`
such as `CPXMILPSolver` , `SCIPMILPSolver` or `GRBMILPSolver`); 
also, `UpdateSolver` are registered to all its sons (`PolyhedralFunctionBlock`,
or directly to itself if and( nf ) = 1) that maps all the `Modification` 
to the corresponding son of NDOBlock (or to NDOBlock if abs( nf ) = 1). 
The latter is configured to use the "natural" representation and has an 
appropriate NDO `Solver` attached (say, `BundleSolver`).

After all this is done, the NDOBlock and LPBlock are solved with the
registered `Solver` and the results (termination status and objective
value, if applicable) are compared.

Then, repeatedly a "linearized" `PolyhedralFunctionBlock` in LPBlock is
randomly modified, with the `Modification` automatically transmitted to
the corresponding "natural" `PolyhedralFunctionBlock` in NDOBlock to keep
them in synch. Then both are re-solved several times; each time the results
of the two `Solver` are compared.

The usage of the executable is the following:

      ./PolyhedralFunctionBlock_test seed [wchg nvar dens #nf #rounds #chng %chng]
       wchg: what to change, coded bit-wise [319]
             0 = add rows, 1 = delete rows 
             2 = modify rows, 3 = modify constants
             4 = change global lower/upper bound
             5 = change linear objective
             8 (+256) = do "abstract" changes
       nvar: number of variables [10]
       dens: rows / variables [3]
       #nf: number of PolyhedralFunction in the sub-Block [0]
       #rounds: how many iterations [40]
       #chng: number of changes [10]
       %chng: probability of changing [0.5]

A batch file is provided that runs a not-so-large buy yet comprehensive set
of tests with different sizes and seeds of the random generator; all these
passing is a good sign that no regressions have been done for the tested
modules, and in particular for `PolyhedralFunction` and
`PolyhedralFunctionBlock`.

A makefile is also provided that builds the executable including the
`BundleSolver` module and all its dependencies, in particular `MILPSolver`
(and, obviously, the core SMS++ library).


## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
