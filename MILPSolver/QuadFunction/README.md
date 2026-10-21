# tests/MILPSolver/QuadFunction

The `QuadFunction` a `:MILPSolver` is asked to handle, in the Objective and in
the Constraint: a tester which provides very comprehensive tests for any
`CDASolver` able to handle Quadratic Programs (such as `MILPSolver` and its
derived classes `CPXMILPSolver`, `SCIPMILPSolver`, `GRBMILPSolver`,
`HiGHSMILPSolver`, `MOSEKMILPSolver`, `DAQPMILPSolver`, `QPOASESMILPSolver`,
`ClarabelMILPSolver`, `ProxQPMILPSolver`, `PIQPMILPSolver`, `OSQPMILPSolver`,
`QPMADMILPSolver` and `QuadProgMILPSolver`).

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
`MOSEKMILPSolver`, `ClarabelMILPSolver`, `PIQPMILPSolver` (with its sparse
backend, `PIQPCfg-C.txt`) and `OSQPMILPSolver` on the continuous relaxation,
`MOSEKMILPSolver` on the integer version. The dense ones, `DAQPMILPSolver`,
`QPOASESMILPSolver`, `ProxQPMILPSolver`, `QPMADMILPSolver` and
`QuadProgMILPSolver`, load the whole
Hessian and do not scale to the largest models of `CL` (up to 15000
variables), where `ProxQPMILPSolver`, which has no time limit, does not end
even with its sparse backend: `batchCL_C_dense` gives them
`LPPar_CL_C-dense.txt` with `-S` on the models with up to about 2000, but
`QPLib_8845`, on which the active set of DAQP cycles. `QPMADMILPSolver` and
`QuadProgMILPSolver` need a positive definite Hessian, and that of these
models is singular, hence they have it regularized by 1e-6 (`QPMADCfg-C.txt`
and `QuadProgCfg-C.txt`). A Solver that the build has
not is left out, and said.

A list of all the available QP instances can be found in the file 
`available_instance.txt`, along with their optimal values for both the continuous
and integer versions. All instances are sourced from the `QPLIB` library 
(https://qplib.zib.de/) and are categorized by objective and constraint complexity 
into different folders. The notation used aligns with the documentation of the online 
library:
 - L represents linear functions;
 - C represents quadratic convex functions;
 - Q represents quadratic non-convex functions.

Two more families of convex QPs come from the test sets of qpbenchmark
(https://github.com/qpsolvers), under the Apache License 2.0, Copyright
2022 Stéphane Caron, 2023-2024 Inria, 2026 CNRS: `MPC` holds the 64
problems of model predictive control in robotics of `mpc_qpbenchmark`,
and `MarosMeszaros` 61 of the 62 of the dense subset of the Maros-Meszaros
test set of `maros_meszaros_qpbenchmark` (at most 1000 variables and 1000
constraints), `VALUES` being left out since its Hessian is not positive
semidefinite; the other 76 are up to some tens of MB each as LP files.
`qpb2lp.py` writes them as LP files from the .npz and .mat of those
repositories, and writes the others too, given their files. Their values
in `available_instances.txt` (of the continuous problem only) are those on
which `GRBMILPSolver` and `MOSEKMILPSolver` agree, or that of
`GRBMILPSolver` where `MOSEKMILPSolver` fails or is farther from the value
of the Maros-Meszaros report, and that of the report for `HS268` and
`S268`. `batchMPC_C` and `batchMM_C` solve them with `LPPar_CL_C.txt`,
`batchMPC_C_dense` and `batchMM_C_dense` with `LPPar_CL_C-dense.txt`, both
given with `-S`, since these folders are not named after a class.

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
