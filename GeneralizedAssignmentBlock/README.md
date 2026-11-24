# test/GeneralizedAssignmentBlock

A tester of `GeneralizedAssignmentBlock`, the `Block` of the Generalized
Assignment Problem: N jobs are assigned to M agents, each job to exactly one
agent, each agent within the capacity of its resource, so as to minimize the
total cost.

An instance is loaded into a `GeneralizedAssignmentBlock`, whose sub-`Block`
are the minimization `BinaryKnapsackBlock` of the agents. It is then solved
by every `Solver` that the `BlockSolverConfig` registers to it, and what
they answer is cross-checked:

- a `:MILPSolver` on the abstract representation of the whole tree, within
  a time limit of 60 seconds (`MILPCfg.txt`), which gives an interval that
  contains the optimum, if not the optimum itself;

- the `LagrangianDualSolver`, relaxing the assignment constraints, whose
  multipliers are free since they are equalities, and solving the
  Lagrangian dual by a `BundleSolver` (`LDCfg.txt`), each
  `BinaryKnapsackBlock` getting its `Solver` by classname from the
  meta-`BlockSolverConfig` `SubBSPar.txt`, i.e., the
  `CoreDPBinaryKnapsackSolver` of `KnapBSCfg.txt`. The problem being an
  integer one, the Lagrangian dual is a relaxation of it, which the battery
  declares with `-R`: its lower bound has to stay below the value of the
  best solution that the `:MILPSolver` finds;

- the same `LagrangianDualSolver` with a `SubGradSolver` as the inner
  `Solver` (`LDCfg-SG.txt`), which is also a relaxation and is held to the
  same bound; a build without `SubGradSolver` leaves it out of the
  cross-check.

- the `RepairGeneralizedAssignmentSolver`, a heuristic that turns the point
  the `Variable` hold into a feasible assignment by a greedy construction
  and a local search with shift and swap moves (`RepairCfg.txt`), whose
  upper bound has to stay above the lower bounds of the others;

- a `PrimalProximalHeur` on the same Lagrangian dual, a few proximal
  iterations within 10 seconds, whose primal recovery is the
  `RepairGeneralizedAssignmentSolver` of `RepairBSCfg.txt` in place of a
  restricted MIP (`PPHCfg.txt`);

- a `BranchAndXSolver` whose nodes are solved by a
  `LagrangianDualRelaxationSolver`, the `PrimalProximalHeur` above with the
  same recovery at every node (`intHeurFreq` 1) and the branching by fixing
  the jobs of each agent in its `BinaryKnapsackBlock`, within 10 seconds
  (`BXCfg.txt`, `InnerBSCfg-LD.txt`, `LDRSCfg.txt`); a build without
  `BranchAndXSolver` leaves it out of the cross-check.

The usage of the executable is the following:

       ./GeneralizedAssignmentBlock_test [options] <file>
       -r, --ref <value>        the optimum of the instance [none]

plus the options that every SMS++ tester understands (`--help` lists them
all). The instances are the OR-Library ones of types A, B, C and D, in
`data/txt` of GeneralizedAssignmentBlock.

The battery `batches/batch` runs the tester on each of the 24 instances.
