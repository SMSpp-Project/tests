# tests/UCBlock/FW

The Frank-Wolfe decomposition of a father `Block` whose leaves are the
`ThermalUnitBlock` of this suite.

The tester is the generic one, [`fw_test.cpp`](../../fw_test.cpp), the same the
suite of `MCFBlock` builds on its own `Block` (see
[`MCFBlock/FW`](../../MCFBlock/FW), where its options are documented): a leaf
`Block` is read `K` times, the `K` copies become the sub-`Block` of a father
`AbstractBlock` with a random objective over their `Variable`, and the
`FrankWolfeSolver` that decomposes it is cross-checked against a monolithic
`:MILPSolver`. Which `Block` is read and which `:Solver` are attached is the
configurations' business, hence nothing of what follows is in the source.

A `ThermalUnitDPSolver` is the Linear Minimization Oracle of each unit, and the
reference `:MILPSolver` solves the continuous relaxation *with* the cut
separation loop (`intRelaxIntVars = 2`). Since DP + Perspective Cuts
characterizes the convex hull of the integer solutions of the unit, the
Dantzig-Wolfe value `FrankWolfeSolver` computes (`intCvxComb = 1`) must equal
the perspective bound, i.e., Frank-Wolfe is a decomposition alternative to
DP + P/C, and this is what the batch checks.

The executable is built by CMake with the suite, and by the makefile of the
suite with `make fw`:

       ./UCBlock_FW_test [ options ] <ThermalUnitBlock netCDF file>

## Configurations and batch

The `ThermalUnitBlock` family: `BSPar-tub.txt`, `FatherBSCfg-tub.txt`,
`TUBSCfg.txt`, `MILPCfg-tub.txt`, `FWCfg-tub.txt`, and the two formulation
`BlockConfig` `TUBCfg-DP.txt` / `TUBCfg-T.txt`.

**Two formulation `BlockConfig`.** `TUBCfg-DP.txt` selects the DP + P/C
formulation (`static_variables = 11`); it is needed **only** for the reference
`:MILPSolver` (which solves the monolithic DP + P/C relaxation), and building
that abstract formulation is expensive. `FrankWolfeSolver` does **not** use it,
its `ThermalUnitDPSolver` oracle having its own internal DP, so any FW-only run
should pass `-B TUBCfg-T.txt` (the plain `T` formulation,
`static_variables = 1`, no Perspective Cuts), which gives the identical result
much faster (e.g., ~2s vs ~28s on a 96-period unit). Use `TUBCfg-DP.txt` only
for the cross-check or the reference-only timing. `BSPar-tub-fwonly.txt`
registers only the `FrankWolfeSolver`; `BSPar-tub-ref.txt` registers only the
reference `:MILPSolver` (the MIQP-only run, for time comparison).

[batch-tub](batch-tub) runs the cross-check on the units of this suite, from
this directory. It is **not** registered as a `ctest`, since the reference
solves a monolithic relaxation with a cut separation loop and Frank-Wolfe
converges sublinearly: expect minutes per run.


## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
