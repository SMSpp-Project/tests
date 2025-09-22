##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile for UCBlock Scenario Generator                                  #
#                                                                            #
##############################################################################

# generator name
NAME = UCBlockScenarioGenerator

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

# default target - build generator in release mode
default: SW = $(SW_RELEASE)
default: $(DIR)/$(NAME)

# debug target - build generator in debug mode
debug: SW = $(SW_DEBUG)
debug: $(DIR)/$(NAME)

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

# main library
MOBJ = $(SMS++OBJ) $(UCBckOBJ) $(MILPSOBJ) $(TSSBkOBJ)
MLIB = $(SMS++LIB) $(UCBckLIB) $(MILPSLIB) $(TSSBkLIB)

# linking
$(DIR)/$(NAME): $(DIR)/$(NAME).o $(MOBJ)
	$(CC) -o $@ $^ $(MLIB) $(SW)

# include directives
MINC = $(SMS++INC) $(UCBckINC) $(MILPSINC) $(TSSBkINC)

# includes
MH = $(SMS++H) $(UCBckH) $(MILPSH) $(TSSBkH)

# compile command
$(DIR)/$(NAME).o: $(DIR)/$(NAME).cpp $(MH)
	$(CC) -c $(DIR)/$(NAME).cpp -o $@ $(MINC) $(SW)

# clean target
clean::
	rm -f $(DIR)/*.o $(DIR)/*~ $(DIR)/$(NAME)

# phony targets
.PHONY: default debug clean

############################ End of makefile #################################