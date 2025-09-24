##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile for UCBlock Scenario Reduction Test                            #
#                                                                            #
##############################################################################

# executables
UC_TEST = uc_scenario_reduction_test
UC_GENERATOR = UCScenarioGenerator

# basic directory
DIR = .

# parent directory for includes
PARENT_DIR = ..

# compiler
CC = clang++

# common flags
COMMON_SW = -std=c++20 -ferror-limit=1 -Wno-enum-compare -DCLANG_1200_0_32_27_PATCH

# debug switches
SW_DEBUG = -g3 -glldb -fno-inline $(COMMON_SW)

# production switches
SW_RELEASE = -O3 $(COMMON_SW) -DNDEBUG

# default target - build test executable in release mode
default: SW = $(SW_RELEASE)
default: $(DIR)/$(UC_TEST)

# all target - build all executables in release mode
all: SW = $(SW_RELEASE)
all: $(DIR)/$(UC_TEST) $(DIR)/$(UC_GENERATOR)

# debug target - build test executable in debug mode
debug: SW = $(SW_DEBUG)
debug: $(DIR)/$(UC_TEST)

# generator target - build generator in release mode
generator: SW = $(SW_RELEASE)
generator: $(DIR)/$(UC_GENERATOR)

# define & include the necessary modules from parent makefile
SMS++SDR = ../../../SMS++
include $(SMS++SDR)/lib/makefile-inc

# UCBlock
UCBckDIR = ../../../UCBlock
include $(UCBckDIR)/lib/makefile

# MILPSolver
MILPSSDR = ../../../MILPSolver
include $(MILPSSDR)/makefile-s

# TwoStageStochasticBlock
TSSBkSDR = ../../../TwoStageStochasticBlock
include $(TSSBkSDR)/makefile-s

# CapacitatedFacilityLocationBlock (for ScenarioReductionSolver.h)
CFLBkSDR = ../../../CapacitatedFacilityLocationBlock
include $(CFLBkSDR)/makefile-s

# main library
MOBJ = $(UCBckOBJ) $(TSSBkOBJ) $(CFLBkOBJ) $(MILPSOBJ) $(SMS++OBJ)
MLIB = $(UCBckLIB) $(TSSBkLIB) $(CFLBkLIB) $(MILPSLIB) $(SMS++LIB)

# test object files
TEST_OBJS = $(DIR)/UCScenarioReductionTest.o $(DIR)/uc_test_main.o ../src/AbstractScenarioReductionTest.o

# generator object files
GEN_OBJS = $(DIR)/UCScenarioGenerator.o

# linking test executable
$(DIR)/$(UC_TEST): $(TEST_OBJS) $(MOBJ)
	$(CC) -o $@ $^ $(MLIB) $(SW)

# linking generator executable (with MILPS for validation)
$(DIR)/$(UC_GENERATOR): $(GEN_OBJS) $(MOBJ)
	$(CC) -o $@ $^ $(MLIB) $(SW)

# include directives (add parent include directory)
MINC = $(SMS++INC) $(UCBckINC) $(MILPSINC) $(TSSBkINC) $(CFLBkINC) -I../include

# includes
MH = $(SMS++H) $(UCBckH) $(MILPSH) $(TSSBkH)

# compile commands
$(DIR)/UCScenarioReductionTest.o: $(DIR)/UCScenarioReductionTest.cpp $(DIR)/UCScenarioReductionTest.h $(MH)
	$(CC) -c $(DIR)/UCScenarioReductionTest.cpp -o $@ $(MINC) $(SW)

$(DIR)/uc_test_main.o: $(DIR)/uc_test_main.cpp $(DIR)/UCScenarioReductionTest.h
	$(CC) -c $(DIR)/uc_test_main.cpp -o $@ $(MINC) $(SW)

../src/AbstractScenarioReductionTest.o: ../src/AbstractScenarioReductionTest.cpp ../include/AbstractScenarioReductionTest.h
	$(CC) -c ../src/AbstractScenarioReductionTest.cpp -o $@ $(MINC) $(SW)

$(DIR)/UCScenarioGenerator.o: $(DIR)/UCScenarioGenerator.cpp
	$(CC) -c $(DIR)/UCScenarioGenerator.cpp -o $@ $(MINC) $(SW)

# clean target
clean::
	rm -f $(DIR)/*.o $(DIR)/*~ $(DIR)/$(NAME)

# phony targets
.PHONY: default debug clean

############################ End of makefile #################################