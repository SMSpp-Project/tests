##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for LagrangianDualSolver_UC)                           #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = MFDCRBlock_test

# basic directory
DIR = .

# common stuff
include ../makefile_common

# define & include the necessary modules- - - - - - - - - - - - - - - - - - -
# if a module is not used in the current configuration, just comment out the
# corresponding include line
# each module outputs some macros to be used here:
# *OBJ is the final object(s) / library
# *LIB is the external libraries + -L< libdirs >
# *H   is the list of all include files
# *INC is the -I< include directories >

# define input macros for SMS++ complete makefile, then include it
SMS++SDR = ../../SMS++
include $(SMS++SDR)/lib/makefile-c

# BundleSolver
BNDSLVSDR = ../../BundleSolver
include $(BNDSLVSDR)/makefile-s

# LagrangianDualSolver
LgDSLVSDR = ../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# not necessary, BundleSolver does that already
# MILPSolver
#MILPSSDR = ../../MILPSolver
#include $(MILPSSDR)/makefile-c

# define input macros for UCBlock makefile, then include it
SFDCRSDR = ../../SingleFlowDCRBlock
include $(SFDCRSDR)/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BNDSLVOBJ) $(LgDSLVOBJ) $(SFDCROBJ) $(SMS++OBJ)

# libraries
MLIB =  $(SMS++LIB) $(BNDSLVLIB) $(LgDSLVLIB) $(SFDCRRLIB) 

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o
	clang++ -o ./MFDCRBlock_test ../../SingleFlowDCRBlock/obj/*.o ../../BundleSolver/obj/BundleSolver.o ../../BundleSolver/obj/ParallelBundleSolver.o ../../BundleSolver/../MILPSolver/obj/MILPSolver.o ../../BundleSolver/../MILPSolver/obj/CPXMILPSolver.o ../../BundleSolver/../MILPSolver/obj/GRBMILPSolver.o ../../BundleSolver/NdoFiOracle/lib/libNDO.a ../../LagrangianDualSolver/obj/LagrangianDualSolver.o ../../LagrangianDualSolver/obj/PrimalProximalHeur.o ../../UCBlock/obj/BatteryUnitBlock.o ../../UCBlock/obj/DesignNetworkBlock.o ../../UCBlock/obj/DCNetworkBlock.o ../../UCBlock/obj/ECNetworkBlock.o ../../UCBlock/obj/HydroSystemUnitBlock.o ../../UCBlock/obj/HydroUnitBlock.o ../../UCBlock/obj/IntermittentUnitBlock.o ../../UCBlock/obj/NetworkBlock.o ../../UCBlock/obj/NuclearUnitBlock.o ../../UCBlock/obj/SlackUnitBlock.o ../../UCBlock/obj/ThermalUnitBlock.o ../../UCBlock/obj/ThermalUnitDPSolver.o ../../UCBlock/obj/ThermalUnitExtDPSolver.o ../../UCBlock/obj/UCBlock.o ../../UCBlock/obj/UnitBlock.o ../../SMS++/obj/AbstractBlock.o ../../SMS++/obj/BendersBFunction.o ../../SMS++/obj/BendersBlock.o ../../SMS++/obj/Block.o ../../SMS++/obj/BlockSolverConfig.o ../../SMS++/obj/BoxSolver.o ../../SMS++/obj/C05Function.o ../../SMS++/obj/ColRowSolution.o ../../SMS++/obj/ColVariable.o ../../SMS++/obj/ColVariableSolution.o ../../SMS++/obj/Configuration.o ../../SMS++/obj/Constraint.o ../../SMS++/obj/DataMapping.o ../../SMS++/obj/DQuadFunction.o ../../SMS++/obj/QuadFunction.o ../../SMS++/obj/FRealObjective.o ../../SMS++/obj/FRowConstraint.o ../../SMS++/obj/Function.o ../../SMS++/obj/LagBFunction.o ../../SMS++/obj/LinearFunction.o ../../SMS++/obj/Objective.o ../../SMS++/obj/OneVarConstraint.o ../../SMS++/obj/PolyhedralFunction.o ../../SMS++/obj/PolyhedralFunctionBlock.o ../../SMS++/obj/RBlockConfig.o ../../SMS++/obj/RowConstraintSolution.o ../../SMS++/obj/Solver.o ../../SMS++/obj/Solution.o ../../SMS++/obj/ThinComputeInterface.o ../../SMS++/obj/Variable.o test.o -L"/lib"  -L"/opt/homebrew/lib" -lnetcdf -L"/Users/lucamencarelli/Desktop/smspp-project/netcdf-cxx4-4.3.1/build/cxx4" -lnetcdf-cxx4      -L"/Users/lucamencarelli/Desktop/smspp-project/Osi-releases-0.108.11/Osi/src/OsiGrb/.libs" -lOsi -lOsiClp -L"/Users/lucamencarelli/Desktop/smspp-project/Osi-releases-0.108.11/Osi/src/OsiCpx/.libs" -lOsiCpx -lOsiGrb -L"/usr/local/include/coin/lib" -lClp -L"/usr/local/include/coin/lib" -lCoinUtils  -L"/Applications/CPLEX_Studio2211/cplex/lib/arm64_osx/static_pic" -lilocplex -lcplex  -L"/Library/gurobi1301/macos_universal2/lib" -lgurobi130  -L"/Applications/CPLEX_Studio2211/cplex/lib/arm64_osx/static_pic" -lilocplex -lcplex  -rpath "/lib" -L"/lib" -rpath "/lib" -L"/lib" -ltbb     -O3 -std=c++20 -ferror-limit=1 -Wno-enum-compare -DCLANG_1200_0_32_27_PATCH -DNDEBUG -Wno-deprecated-declarations

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(BNDSLVINC) $(LgDSLVINC) $(SFDCRINC)

# includes
MH =    $(SMS++H) $(BNDSLVH) $(LgDSLVH) $(SFDCRH)

# compile command

$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(SW) 

############################ End of makefile #################################
