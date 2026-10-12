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

       ./PolyhedralFunctionBlock_test [options]
       -e, --seed <n>      seed of the random generator [0]
       -k, --wchg <bits>   what to change, coded bit-wise [63]
                           1 = add rows, 2 = delete rows
                           4 = modify rows, 8 = modify constants
                           16 = change local lower/upper bound
                           32 = change linear objective
                           64 = change global lower/upper bound
                           512 = do "abstract" changes
                           1024 = use the dual (Fenchel) representation
                                  for the LPBlock
                           2048 = scale each PFB row locally
                           4096 = apply one global PFB epigraph scale
       -n, --rounds <n>    how many iterations [40]
       -m, --nchng <n>     number of changes [10]
       -q, --pchng <p>     probability of changing [0.5]
       -N, --nvar <n>      number of variables [10]
       -d, --dens <x>      rows / variables [3]
       -f, --nf <n>        number of PolyhedralFunction in the sub-Block [0]
       -V, --pvert <p>     probability that a generated row is vertical [0]
       -X, --detach <0|1>  if the Solver of NDOBlock is detached and
                           re-attached [0]

A batch file is provided that runs a not-so-large buy yet comprehensive set
of tests with different sizes and seeds of the random generator; all these
passing is a good sign that no regressions have been done for the tested
modules, and in particular for `PolyhedralFunction` and
`PolyhedralFunctionBlock`.

`PolyhedralFunctionBlock_prune_test` is a unit test of
`PolyhedralFunctionBlock::remove_redundant_rows()`: it builds a convex
`PolyhedralFunction` with a row parallel to (and dominated by) another one
and a row that is inactive, and checks that exactly these two are removed,
the second by the LP that the `:MILPSolver` of `LPPar.txt` solves over the
epigraph. The geometric pruning alone is tested in
[PolyhedralFunction](../PolyhedralFunction).

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
