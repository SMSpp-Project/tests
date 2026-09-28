# tests/MCFBlock

The suite of `MCFBlock`: every `:Solver` that can solve a min-cost flow
problem, on the instances of the module, and the Frank-Wolfe decomposition of
a father `Block` whose leaves are those same `MCFBlock`.

`MCFBlock_test` reads an instance (in DIMACS or netCDF) into a `MCFBlock`,
attaches to it every `:Solver` its `BlockSolverConfig` names, and cross-checks
what they return while the instance changes: costs, capacities and deficits,
arcs opened and closed, arcs added and deleted. What each change is, and which
of them a run makes, is the bit-wise code of its `-k`. [BSPar.txt](BSPar.txt)
names a `MCFSolver< MCFSimplex >`, a `MCFSolver< RelaxIV >`, the four LEMON
algorithms of `MCFLemonSolver` on the graph that can change and a
`:MILPSolver`; [BSPar-large.txt](BSPar-large.txt) leaves out the two LEMON
algorithms that are slow on the large instances, CycleCanceling and
CapacityScaling, and [BSPar-static.txt](BSPar-static.txt) adds to those of
`BSPar.txt` the four LEMON algorithms on `SmartDigraph`, which does not
support the changes of the arcs: `batch-small` and `batch` also solve each of
their instances once, as it is, with the latter.

The batteries are in [batches](batches), one per family of instances, and each
of them runs its family with every tester that applies to it:

- [batch-small](batches/batch-small), the small instances of `MCFClassSolver`,
  static and with dynamic arcs, with `BSPar.txt`: the fast one, and the one
  that walks every code path of the decomposition;

- [batch](batches/batch) and [batch-dense](batches/batch-dense), the `net` and
  `goto` families of the module, the second one being the large instances,
  with `BSPar-large.txt`.

The configurations of the decomposition are the `Father*` ones, flat beside
those of every other `:Solver` of the suite, and the section below says what
each of them is for.


## The Frank-Wolfe decomposition of a father of K networks

The `MCFBlock` of this suite are also solved as the leaves of a decomposition,
in the batteries of the suite themselves, one per family of instances: each of
them walks its family with every `:Solver` that applies to it, the
decomposition comprised, so that what is compared is compared on the same
instances. `batch-small` is the fast one, and the one that walks every code
path of the decomposition.

The tester is the generic one, [`fw_test.cpp`](../fw_test.cpp): a "leaf"
`Block` is read `K` times from a `netCDF` file given on the command line; the
`K` copies become the sub-`Block` of a father `AbstractBlock`, and a random
father `FRealObjective` is built over their `Variable`. The father `Block` is
then solved both by a `FrankWolfeSolver` (which decomposes it, using the
`:Solver` registered to each sub-`Block` as a Linear Minimization Oracle) and
by a monolithic `:MILPSolver`, and the two optima are cross-checked by
`SolveAll()`.

The tester makes no assumption, at the C++ level, on which sub-`Block` is read
or which `:Solver` are attached: everything is driven by the
`BlockSolverConfig` (`-S`, and `-F` for the Polyhedral reference) and the
optional `BlockConfig` (`-B`) files, so the same source is built by the suite
of `UCBlock` on `ThermalUnitBlock` leaves (see [`UCBlock`](../UCBlock)). It is
the configuration's responsibility to register an appropriate LMO `:Solver` to
each sub-`Block` and a `:MILPSolver` for the cross-check; here the leaves are
`MCFBlock` and their oracle a network-simplex `MCFSolver`.

The usage of the executable is:

       ./MCFBlock_FW_test [ options ] <leaf Block netCDF file>

         -S, --solver-config <f>  BlockSolverConfig of the father (required)
         -B, --block-config <f>   BlockConfig applied to each sub-Block
         -k, --children <K>       number of sub-Block copies [2]
         -o, --objtype <t>        father objective: 0 DQuad, 1 Quad, 2 Poly [0]
         -a, --scale <s>          scale of the random father objective [1]
         -e, --seed <n>           random seed [1]
         -r, --rows <m>           PolyhedralFunction rows [nvar+1]
         -F, --refconf <f>        reference (MILP) BlockSolverConfig, Poly test
         -V, --vargroups <l>      comma-separated names of the sub-Block static
                                  variable groups to build the father over
                                  (default: the whole sub-Block objective)

