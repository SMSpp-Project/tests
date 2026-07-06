##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for BoxSolver)                                         #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = BoxSolver_test

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

# MILPSolver, without core SMS++
MILPSSDR = ../../MILPSolver
include $(MILPSSDR)/makefile-s

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SMS++OBJ) $(MILPSOBJ) $(CMMNUOBJ)

# libraries
# note: linking the SMS++ object files directly (lib/makefile-c above)
# rather than the static library pulls in SMS++/obj/BoxSolver.o
# unconditionally, so BoxSolver registers in the Solver factory without any
# --force-link trick (it is never explicitly mentioned in test.cpp)
MLIB =  $(SMS++LIB) $(MILPSLIB)

$(DIR)/$(NAME): $(DIR)/test.o $(MOBJ)
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(MILPSINC) $(CMMNUINC)

# includes
MH =    $(SMS++H) $(MILPSH) $(CMMNUH)

# compile command


$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
