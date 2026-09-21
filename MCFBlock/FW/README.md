# tests/MCFBlock/FW

The configurations of the Frank-Wolfe decomposition of a father `Block` whose
leaves are the `MCFBlock` of this suite, and what they are for. The runs are
in the batteries of the suite, [`batches`](../batches), one per family of
instances: each of them walks its family with every `:Solver` that applies to
it, the decomposition comprised, so that what is compared is compared on the
same instances. `batch-small` is the fast one, and the one that walks every
code path of the decomposition.

## The Frank-Wolfe decomposition

The tester is the generic one, [`fw_test.cpp`](../../fw_test.cpp): a "leaf"
`Block` is read `K` times from a `netCDF` file given on the command line; the
`K` copies become the sub-`Block` of a father `AbstractBlock`, and a random
father `FRealObjective` is built over their `Variable`. The father `Block` is
then solved both by a `FrankWolfeSolver` (which decomposes it, using the
`:Solver` registered to each sub-`Block` as a Linear Minimization Oracle) and
by a monolithic `:MILPSolver`, and the two optima are cross-checked by
`SolveAll()`.

The tester makes no assumption, at the C++ level, on which sub-`Block` is read
or which `:Solver` are attached: everything is driven by the
`BlockSolverConfig` (`-S`, and `-R` for the Polyhedral reference) and the
optional `BlockConfig` (`-B`) files, so the same source is built by the suite
of `UCBlock` on `ThermalUnitBlock` leaves (see [`UCBlock/FW`](../../UCBlock/FW)).
It is the configuration's responsibility to register an appropriate LMO
`:Solver` to each sub-`Block` and a `:MILPSolver` for the cross-check; here the
leaves are `MCFBlock` and their oracle a network-simplex `MCFSolver`.

The usage of the executable is:

       ./MCFBlock_FW_test [ options ] <leaf Block netCDF file>

         -S, --solver-config <f>  BlockSolverConfig of the father (required)
         -B, --block-config <f>   BlockConfig applied to each sub-Block
         -k, --children <K>       number of sub-Block copies [2]
         -o, --objtype <t>        father objective: 0 DQuad, 1 Quad, 2 Poly [0]
         -a, --scale <s>          scale of the random father objective [1]
         -e, --seed <n>           random seed [1]
         -r, --rows <m>           PolyhedralFunction rows [nvar+1]
         -R, --refconf <f>        reference (MILP) BlockSolverConfig, Poly test
         -V, --vargroups <l>      comma-separated names of the sub-Block static
                                  variable groups to build the father over
                                  (default: the whole sub-Block objective)

For `-o 0/1` (a `DQuadFunction` / `QuadFunction` father) both `:Solver` are
registered to the same father via `-S` and `SolveAll()` cross-checks them. For
`-o 2` (a nonsmooth `PolyhedralFunction` father, where Frank-Wolfe has no
global-convergence guarantee) two copies are built — the Frank-Wolfe one (`-S`)
and a reference one in which the same `PolyhedralFunction` lives inside a
linearized `PolyhedralFunctionBlock` solved by a `:MILPSolver` (`-R`) — and the
test checks that the Frank-Wolfe bracket `[ lb , value ]` contains the true
optimum.

The `-V` option lets the test build the father objective over named "physical"
variable groups of the sub-`Block` only, ignoring the formulation's auxiliary
objective variables.

A second tester, [`test_fw_mods.cpp`](../test_fw_mods.cpp), runs the same
cross-check while the feasible region of a sub-`Block` changes (arc costs,
capacities and arc fixing): what it changes is of the `MCFBlock`, hence that
one is not generic and lives in the suite proper.

## The configurations

The `MCFBlock` family: `BSPar.txt`, `FatherBSCfg.txt`, `MCFBSCfg.txt`,
`MILPCfg.txt`, `FWCfg.txt`, the Polyhedral two-copy variants `BSPar-fw.txt` /
`BSPar-milp.txt` / `FatherBSCfg-fw.txt` / `FatherBSCfg-milp.txt`, the
warm-started `BSPar-warm.txt` / `FatherBSCfg-warm.txt` / `FWCfg-warm.txt` that
the rounds of Modification use, and `BSPar-lemon.txt` /
`MCFBSCfg-lemon.txt`, where the LMO of each sub-`Block` is the network simplex
of LEMON instead of the one of MCFClass. A battery names them with `-c FW`,
which makes every nested name resolve into this directory while the working
directory stays the one of the suite, where the instances are.

**Single solver vs cross-check.** The single-block path runs every `:Solver`
registered to the father and cross-checks them. The reference `:MILPSolver`
can be either bundled in the `-S` config or supplied separately via the
optional `-R` config (registered *additively*). Omitting the reference runs
the solver under test **alone**, which is useful to profile `FrankWolfeSolver`
without the (possibly very slow) reference solve. The solver's own log can be
driven straight from the `ComputeConfig` via the standard `strLogFileName`
(the file to write) plus `intLogVerb` (1 = per-call summary, 2 = per-iteration)
parameters, no `-v` needed; `-v` remains available to send the log to
`stdout`.

## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
