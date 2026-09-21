# tests/UCBlock/FW

The configurations of the Frank-Wolfe decomposition of a father `Block` whose
leaves are the `ThermalUnitBlock` of this suite. The runs are in the batteries
of the single units, [`batches-tub`](../batches-tub), together with those of
the dynamic programming solver, since they are posed on the same instances;
only a few of them, the reference being a monolithic relaxation with a cut
separation loop, i.e., minutes per run. A battery names these configurations
with `-c FW`, which makes every nested name resolve into this directory while
the working directory stays the one of the suite, where the instances are.

The tester is the generic one,
[`fw_test.cpp`](../../fw_test.cpp), the same the suite of `MCFBlock` builds on
its own Block: a leaf Block is read `K` times, the `K` copies become the
sub-`Block` of a father `AbstractBlock` with a random objective over their
`Variable`, and the `FrankWolfeSolver` that decomposes it is cross-checked
against a monolithic `:MILPSolver`. Which Block is read and which `:Solver`
are attached is the configurations' business, hence nothing of what follows is
in the source.

Here a `ThermalUnitDPSolver` is the Linear Minimization Oracle of each unit
(`TUBSCfg.txt`), and the reference
`:MILPSolver` solves the continuous relaxation *with* the cut separation loop
(`MILPCfg.txt`, `intRelaxIntVars = 2`). Since DP + Perspective Cuts
characterizes the convex hull of the integer solutions of the unit, the
Dantzig-Wolfe value `FrankWolfeSolver` computes (`intCvxComb = 1`) must equal
the perspective bound, i.e., Frank-Wolfe is a decomposition alternative to
DP + P/C, and this is what is checked.

The configurations are `BSPar.txt`, which maps the father to
`FatherBSCfg.txt` and each unit to `TUBSCfg.txt`, plus `FWCfg.txt` and
`MILPCfg.txt` that the father holds; `BSPar-fw.txt` registers the
`FrankWolfeSolver` alone and `BSPar-milp.txt` the reference `:MILPSolver`
alone, for the runs that time one of the two. The formulation is chosen by
`TUBCfg-DP.txt` (DP + P/C, which only the reference needs and which is
expensive to build) or by `TUBCfg-T.txt` (the plain `T` formulation, which
gives the identical result much faster when the reference is not run).





## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
