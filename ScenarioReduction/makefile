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

# compiler
CC = clang++

# common flags
COMMON_SW = -std=c++20 -ferror-limit=1 -Wno-enum-compare -DCLANG_1200_0_32_27_PATCH

# debug switches
SW_DEBUG = -g3 -glldb -fno-inline $(COMMON_SW)

# production switches
SW_RELEASE = -O3 $(COMMON_SW) -DNDEBUG

# default target
default: release

# debug target
debug: SW = $(SW_DEBUG)
debug: $(DIR)/$(NAME)

# release target
release: SW = $(SW_RELEASE)
release: $(DIR)/$(NAME)

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
	rm -f $(DIR)/*.o $(DIR)/*~ $(DIR)/$(NAME)
	rm -f $(DIR)/cache/*.nc4

# phony targets
.PHONY: debug release clean

############################ End of makefile #################################