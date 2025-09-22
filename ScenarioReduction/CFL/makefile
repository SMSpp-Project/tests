##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile for CFL Scenario Reduction Test                                 #
#                                                                            #
##############################################################################

# executables
CFL_TEST = cfl_scenario_reduction_test
CFL_GENERATOR = CFLScenarioGenerator

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

# default target - build CFL test executable in release mode
default: SW = $(SW_RELEASE)
default: $(DIR)/$(CFL_TEST)

# all target - build all executables in release mode
all: SW = $(SW_RELEASE)
all: $(DIR)/$(CFL_TEST) $(DIR)/$(CFL_GENERATOR)

# debug target - build CFL test executable in debug mode
debug: SW = $(SW_DEBUG)
debug: $(DIR)/$(CFL_TEST)

# define & include the necessary modules - same as parent makefile
# these provide *OBJ, *LIB, *H, and *INC macros

# SMS++
SMS++SDR = ../../../SMS++
include $(SMS++SDR)/lib/makefile-inc

# # StochasticBlock
# StcBlkSDR = ../../../StochasticBlock
# include $(StcBlkSDR)/makefile

# TwoStageStochasticBlock
TSSBkSDR = ../../../TwoStageStochasticBlock
include $(TSSBkSDR)/makefile-s

# CapacitatedFacilityLocationBlock
CFLBkSDR = ../../../CapacitatedFacilityLocationBlock
include $(CFLBkSDR)/makefile-s

# MILPSolver
MILPSSDR = ../../../MILPSolver
include $(MILPSSDR)/makefile-s

# object files from modules (same as parent)
MOBJ = $(TSSBkOBJ) $(CFLBkOBJ) $(MILPSOBJ) $(SMS++OBJ) 

# libraries (same as parent)
MLIB = $(TSSBlkLIB) $(CFLBkLIB) $(MILPSLIB) $(SMS++LIB)

# object files for CFL test
CFL_TEST_OBJ = $(DIR)/cfl_test_main.o $(DIR)/CFLScenarioReductionTest.o ../src/AbstractScenarioReductionTest.o

# linking CFL test
$(DIR)/$(CFL_TEST): $(CFL_TEST_OBJ) $(MOBJ)
	$(CC) -o $@ $^ $(MLIB) $(SW)

# linking generator
$(DIR)/$(CFL_GENERATOR): $(DIR)/$(CFL_GENERATOR).o $(MOBJ)
	$(CC) -o $@ $^ $(MLIB) $(SW)

# include directives (same as parent)
MINC = -I../include $(SMS++INC) $(CFLBkINC) $(MILPSINC) $(TSSBkINC)

# includes (same as parent)
MH = $(SMS++H) $(CFLBkH) $(MILPSH) $(TSSBkH)

# compile commands
$(DIR)/cfl_test_main.o: $(DIR)/cfl_test_main.cpp $(DIR)/CFLScenarioReductionTest.h $(MH)
	$(CC) -c $< -o $@ $(MINC) $(SW)

$(DIR)/CFLScenarioReductionTest.o: $(DIR)/CFLScenarioReductionTest.cpp $(DIR)/CFLScenarioReductionTest.h $(MH)
	$(CC) -c $< -o $@ $(MINC) $(SW)

# compile abstract base class
../src/AbstractScenarioReductionTest.o: ../src/AbstractScenarioReductionTest.cpp ../include/AbstractScenarioReductionTest.h $(MH)
	$(CC) -c $< -o $@ $(MINC) $(SW)

# compile generator
$(DIR)/$(CFL_GENERATOR).o: $(DIR)/$(CFL_GENERATOR).cpp $(MH)
	$(CC) -c $< -o $@ $(MINC) $(SW)

# clean target
clean::
	rm -f $(DIR)/*.o $(DIR)/*~
	rm -f $(DIR)/$(CFL_TEST) $(DIR)/$(CFL_GENERATOR)
	rm -f ../src/*.o

# phony targets
.PHONY: default all debug clean

############################ End of makefile #################################