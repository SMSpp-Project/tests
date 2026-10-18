# tests/MILPSolver/QuadFunction

The `QuadFunction` a `:MILPSolver` is asked to handle, in the Objective and in
the Constraint: a tester which provides very comprehensive tests for any
`CDASolver` able to handle Quadratic Programs (such as `MILPSolver` and its
derived classes `CPXMILPSolver`, `SCIPMILPSolver`, `GRBMILPSolver`,
`HiGHSMILPSolver`, `MOSEKMILPSolver`, `DAQPMILPSolver`, `QPOASESMILPSolver` and
`ClarabelMILPSolver`).

This executable, given the filename of a LP file containing a quadratic model,
read the corresponding problem using the function `AbstractBlock::read_lp` and 
represent it in an `AbstractBlock` (LPBlock). Additionally, it accepts a second 
argument, which is a character indicating whether you are interested in solving
the continuous relaxation (C) or the integer version (I).

The Solver of a BlockSolverConfig are then attached to LPBlock and the model
is solved by each of them, which is checked against the others and against
the known optimal value of the problem, if any, a Solver stopped by its time
limit being held to the interval of its bounds (`SolveAll()` of
`common_utils`). The BlockSolverConfig is `LPPar_<class>_<type>.txt` if there
is one for the class of the model, given by the folder of its file
(`ObjC_ConsL` is the class `CL`), and `LPPar_<type>.txt` otherwise: the class
`CL` has its own, since next to the `SCIPMILPSolver` of `LPPar_C.txt` and
`LPPar_I.txt` it attaches the `:MILPSolver` for convex QPs, which refuse the
quadratic constraints of `LC` and the nonconvex objective of `QL`:
`MOSEKMILPSolver` and `ClarabelMILPSolver` on the continuous relaxation,
`MOSEKMILPSolver` on the integer version. The dense ones, `DAQPMILPSolver`
and `QPOASESMILPSolver`, load the whole Hessian and do not scale to the
largest models of `CL` (up to 15000 variables): `batchCL_C_dense` gives them
`LPPar_CL_C-dense.txt` with `-S` on the models with up to about 2000, but
`QPLib_8845`, on which the active set of DAQP cycles. A Solver
that the build has not is left out, and said.

A list of all the available QP instances can be found in the file 
`available_instance.txt`, along with their optimal values for both the continuous
and integer versions. All instances are sourced from the `QPLIB` library 
(https://qplib.zib.de/) and are categorized by objective and constraint complexity 
into different folders. The notation used aligns with the documentation of the online 
library:
 - L represents linear functions;
 - C represents quadratic convex functions;
 - Q represents quadratic non-convex functions.

You can also download and run any specific instance you would like, as long as the 
correct path to the instance is specified. Additionally, you can provide the 
known optimal value for the instance being tested with -r.

The usage of the executable is the following:

       ./MILPSolver_QuadFunction_test [-t I|C] [-r opt] LP-file
       -t, --type <I|C>    solve the integer or the relaxed version [I]
       -r, --ref <value>   optimal value of the model [the one in
                           available_instances.txt, if any]

In the batches folder, different batch files are provided to run all the available
instances with a specific degree of complexity. All these passing is a good sign 
that no regressions have been done for the tested modules.

A makefile is also provided that builds the executable including 
and all its dependencies, in particular `MILPSolver` (and, obviously, 
the core SMS++ library).

## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

- **Enrico Calandrini**  
  Dipartimento di Informatica  
  Università di Pisa


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
