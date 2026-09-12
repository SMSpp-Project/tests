# test/ThermalUnitBlock_Solver

A tester for the `ThermalUnitDPSolver` specialised Dynamic Programming
`:Solver` for `ThermalUnitBlock`. A `ThermalUnitBlock` instance is loaded
from a `netCDF` file, two different `:Solver` are registered to the
`ThermalUnitBlock`, the second of which is assumed to be a
`ThermalUnitDPSolver`, the `ThermalUnitBlock` is solved by both `Solver`
and the results are compared. The `ThermalUnitBlock` is then repeatedly
randomly modified and re-solved several times, the results are compared.

The usage of the executable is the following:

       ./TUDPS_test file [seed wchg wf #rounds #chng %chng]
       wchg: what to change, coded bit-wise [135]
             0 = fixed costs, 1 = linear costs
             2 = quadratic costs
             +128 = also change abstract representation
       wf:   what formulation, coded bit-wise [1]
             0 = 3bin, 1 = T, 2 = pt, 3 = DP
             4 = SU, 5 = SD (formulation)
             +8 = also use perspective cuts
       #rounds: how many iterations [100]
       #chng: number changes [10]
       %chng: probability of changing [0.6]

Two sets of batch files are provided in the [batches](batches) and
[cuts](batches) that solve different sets of the available single-unit
instances with some of the (many) different formulations supported
by `ThermalUnitBlock`, in particular without and with "Perspective
Cuts".

The same tester drives the nuclear units: a `NuclearUnitBlock` is a
`ThermalUnitBlock`, and the `BlockSolverConfig` `BSCfg-nuc.txt` attaches the
`NuclearUnitExtDPSolver` in place of the thermal dynamic programming Solver.
`batch-nuclear` checks both the original model of the modulations and the
operating rules of nuclear units (modulations of several instants, stability
after a modulation and after a start-up, daily limits, deep decreases and
their costs, power bands) together with spinning reserves and reactive power,
on a load that the unit can follow while on throughout and on one whose
trough lies below the minimum power, where it has to shut down and start up
again, which is what brings the rules that follow a start-up into play. Two environment variables
price the reserves (`TUDPS_RESCOST`, the cost of both reserves) and the
reactive power (`TUDPS_QCOST`), since a unit solved standalone has no system
constraint whose multipliers would do it; a third one, `TUDPS_FIXMOD`, fixes
one modulation variable out of the given number, so that the two Solvers are
compared on a unit whose operating rules are partly decided already.

Two further `BlockSolverConfig` are provided for the study of the solve
times at the horizons at which the unit commitment is solved:
`BSCfg-nuc-lim.txt` is `BSCfg-nuc.txt` with a time limit on the MILP solver,
so that an instance that it cannot close still returns the pair of bounds it
has reached, and `BSCfg-nuc-dponly.txt` attaches the dynamic programming
Solver alone, so that the optimal schedule can be inspected without paying
for the MILP solve.

A makefile is also provided that builds the executable including the
`MILPSolver` module and the `UCBlock` module (and, obviously, the core
SMS++ library).


## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
