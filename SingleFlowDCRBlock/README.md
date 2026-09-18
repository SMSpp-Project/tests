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

Two batch files are provided, `batch` for the SOCP formulation and
`batch-pc` for the P/C one, each sweeping a range of network sizes,
tightnesses and seeds; all of them passing is a good sign that no
regression has been made in the tested modules.

A makefile is also provided that builds the executable including the
`SingleFlowDCRBlock` and `MILPSolver` modules and all their dependencies
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
