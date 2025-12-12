##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for LukFiBlock)                                        #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = LukFi_test

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

# core SMS++
SMS++SDR = ../../SMS++
include $(SMS++SDR)/lib/makefile-inc

# BundleSolver
BNDSLVSDR = ../../BundleSolver
include $(BNDSLVSDR)/makefile-c

# LukFiBlock
LukFiSDR = ../../LukFiBlock
include $(LukFiSDR)/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(LukFiOBJ) $(SMS++OBJ) $(BNDSLVOBJ)

# libraries
MLIB =  $(SMS++LIB) $(BNDSLVLIB) $(LukFiLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/main.o
	$(CC) -o $(DIR)/$(NAME) $(DIR)/main.o $(MOBJ) $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(BNDSLVINC) $(LukFiINC)

# includes
MH =    $(SMS++H) $(BNDSLVH) $(LukFiH)

# compile command

$(DIR)/main.o: $(DIR)/main.cpp $(MH)
	$(CC) -c $*.cpp -o $@ $(MINC) $(SW)

############################ End of makefile ################################
