##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for ScenarioReduction)                                #
#                                                                            #
##############################################################################

# module name
NAME = ScenarioReduction_test

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

# StochasticBlock (includes DiscreteScenarioSet)
StcBlkSDR = ../../StochasticBlock
include $(StcBlkSDR)/makefile

# CapacitatedFacilityLocationBlock (includes ScenarioReductionSolver)
CFLBkSDR = ../../CapacitatedFacilityLocationBlock
include $(CFLBkSDR)/makefile-s

# MILPSolver (needed for scenario reduction and two-stage solving)
MILPSSDR = ../../MILPSolver
include $(MILPSSDR)/makefile-c

# TwoStageStochasticBlock
TSSBkSDR = ../../twostagestochasticblock
include $(TSSBkSDR)/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files for our test
TEST_OBJ = $(DIR)/test.o $(DIR)/CFL_DSS.o

# object files from modules
MOBJ =  $(StcBlkOBJ) $(CFLBkOBJ) $(MILPSOBJ) $(SMS++OBJ) $(MCFBkOBJ) $(TSSBkOBJ)

# libraries
MLIB =  $(StcBlkLIB) $(CFLBkLIB) $(MILPSLIB) $(SMS++LIB) $(MCFBkLIB)

# main target
$(DIR)/$(NAME): $(MOBJ) $(TEST_OBJ)
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(StcBlkINC) $(CFLBkINC) $(MILPSINC) $(MCFBkINC) $(TSSBkINC)

# includes
MH =    $(SMS++H) $(StcBlkH) $(CFLBkH) $(MILPSH) $(MCFBkH) $(TSSBkH)

# compile commands

$(DIR)/test.o: $(DIR)/test.cpp $(DIR)/CFL_DSS.h $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(SW)

$(DIR)/CFL_DSS.o: $(DIR)/CFL_DSS.cpp $(DIR)/CFL_DSS.h $(MH)
	$(CC) -c $(DIR)/CFL_DSS.cpp -o $@ $(MINC) $(SW)

# clean target
clean::
	rm -f $(DIR)/*.o $(DIR)/$(NAME)

############################ End of makefile #################################