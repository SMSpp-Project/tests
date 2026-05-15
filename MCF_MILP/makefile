##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for MCF_MILP)                                          #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = MCF_MILP_test

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

# not necessary, MCFBlock does that already
# define input macros for SMS++ complete makefile, then include it
#SMS++SDR = ../..SMS++
#include $(SMS++SDR)/lib/makefile-inc

# MCFBlock
MCFBkSDR = ../../MCFBlock
include $(MCFBkSDR)/makefile-c

# LEMONSolver, assuming all dependencies (MCFBlock) are there already
# (comment out if not required)
# note: -std=c++20 is required if this is included
MCFLESDR = ../../LEMONSolver
include $(MCFLESDR)/makefile

# MILPSolver
MILPSSDR = ../../MILPSolver
include $(MILPSSDR)/makefile-s

# common_utils stuff - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CMMNUOBJ = ../common_utils.o
CMMNUH = ../common_utils.h
CMMNUINC = -I..

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(MCFLEOBJ) $(MILPSOBJ) $(MCFBkOBJ) $(CMMNUOBJ)

# libraries
MLIB =  $(MCFBkLIB) $(MCFLELIB) $(MILPSLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o
	$(CC) -o $(DIR)/$(NAME) $(DIR)/test.o $(MOBJ) $(MLIB) $(SW)
#	-Wl,-ld_classic
# emergency linker option for certain settings on macOS

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(MCFBkINC) $(MCFLEINC) $(MILPSINC) $(CMMNUINC)

# includes
MH =    $(MCFBkH) $(MCFLEH) $(MILPSH) $(CMMNUH)

# compile command

$(CMMNUOBJ): $(CMMNUH) $(SMS++H) ../common_utils.cpp
	$(CC) -c ../common_utils.cpp -o $@ $(MINC) $(SW)

$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
