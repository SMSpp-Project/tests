# test/SingleFlowDCRBlock

A tester of `SingleFlowDCRBlock`, the `Block` for single-flow
Delay-Constrained Routing (DCR) problems: routing one flow on a network at
minimum cost, reserving on each arc it uses a rate large enough for the
worst-case end-to-end delay, as given by a network-calculus formula, to
meet the deadline of the flow.

The instance is built at random around a source-sink path, every arc of
which is given enough capacity that the path meets the deadline: the
instance is therefore always feasible and its optimum finite, whatever the
size and the seed. The deadline is the delay of that path when each of its
arcs reserves its whole capacity, times the "tightness" `-t`: at `-t 1.05`
few routings meet it and the delay constraint is what decides the solution,
while at `-t 4` the deadline is loose and the problem is close to a plain
min-cost path.

The instance is then solved by every `Solver` that the `BlockSolverConfig`
registers to it, and what they answer is cross-checked: a `:MILPSolver`,
which solves the formulation that the `BlockConfig` asked the `Block` to
build, and the `SingleFlowDCRBendersSolver`, which ignores the abstract
representation altogether and works on the "physical" data through its
Benders-with-nested-Lagrangian scheme. Two formulations of the (nonlinear)
burst-delay terms are available:

- the "SOCP" one (`DCRCfg-socp.txt`), in which the two rotated cones are
  explicitly constructed: it is the exact formulation of the problem, and
  it needs a `:MILPSolver` that copes with conic constraints (`BSPar.txt`);

- the "P/C" one (`DCRCfg-pc.txt`), in which the cones are outer-approximated
  by dynamically separated linear cuts: every `:MILPSolver` solves it
  (`BSPar-pc.txt`), but until all the violated cuts have been separated it
  is a relaxation of the DCR problem, which is why the batch that uses it
  declares the `:MILPSolver` to be solving a relaxation (`-R r,`).

On top of the values, the `Solution` that each `Solver` produces is
checked: the routing and the reserved rates it carries must satisfy the
constraints of the DCR problem, which `SingleFlowDCRBlock::is_sol_feasible()`
answers without looking at the `Variable` of the `Block` at all, and the
cost they give must be the value the `Solver` declares. The `Variable` are
deliberately scrambled before asking for the `Solution`, so that a `Solver`
that builds it out of them rather than out of its own data structures is
caught.

The usage of the executable is the following:

       ./SingleFlowDCRBlock_test [options] [<file>]
       -B, --bconf <file>   BlockConfig file
       -S, --sconf <file>   BlockSolverConfig file
       -n, --nodes <n>      nodes of the random instance [10]
       -m, --arcs <m>       arcs of the random instance [30]
       -e, --seed <n>       seed of the random instance [1]
       -t, --tightness <t>  deadline over the delay of the guaranteed
                            path, >= 1 [1.2]

plus the options that every SMS++ tester understands (`--help` lists them
all). If a `<file>` is given, the `SingleFlowDCRBlock` is de-serialized out
of that netCDF file instead of being generated.

Three batch files are provided: `batch` for the SOCP formulation and
`batch-pc` for the P/C one, each sweeping a range of network sizes,
tightnesses and seeds of the random instances, and `batch-instances`, which
runs both formulations on the instances of the module, i.e., on 10 flows of
each of 14 real networks (see `data/README.md` in `SingleFlowDCRBlock`, whose
build downloads them); all of them passing is a good sign that no regression
has been made in the tested modules.

## MultiFlowDCRBlock_test

A tester of `MultiFlowDCRBlock`, the `Block` of the multi-flow DCR problem:
a set of flows, each of which is a `SingleFlowDCRBlock`, tied by the
capacity of the arcs they share. The instance is read out of a netCDF file,
and it is solved by every `Solver` that the `BlockSolverConfig` registers to
it, cross-checked as above:

- a `:MILPSolver` on the formulation that holds all the flows, each in the
  SOCP one (the default), which gives the optimum;

- the `LagrangianDualSolver`, which relaxes the mutual capacity constraints
  and solves each flow on its own (`LDCfg.txt`), by the
  `SingleFlowDCRBendersSolver` (`BSPar-multi.txt`, with
  `DCRBSCfg-benders.txt` for the flows) or by a `:MILPSolver`
  (`BSPar-multi-milp.txt`, with `DCRBSCfg-milp.txt`). The Lagrangian dual is
  not tight in general, and the bound is lower with the
  `SingleFlowDCRBendersSolver`, which stops on its own criterion, hence the
  batch declares the `LagrangianDualSolver` a relaxation (`-R ,r`).

The two `LagrangianDualSolver` are two configurations rather than two rows
of one: without copies of the flows the `Solver` of one would reach the
other, and a `SingleFlowDCRBlock` cannot be copied yet. The batch file in
`batches-multiflow` runs both on the instances of the module with the first
2 to 5 flows of each network: with 1 flow the mutual capacity of an arc is
0.8 times its own, too small for the flow on 11 networks out of 14, and on
an infeasible instance the `LagrangianDualSolver` stops without saying so.
The tester and its batch are built only if the `LagrangianDualSolver` and
the `BundleSolver` are.

The `ComputeConfig` of a `:MILPSolver` is `MILPCfg.txt`, and Gurobi reads
it through `GRBCfg.txt`, which adds a numerical focus of 2: without it the
barrier stops short of the optimum of the SOCP formulation on some
instances of the module, with a "Numeric error" or with a value a little
below the optimum.

A makefile is also provided that builds `SingleFlowDCRBlock_test` including
the `SingleFlowDCRBlock` and `MILPSolver` modules and all their dependencies
(hence, obviously, the core SMS++ library).

## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

- **Donato Meoli**  
  Dipartimento di Informatica  
  Università di Pisa

## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
