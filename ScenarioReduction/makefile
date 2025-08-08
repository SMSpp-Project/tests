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

# MILPSolver (needed for scenario reduction)
MILPSSDR = ../../MILPSolver
include $(MILPSSDR)/makefile-c

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(StcBlkOBJ) $(CFLBkOBJ) $(MILPSOBJ) $(SMS++OBJ) $(MCFBkOBJ)

# libraries
MLIB =  $(StcBlkLIB) $(CFLBkLIB) $(MILPSLIB) $(SMS++LIB) $(MCFBkLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(StcBlkINC) $(CFLBkINC) $(MILPSINC) $(MCFBkINC)

# includes
MH =    $(SMS++H) $(StcBlkH) $(CFLBkH) $(MILPSH) $(MCFBkH)

# compile command

$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################