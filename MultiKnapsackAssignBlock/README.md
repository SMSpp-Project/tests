# test/MultiKnapsackAssignBlock

A tester of `MultiKnapsackAssignBlock`, the `Block` of the Multiple Knapsack
Assignment Problem: N items, partitioned into R classes, are placed in M
knapsacks, each of which is given at most one class and only holds items of
that class within its capacity, so as to maximize the total profit.

An instance is loaded into a `MultiKnapsackAssignBlock`, whose sub-`Block`
are the `BinaryKnapsackBlock` of the pairs of a knapsack and a class. It is
then solved by every `Solver` that the `BlockSolverConfig` registers to it,
and what they answer is cross-checked:

- a `:MILPSolver` on the abstract representation of the whole tree, within
  a time limit of 60 seconds (`MILPCfg.txt`), which gives an interval that
  contains the optimum, if not the optimum itself;

- the `LagrangianDualSolver`, relaxing the constraints that link the
  sub-`Block` (each item in at most one knapsack, each knapsack given at
  most one class) and solving the Lagrangian dual by a `BundleSolver`
  (`LDCfg.txt`), each `BinaryKnapsackBlock` getting its `Solver` by
  classname from the meta-`BlockSolverConfig` `SubBSPar.txt`, i.e., the
  `CoreDPBinaryKnapsackSolver` of `KnapBSCfg.txt`. The problem being an
  integer one, the Lagrangian dual is a relaxation of it, which the battery
  declares with `-R ,r`: its upper bound has to stay above the value of the
  best solution that the `:MILPSolver` finds.

The usage of the executable is the following:

       ./MultiKnapsackAssignBlock_test [options] <file>
       -r, --ref <value>        the optimum of the instance [none]

plus the options that every SMS++ tester understands (`--help` lists them
all). The instances are those of the module, in `data/txt` of
MultiKnapsackAssignBlock, which CMake downloads from the GitLab Package
Registry of the module (the fixture `extract_mkab_txt`).

The battery `batches/batch` runs the tester on each of the 12 instances.
