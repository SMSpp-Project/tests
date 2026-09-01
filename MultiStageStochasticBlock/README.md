# test/MultiStageStochasticBlock

A tester which provides initial tests for `MultiStageStochasticBlock`,
the SMS++ Block that aggregates one `TwoStageStochasticBlock` per
outer-stage scenario and ties the first-stage variables across them with
an outer set of non-anticipativity constraints, as well as for quite a
lot of the mechanics of the "core" SMS++ library.

This executable, given the filename of a netCDF file containing the
description of a `MultiStageStochasticBlock`, solves its deterministic
equivalent (the extensive form of the whole multi-stage scenario tree)
with a `:MILPSolver`, comparing the result against an optional reference
objective value passed on the command line. The running time is printed.
The relative tolerance for the comparison is fixed at `1e-5`.

The usage of the executable is the following:

       ./MSSB_test MSSB-file [BSC-file ws ref]
       BSC-file: BlockSolverConfig description [BSPar-MS.txt]
       ws:       0 = LagrangianDualSolver, 1 = reserved [0]
       ref:      reference objective value to compare against [none]

The inner Block of each `TwoStageStochasticBlock` can be any SMS++ Block:
the tester is problem-agnostic. The instances shipped under `batches/`
happen to embed a `UCBlock` (for energy-community-type applications), but
nothing in the executable assumes a specific inner Block type.

For the instances produced by `csv2nc4.jl --multistage`, the multi-stage
extensive form coincides with the flattened `TwoStageStochasticBlock`
one, so the reference objective values under `batches/batch-ec` are
exactly those of the corresponding `TSSB_EC_*` instances of
`tests/TwoStageStochasticBlock`.

The instances of `batches/batch-resilient` are the multi-stage counterpart
of the resilient family: the same PyPSA-Eur network the two-stage ones are
drawn from, with its single axis of uncertainty split in two, the climate
year that scales the availability of the renewables and the hydro inflow in
the outer stage, and the demand, drawn conditional on it, in the inner one.
Their reference objective values are the optimum of the equivalent flat
network solved by PyPSA, which coincides with the tree one as long as the
only here-and-now Variable are the design ones.

A makefile is also provided that builds the executable including the
`MultiStageStochasticBlock`, `TwoStageStochasticBlock`,
`LagrangianDualSolver`, `BundleSolver`, `MILPSolver` modules and the core
SMS++ library, together with the inner-Block module needed by the
instances in `batches/` (currently `UCBlock`).

## Configuration files

- `BSPar-MS.txt` — outer `BlockSolverConfig` registering a `:MILPSolver`
  (default `GRBMILPSolver`) on the deterministic equivalent.
- `MILPCfg.txt` — `ComputeConfig` of the `:MILPSolver` (continuous
  relaxation, used to match the `EnergyCommunity.jl` linear reference).
- `InnerBCfg.txt` — "meta"-`BlockConfig` mapping each inner-Block
  classname (`ThermalUnitBlock`, `DCNetworkBlock`, ...) to its own
  `BlockConfig` (`TUBCfg.txt`, `DCNBCfg.txt`, `ACBCfg.txt`,
  `PFBCfg.txt`), applied recursively over the whole Block tree.


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
