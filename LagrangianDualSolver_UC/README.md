# test/LagrangianDualSolver_UC

A tester which provides initial tests for `LagrangianDualSolver`,
`LagBFunction`, any `CDASolver` able to handle `C05Function` in the
`Objective` (such as `BundleSolver`), any `CDASolver` able to handle
Linear Programs (such as `MILPSolver` and its derived classes
`CPXMILPSolver` and `SCIPMILPSolver`), the `UCBlock` set of `Block`
for Unit-Commitment problems, as well as for quite a lot of the
mechanics of the "core" SMS++ library.

This executable, given the filename of a netCDF file containing the
description of a `UCBlock` (instance of the Unit-Commitment problem),
solves its Lagrangian Dual (with the continuous relaxation of the
subproblems solved, i.e., basically the Linear Dual) with a
`LagrangianDualSolver` and its continuous relaxation with a
`:MILPSolver`, comparing the results (and printing the running time).

The usage of the executable is the following:

       ./LDS_UC_test UC-file [BSC-file]
       BSC-file: BlockSolverConfig description [BSPar.txt]

The test are supposed to be ran on the pure-thermal "academic" UC
instances available at

http://groups.di.unipi.it/optimize/Data/UC.html

(translated in netCDF with the translator available in the `UCBlock`
repo). The layout assumes that the instances are in the sub-folder
"data", that can be symlinked from the `UCBlock` repo such as in

    ln -s ../../UCBlock/netCDF_files/UC_Data/T-Ramp data

A makefile is also provided that builds the executable including the
`LagrangianDualSolver` module, the `BundleSolver` module and all its
dependencies, in particular `MILPSolver` together of course with the
core SMS++ library, and the `UCBlock` module.

It must be noted, however, that since the Unit Commitment problem has
integer variables, the Lagrangian Dual and the original integer
problem are not equivalent (the Lagrangian Dual is a relaxation) and
the Lagrangian Dual and the continuous relaxation of the integer
problem are not equivalent (the Lagrangian Dual is a better
relaxation, i.e., it provides tighter = larger lower bounds).

There are only two cases in which the Lagrangian Dual and the
continuous relaxation of the integer problem are equivalent:

1. if the ThermalUnitBlock subproblems (currently the only ones
   that contain integer variables) are solved only in the sense
   of their continuous relaxation, as in this case the two
   relaxations coincide whatever formulation of the
   ThermalUnitBlock is used;

2. if the ThermalUnitBlock subproblems (currently the only ones
   that contain integer variables) are solved to integer optimality
   but the "DP formulation" of the ThermalUnitBlock is used in
   the :MILPSolver together with "Perspective Cuts" (P/C).

This is why diferent BlockConfig [TUBCfg\*] and BlockSolverConfig
[TUBSCfg\*] are provided for the ThermalUnitBlock subproblems:

- TUBCfg-DP.txt is supposed to go together with either
  TUBSCfg-DP.txt or TUBSCfg-ILP.txt: it forces the "DP formulation"
  to be used and P/Cs to be separated, which means that the
  :MILPSolver provides the same strong bound as the Lagrangian Dual
  where the ThermalUnitBlock are solved to integer optimality (this
  should be done by the more efficient ThermalUnitDPSolver, but
  using a :MILPSolver ran to integer optimality is mathematically
  equivalent and it may be nice to further test the equivalence
  between the "abstract" and the "physical" solution);

- TUBCfg-LP.txt is supposed to go together with TUBSCfg-CLP.txt;
  there the user can arbitrarily change which formulation is used,
  comprised whether or not P/Cs are separated, as the two bounds
  will be equivalent anyway; yet, note that "large" formulations
  like SU, SD and especially DP may make both the :MILPSolver
  applied to the whole UC and these applied to the ThermalUnitBlock
  subproblems rather slow.

Since the "DP formulation" is rather large, the batch file
batches/batch-acad-s is provided for the first case that only
solves the set of "academic" UC instances that are appropriately
small. Instead, batches/batch-acad is meant for the second case
and solves them all. 

Both batches automatically copy the right TUBCfg\*.txt and
TUBSCfg\*.txt for the intended tests to suceed.


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
