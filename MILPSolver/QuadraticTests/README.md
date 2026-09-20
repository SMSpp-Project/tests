# test/QuadraticTests

A tester which provides very comprehensive tests for any  `CDASolver` 
able to handle Quadratic Programs (such as `MILPSolver` and its derived 
classes`CPXMILPSolver` , `SCIPMILPSolver` , `GRBMILPSolver` and 
`HiGHSMILPSolver`).

This executable, given the filename of a LP file containing a quadratic model,
read the corresponding problem using the function `AbstractBlock::read_lp` and 
represent it in an `AbstractBlock` (LPBlock). Additionally, it accepts a second 
argument, which is a character indicating whether you are interested in solving
the continuous relaxation (C) or the integer version (I).

An appropriate `CDASolver` is then attached to LPBlock, which
can be any `Solver` capable of handling Quadratic Programs (say, some derived 
class of `MILPSolver` such as `GRBMILPSolver`) and the model is solved. 

After all this is done, the results (termination status and objective
value, if applicable) are compared with a known optimal solution of the problem.

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
known optimal value for the instance being tested as a third argument.

The usage of the executable is the following:

       ./quad_test LP-file [type opt]
       type: wheter solving the integer or relaxed version [I]
       opt: optimal solution of the model [0]

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
