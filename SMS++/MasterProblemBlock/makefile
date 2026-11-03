##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for MasterProblemBlock)                                #
#                                                                            #
#                                Donato Meoli                                #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = MasterProblemBlock_test

# basic directory
DIR = .

# common stuff
TSTROOT = ../..
include ../../makefile_common

# define & include the necessary modules- - - - - - - - - - - - - - - - - - -
# if a module is not used in the current configuration, just comment out the
# corresponding include line
# each module outputs some macros to be used here:
# *OBJ is the final object(s) / library
# *LIB is the external libraries + -L< libdirs >
# *H   is the list of all include files
# *INC is the -I< include directories >

# define input macros for SMS++ complete makefile, then include it
SMS++SDR = ../../../SMS++
include $(SMS++SDR)/lib/makefile-c

# MILPSolver, without core SMS++
MILPSSDR = ../../../MILPSolver
include $(MILPSSDR)/makefile-s

# BDMinQuadSolver, the Solver of the third entry of MPBSolvers.txt
BDMQSkSDR = ../../../BDMinQuadSolver
include $(BDMQSkSDR)/makefile-s

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BDMQSkOBJ) $(SMS++OBJ) $(MILPSOBJ) $(CMMNUOBJ)

# libraries
MLIB =  $(BDMQSkLIB) $(SMS++LIB) $(MILPSLIB)

$(DIR)/$(NAME): $(DIR)/test.o $(MOBJ)
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(MILPSINC) $(BDMQSkINC) $(CMMNUINC)

# includes
MH =    $(SMS++H) $(MILPSH) $(BDMQSkH) $(CMMNUH)

# compile command


$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
