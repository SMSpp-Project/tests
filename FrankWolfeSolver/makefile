##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for FrankWolfeSolver)                                  #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = FWS_test

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

# MCFBlock (its makefile-c brings in the core SMS++)
MCFBkSDR = ../../MCFBlock
include $(MCFBkSDR)/makefile-c

# MCFClass, needed by MCFClassSolver (now a separate module)
MCFC_NO_PATHS = 0
libMCFClDIR = ../../MCFClassSolver/MCFClass
include $(libMCFClDIR)/lib/makefile-inc

# MCFClassSolver (the MCFSolver<...> LMO of each MCFBlock sub-Block); deps
# (SMS++, MCFBlock, MCFClass) are defined above
MCFCSSDR = ../../MCFClassSolver
include $(MCFCSSDR)/makefile

# MILPSolver (the core SMS++ is already defined by MCFBlock above)
MILPSSDR = ../../MILPSolver
include $(MILPSSDR)/makefile-s

# FrankWolfeSolver (the core SMS++ is already defined above)
FWSlvSDR = ../../FrankWolfeSolver
include $(FWSlvSDR)/makefile-s

# UCBlock (ThermalUnitBlock + ThermalUnitDPSolver for the TUB test); the core
# SMS++ is already defined above, this brings the UCBlock objects/includes
UCBckDIR = ../../UCBlock
include $(UCBckDIR)/lib/makefile

# the common_utils macros ($(CMMNU*)) and build rule come from makefile_common

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(CMMNUOBJ) $(MCFBkOBJ) $(MCFCSOBJ) $(libMCFClOBJ) $(MILPSOBJ) \
        $(FWSlvOBJ) $(UCBckOBJ)

# libraries
MLIB =  $(MCFBkLIB) $(libMCFClLIB) $(MILPSLIB) $(FWSlvLIB) $(UCBckLIB)

# include directives
MINC =  $(MCFBkINC) $(MCFCSINC) $(libMCFClINC) $(MILPSINC) $(FWSlvINC) \
        $(UCBckINC) $(CMMNUINC)

# includes
MH =    $(MCFBkH) $(MCFCSH) $(MILPSH) $(FWSlvH) $(UCBckH) $(CMMNUH)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o
	$(CC) -o $(DIR)/$(NAME) $(DIR)/test.o $(MOBJ) $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# compile command

$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $*.cpp -o $@ $(MINC) $(SW)

############################ End of makefile ################################
