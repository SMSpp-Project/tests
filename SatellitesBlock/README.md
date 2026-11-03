# test/SatellitesBlock

A tester of the `Block` of `SatellitesBlock`, the module for the Satellite
Constellation Design Problem: choosing the orbits of a constellation of
satellites so as to minimize either their number or the sum over the targets
of the maximum revisit times.

An instance is loaded into the `Block` whose classname is given with `-b`:
`ConstellationBlock` (the continuous version, one `SatelliteBlock` per
satellite), `DiscreteConstellationBlock` (the version where the observability
threshold of each satellite takes one of finitely many levels, one
`DiscreteSatelliteBlock` per satellite) or `MultiTargetBlock` (one
`SingleTargetBlock` per target). It is then solved by every `Solver` that the
`BlockSolverConfig` registers to it, and what they answer is cross-checked:

- a `:MILPSolver` on the abstract representation of the whole tree
  (`MILPCfg.txt`), which has to find the optimum given with `-r`;

- the `LagrangianDualSolver`, relaxing the constraints that link the
  sub-`Block` and solving the Lagrangian dual by a `BundleSolver`
  (`LDCfg.txt`), each sub-`Block` getting its `Solver` by classname from the
  meta-`BlockSolverConfig` `SubBSPar.txt`: the ad hoc
  `DiscreteSatelliteSolver`, which solves a `DiscreteSatelliteBlock` by
  inspection (`SatBSCfg-discrete.txt`), or a `:MILPSolver` for a
  `SatelliteBlock` and a `SingleTargetBlock` (`MILPBSCfg.txt`). The ad hoc
  `SatelliteSolver` is not used: it fixes the observability threshold to its
  maximum, hence its value is not a lower bound on the sub-problem, and
  neither is the Lagrangian dual computed with it. The problem being an
  integer one, the Lagrangian dual is a relaxation of it, which the battery
  declares with `-R ,r`: its lower bound has to stay below the optimum.

The usage of the executable is the following:

       ./SatellitesBlock_test [options] <file>
       -b, --block <classname>  the Block the instance is loaded into
                                [ConstellationBlock]
       -r, --ref <value>        the optimum of the instance [none]

plus the options that every SMS++ tester understands (`--help` lists them
all). The instances are those of the module, in `data/txt` of
SatellitesBlock.

The battery `batches/batch` runs the tester on each instance, loaded into
each `Block` it is an instance of, against the optimum of the `:MILPSolver`.
For the `MultiTargetBlock` it uses `BSPar-target.txt`, which only stops the
bundle after 50 iterations: each of them solves a `:MILPSolver` per
`SingleTargetBlock`, and the bound it gives when stopped is still a lower
bound.
