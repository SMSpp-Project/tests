# test/FrankWolfeSolver

A generic tester for the `FrankWolfeSolver` decomposition `:Solver`.

A "leaf" `Block` is read `K` times from a `netCDF` file given on the command
line; the `K` copies become the sub-`Block` of a father `AbstractBlock`, and a
random father `FRealObjective` is built over their `Variable`. The father
`Block` is then solved both by a `FrankWolfeSolver` (which decomposes it, using
the `:Solver` registered to each sub-`Block` as a Linear Minimization Oracle)
and by a monolithic `:MILPSolver`, and the two optima are cross-checked by
`SolveAll()`.

The tester is **generic**: it makes no assumption, at the C++ level, on which
sub-`Block` is read or which `:Solver` are attached — everything is driven by
the `BlockSolverConfig` (`-S`, and `-R` for the Polyhedral reference) and the
optional `BlockConfig` (`-B`) files. It is the configuration's responsibility to
register an appropriate LMO `:Solver` to each sub-`Block` and a `:MILPSolver` for
the cross-check.

The usage of the executable is:

       ./FWS_test [ options ] <leaf Block netCDF file>

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
variable groups of the sub-`Block` only (e.g. `p_thermal,u_thermal` for a
`ThermalUnitBlock`), ignoring the formulation's auxiliary objective variables.

## Configurations and batches

Several ready-made configuration files are provided:

- the `MCFBlock` family (`BSPar.txt`, `FatherBSCfg.txt`, `MCFBSCfg.txt`,
  `MILPCfg.txt`, `FWCfg.txt`, and the Polyhedral two-copy variants `BSPar-fw.txt`
  / `BSPar-milp.txt` / `FatherBSCfg-fw.txt` / `FatherBSCfg-milp.txt`): a
  network-simplex `MCFSolver` is the LMO of each `MCFBlock` and a `:MILPSolver`
  is the monolithic cross-check;

- the `ThermalUnitBlock` family (`BSPar-tub.txt`, `FatherBSCfg-tub.txt`,
  `TUBSCfg.txt`, `MILPCfg-tub.txt`, `FWCfg-tub.txt`, `TUBCfg.txt`): a
  `ThermalUnitDPSolver` is the LMO of each unit, the units are in the
  DP + Perspective-Cuts formulation (`BlockConfig` `TUBCfg.txt`), and the
  reference `:MILPSolver` solves the continuous relaxation *with* the cut
  separation loop (`intRelaxIntVars = 2`). Since DP + P/C characterizes the
  convex hull of the unit's integer solutions, this checks that the
  Dantzig-Wolfe value `FrankWolfeSolver` computes (`intCvxComb = 1`) equals the
  perspective bound — i.e. that Frank-Wolfe is a decomposition alternative to
  DP + P/C.

Two batch scripts are also provided:

- [regression](regression): a fast suite (small `MCFBlock` instances, static
  and dynamic arcs) covering vanilla / Away-step / BPCG, the bounded active set,
  parallel LMO and the Polyhedral bracket; meant to be expanded as the solver
  evolves. Registered as a `ctest` (`FWS_test/regression`).

- [batch-large](batch-large): a large-scale stress run on big `MCFBlock`
  instances (the "goto" family); expect it to take a long time.

A makefile is also provided that builds the executable including the `MCFBlock`,
`MCFClassSolver`, `UCBlock`, `MILPSolver` and `FrankWolfeSolver` modules (and,
obviously, the core SMS++ library).


## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
