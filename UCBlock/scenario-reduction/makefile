##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for UC scenario reduction)                            #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = UCScenarioGenerator

# basic directory
DIR = .

# common stuff
include ../../makefile_common

# common_utils (shared test helpers in tests/); override the
# makefile_common defaults since this test lives two levels below tests/
CMMNUSDR = ../..
CMMNUOBJ = $(CMMNUSDR)/common_utils.o
CMMNUH   = $(CMMNUSDR)/common_utils.h
CMMNUINC = -I$(CMMNUSDR)

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

# UCBlock
UCBckDIR = ../../../UCBlock
include $(UCBckDIR)/lib/makefile

# StochasticBlock
StcBlkSDR = ../../../StochasticBlock
include $(StcBlkSDR)/makefile-s

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(CMMNUOBJ) $(StcBlkOBJ) $(UCBckOBJ) $(SMS++OBJ)

# libraries
MLIB =  $(SMS++LIB) $(UCBckLIB) $(StcBlkLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/$(NAME).o
	$(CC) -o $(DIR)/$(NAME) $+ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(UCBckINC) $(StcBlkINC) $(CMMNUINC)

# includes
MH =    $(SMS++H) $(UCBckH) $(StcBlkH) $(CMMNUH)

# compile command

$(DIR)/$(NAME).o: $(DIR)/$(NAME).cpp $(MH)
	$(CC) -c $(DIR)/$(NAME).cpp -o $@ $(MINC) $(SW)

$(CMMNUOBJ): $(CMMNUSDR)/common_utils.cpp $(CMMNUH) $(MH)
	$(CC) -c $(CMMNUSDR)/common_utils.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
