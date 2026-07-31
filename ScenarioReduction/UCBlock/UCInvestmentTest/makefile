##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for the UC investment scenario reduction)             #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = uc_investment_test

# basic directory
DIR = .

# common stuff
include ../../../makefile_common

# define & include the necessary modules- - - - - - - - - - - - - - - - - - -
# if a module is not used in the current configuration, just comment out the
# corresponding include line
# each module outputs some macros to be used here:
# *OBJ is the final object(s) / library
# *LIB is the external libraries + -L< libdirs >
# *H   is the list of all include files
# *INC is the -I< include directories >

# define input macros for SMS++ complete makefile, then include it
SMS++SDR = ../../../../SMS++
include $(SMS++SDR)/lib/makefile-c

# UCBlock
UCBckDIR = ../../../../UCBlock
include $(UCBckDIR)/lib/makefile

# ScenarioReductionSolver (brings TwoStageStochasticBlock, StochasticBlock and
# CapacitatedFacilityLocationBlock in input)
SRSkSDR = ../../../../ScenarioReductionSolver
include $(SRSkSDR)/makefile-s

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SRSkOBJ) $(UCBckOBJ) $(SMS++OBJ)

# libraries
MLIB =  $(SMS++LIB) $(UCBckLIB) $(SRSkLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/UCInvestmentReductionTest.o
	$(CC) -o $(DIR)/$(NAME) $+ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(UCBckINC) $(SRSkINC)

# includes
MH =    $(SMS++H) $(UCBckH) $(SRSkH)

# compile command

$(DIR)/UCInvestmentReductionTest.o: $(DIR)/UCInvestmentReductionTest.cpp $(MH)
	$(CC) -c $(DIR)/UCInvestmentReductionTest.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
