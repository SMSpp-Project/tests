##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for CapacitatedFacilityLocationBlock)                  #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = CFL_test

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
include $(SMS++SDR)/lib/makefile-inc

# CapacitatedFacilityLocationBlock
CFLBkSDR = ../../CapacitatedFacilityLocationBlock
include $(CFLBkSDR)/makefile-s

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

# MCFClass, needed by MCFClassSolver
MCFC_NO_PATHS = 0
libMCFClDIR = ../../MCFClassSolver/MCFClass
include $(libMCFClDIR)/lib/makefile-inc

# MCFClassSolver, assuming all dependencies (SMS++, MCFBlock, MCFClass) are
# there already (comment out if not required)
MCFCSSDR = ../../MCFClassSolver
include $(MCFCSSDR)/makefile

# MCFLemonSolver, assuming all dependencies (MCFBlock) are there already
# (comment out if not required)
# note: -std=c++20 is required if this is included
#MCFLESDR = ../../MCFLemonSolver
#include $(MCFLESDR)/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BNDSLVOBJ) $(LgDSLVOBJ) $(CFLBkOBJ) $(SMS++OBJ) $(MCFBkOBJ) \
        $(MCFCSOBJ) $(libMCFClOBJ) $(MCFLEOBJ) $(CMMNUOBJ)

# libraries
MLIB =  $(BNDSLVLIB) $(LgDSLVLIB) $(CFLBkLIB) $(SMS++LIB) $(MCFBkLIB) \
        $(libMCFClLIB) $(MCFLELIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(BNDSLVINC) $(LgDSLVINC) $(CFLBkINC) $(MCFBkINC) \
        $(MCFCSINC) $(libMCFClINC) $(MCFLEINC) $(CMMNUINC)

# includes
MH =    $(SMS++H) $(BNDSLVH) $(LgDSLVH) $(CFLBkH) $(MCFBkH) $(MCFCSH) \
        $(MCFLEH) $(CMMNUH)

# compile command


$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(SW)

# the generator of the scenarios of this Block- - - - - - - - - - - - - - - -
# it writes the TwoStageStochasticBlock that the battery of the reduction then
# solves; the two stochastic modules are only linked by it

StcBlkSDR = ../../StochasticBlock
include $(StcBlkSDR)/makefile-s

TSSBkSDR = ../../TwoStageStochasticBlock
include $(TSSBkSDR)/makefile

scenarios: SW = $(SW_RELEASE)
scenarios: $(DIR)/CFLScenarioGenerator

$(DIR)/CFLScenarioGenerator: $(MOBJ) $(StcBlkOBJ) $(TSSBkOBJ) \
                             $(DIR)/CFLScenarioGenerator.o
	$(CC) -o $@ $(DIR)/CFLScenarioGenerator.o $(MOBJ) $(StcBlkOBJ) \
	      $(TSSBkOBJ) $(MLIB) $(StcBlkLIB) $(SW)

$(DIR)/CFLScenarioGenerator.o: $(DIR)/CFLScenarioGenerator.cpp $(MH) \
                               $(StcBlkH) $(TSSBkH)
	$(CC) -c $(DIR)/CFLScenarioGenerator.cpp -o $@ $(MINC) $(StcBlkINC) \
	      $(TSSBkINC) $(SW)

.PHONY: scenarios

clean::
	rm -f $(DIR)/CFLScenarioGenerator

############################ End of makefile #################################