For `-o 0/1` (a `DQuadFunction` / `QuadFunction` father) both `:Solver` are
registered to the same father via `-S` and `SolveAll()` cross-checks them. For
`-o 2` (a nonsmooth `PolyhedralFunction` father, where Frank-Wolfe has no
global-convergence guarantee) two copies are built, the Frank-Wolfe one (`-S`)
and a reference one in which the same `PolyhedralFunction` lives inside a
linearized `PolyhedralFunctionBlock` solved by a `:MILPSolver` (`-F`), and the
test checks that the Frank-Wolfe bracket `[ lb , value ]` contains the true
optimum.

The `-V` option lets the test build the father objective over named "physical"
variable groups of the sub-`Block` only, ignoring the formulation's auxiliary
objective variables.

A second tester, [`test_fw_mods.cpp`](test_fw_mods.cpp), runs the same
cross-check while the feasible region of a sub-`Block` changes (arc costs,
capacities and arc fixing): what it changes is of the `MCFBlock`, hence that
one is not generic and lives in the suite proper.

The configurations are the same set, with the same names, in every suite that
poses this tester on its own `Block`. `FatherBSPar.txt` is the
meta-`BlockSolverConfig` that dispatches by classname: the father goes to
`FatherBSCfg.txt` and each leaf to the `BlockSolverConfig` that makes its
`:Solver` the oracle. `FatherBSCfg.txt` registers the monolithic reference and
one `FrankWolfeSolver` per variant of the decomposition, the way the `BSPar`
of a suite do with the `:Solver` of its `Block`, so that a single run
cross-checks the variants against one another and all of them against the
reference; the variants are vanilla, Away-step, Blended Pairwise, Away-step
with aggregation, vanilla with the oracles in parallel, and the two that take
the direction from a stabilized master of two and of ten pieces, the master of
the last being a `MasterProblemBlock` solved by the `:Solver` of
`MPBCfg-FW.txt`, which names a backend that solves QPs.

Each variant is written as an override of `FWCfg.txt`, the fragment holding
what they have in common, so that what a variant changes is the only thing its
lines say; the reference reads `MILPCfg-FW.txt`. An override block writes the
extra-`Configuration` slot even when it changes nothing of it, because one
that leaves it out is read on to the end of the stream and swallows the
`ComputeConfig` that follows it. What `intLMOObj` selects is not among the
variants: that parameter decides whether the sub-`Block` objectives enter the
problem at all, and not merely what the oracle is shown, so `LMOLinear` solves
a different problem and has nothing to be compared with here.

Three more shapes of the same lineup exist, and no battery rewrites a
configuration file to select a variant. `FatherBSPar-fast.txt` keeps the
reference and the two variants that between them walk the most of the
machinery, Away-step with aggregation and the master problem one, for the
instances on which a single run costs the best part of an hour;
`FatherBSPar-fw.txt` keeps a variant alone, with nothing to compare it with,
for the runs that only time it; `FatherBSPar-milp.txt` keeps the reference
alone, for the same reason.

Of what only this suite has, `FatherBSPar-lemon.txt` makes the oracle of each
sub-`Block` the network simplex of LEMON (`MCFBSCfg-lemon.txt`) instead of the
one of MCFClass (`MCFBSCfg.txt`), and `FatherBSPar-warm.txt` /
`FatherBSCfg-warm.txt` keep the active set across a change of the feasible
region, which is what the rounds of `Modification` exercise. `batch-small`
also runs the decomposition with each of the other `Solver` of a `MCFBlock` as
the oracle, on copies of `FatherBSPar-lemon.txt` and of `MCFBSCfg.txt` or
`MCFBSCfg-lemon.txt` with the name of the `Solver` changed that the script
[fw_lmo_solvers](batches/fw_lmo_solvers) writes and removes at the end; the
other two batteries use the reduced lineup, a single run on their instances
taking the best part of an hour.

## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
