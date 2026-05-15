##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for PolyhedralFunctionBlock)                           #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = PolyhedralFunctionBlock_test

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

# not necessary, BundleSolver does that already
# MILPSolver
#MILPSSDR = ../../../MILPSolver
#include $(MILPSSDR)/makefile

# common_utils stuff - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CMMNUOBJ = ../common_utils.o
CMMNUH = ../common_utils.h
CMMNUINC = -I..

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BNDSLVOBJ) $(SMS++OBJ) $(CMMNUOBJ)

# libraries
MLIB =  $(BNDSLVLIB) $(SMS++LIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(BNDSLVINC) $(CMMNUINC)

# includes
MH =    $(SMS++H) $(BNDSLVH) $(CMMNUH)

# compile command

$(CMMNUOBJ): $(CMMNUH) $(SMS++H) ../common_utils.cpp
	$(CC) -c ../common_utils.cpp -o $@ $(MINC) $(SW)

$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
