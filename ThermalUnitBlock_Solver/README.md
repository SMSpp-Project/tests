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
[cuts](cuts) that solve different sets of the available single-unit
instances with some of the (many) different formulations supported
by `ThermalUnitBlock`, in particular without and with "Perspective
Cuts".

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
