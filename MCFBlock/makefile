##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for MCFBlock)                                          #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = MCFBlock_test

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

# MCFClass, needed by MCFClassSolver
MCFC_NO_PATHS = 0
libMCFClDIR = ../../MCFClassSolver/MCFClass
include $(libMCFClDIR)/lib/makefile-inc

# MCFClassSolver, assuming all dependencies (SMS++, MCFBlock, MCFClass) are
# there already (comment out if not required)
MCFCSSDR = ../../MCFClassSolver
include $(MCFCSSDR)/makefile

# MCFLemonSolver, assuming all dependencies (MCFBlock) are there already
# (comment out if not required)
# note: -std=c++20 is required if this is included
MCFLESDR = ../../MCFLemonSolver
include $(MCFLESDR)/makefile

# MILPSolver
MILPSSDR = ../../MILPSolver
include $(MILPSSDR)/makefile-s

# FrankWolfeSolver, for the two Frank-Wolfe testers only ("make fw" and
# "make fwmods"); its objects are not in MOBJ, so the tester of the Block
# does not link it
FWSlvSDR = ../../FrankWolfeSolver
include $(FWSlvSDR)/makefile-s

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(CMMNUOBJ) $(MCFCSOBJ) $(libMCFClOBJ) $(MCFLEOBJ) $(MILPSOBJ) \
        $(MCFBkOBJ)

# libraries
MLIB =  $(MCFBkLIB) $(libMCFClLIB) $(MCFLELIB) $(MILPSLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o
	$(CC) -o $(DIR)/$(NAME) $(DIR)/test.o $(MOBJ) $(MLIB) $(SW)
#	-Wl,-ld_classic
# emergency linker option for certain settings on macOS

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(MCFBkINC) $(MCFCSINC) $(libMCFClINC) $(MCFLEINC) $(MILPSINC) \
        $(CMMNUINC)

# includes
MH =    $(MCFBkH) $(MCFCSH) $(MCFLEH) $(MILPSH) $(CMMNUH)

# compile command


$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(SW)

# the Frank-Wolfe decomposition on this Block- - - - - - - - - - - - - - - - -
# the generic tester lives in the root of the suites, since the leaf Block it
# reads is whichever one the -fw configurations name; the one of the
# Modification of the feasible region is of this Block, hence it is here

fw: SW = $(SW_RELEASE)
fw: $(DIR)/MCFBlock_FW_test

fwmods: SW = $(SW_RELEASE)
fwmods: $(DIR)/MCFBlock_FWmods_test

$(DIR)/MCFBlock_FW_test: $(MOBJ) $(FWSlvOBJ) $(DIR)/fw_test.o
	$(CC) -o $@ $(DIR)/fw_test.o $(MOBJ) $(FWSlvOBJ) $(MLIB) $(FWSlvLIB) \
	      $(SW)

$(DIR)/MCFBlock_FWmods_test: $(MOBJ) $(FWSlvOBJ) $(DIR)/test_fw_mods.o
	$(CC) -o $@ $(DIR)/test_fw_mods.o $(MOBJ) $(FWSlvOBJ) $(MLIB) \
	      $(FWSlvLIB) $(SW)

$(DIR)/fw_test.o: ../fw_test.cpp $(MH) $(FWSlvH)
	$(CC) -c ../fw_test.cpp -o $@ $(MINC) $(FWSlvINC) $(SW)

$(DIR)/test_fw_mods.o: $(DIR)/test_fw_mods.cpp $(MH) \
                       $(FWSlvH)
	$(CC) -c $(DIR)/test_fw_mods.cpp -o $@ $(MINC) $(FWSlvINC) $(SW)

.PHONY: fw fwmods

clean::
	rm -f $(DIR)/MCFBlock_FW_test $(DIR)/MCFBlock_FWmods_test

############################ End of makefile #################################
