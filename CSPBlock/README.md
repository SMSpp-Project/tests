# test/CSPBlock

A tester of `CSPBlock`, the `Block` of the constraint satisfaction and
optimization problems of constraint programming, which is loaded from a
file in (a subset of) the FlatZinc format.

The instance is loaded into a `CSPBlock` and solved by every `Solver` that
the `BlockSolverConfig` registers to it, and what they answer is
cross-checked, against one another and against the optimum of the instance.
`BSPar.txt` registers three `FlatZincCSPSolver`, which hand the problem to
CP-SAT, Gecode and Chuffed through MiniZinc, each within the time limit of
`FlatZincCfg.txt`; a `Solver` stopped by it is held to the bound of the
solution it has found, if any.

The usage of the executable is the following:

       ./CSPBlock_test [options] <file>
       -r, --ref <value>        the optimum of the instance [none]

plus the options that every SMS++ tester understands (`--help` lists them
all). The instances are those in `data/fzn` of CSPBlock, each family with
the optima of its instances in `optima.txt` (0 for those of a satisfaction
problem).

The battery `batches/batch` runs the tester on the instances of a family,
or of all of them: the RCPSP of PSPLIB j30 (`rcpsp`), graphs of the DIMACS
colouring benchmark (`colouring`), Golomb rulers (`golomb`) and quasigroup
completion problems (`qcp`). CTest runs it once per family, where the
`minizinc` driver is on the PATH.
