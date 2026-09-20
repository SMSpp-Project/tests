##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for UCBlock)                                           #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = UCBlock_test

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

# LagrangianDualSolver
LgDSLVSDR = ../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# not necessary, BundleSolver does that already
# MILPSolver
#MILPSSDR = ../../MILPSolver
#include $(MILPSSDR)/makefile-c

# define input macros for UCBlock makefile, then include it
UCBckDIR = ../../UCBlock
include $(UCBckDIR)/lib/makefile

# FrankWolfeSolver, for the Frank-Wolfe tester only ("make fw"); its objects
# are not in MOBJ, so the tester of the Block does not link it
FWSlvSDR = ../../FrankWolfeSolver
include $(FWSlvSDR)/makefile-s

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ) $(SMS++OBJ) $(CMMNUOBJ)

# libraries
MLIB =  $(SMS++LIB) $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC) $(CMMNUINC)

# includes
MH =    $(SMS++H) $(BNDSLVH) $(LgDSLVH) $(UCBckH) $(CMMNUH)

# compile command


$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(SW)

# the dynamic programming solver of the unit against a :MILPSolver- - - - - -

tudps: SW = $(SW_RELEASE)
tudps: $(DIR)/TUDPS_test

$(DIR)/TUDPS_test: $(MOBJ) $(DIR)/test_tudps.o
	$(CC) -o $@ $(DIR)/test_tudps.o $(MOBJ) $(MLIB) $(SW)

$(DIR)/test_tudps.o: $(DIR)/test_tudps.cpp $(MH)
	$(CC) -c $(DIR)/test_tudps.cpp -o $@ $(MINC) $(SW)

# the Frank-Wolfe decomposition on this Block- - - - - - - - - - - - - - - - -
# the generic tester lives in the root of the suites, since the leaf Block it
# reads is whichever one the configurations of FW/ name

fw: SW = $(SW_RELEASE)
fw: $(DIR)/UCBlock_FW_test

$(DIR)/UCBlock_FW_test: $(MOBJ) $(FWSlvOBJ) $(DIR)/fw_test.o
	$(CC) -o $@ $(DIR)/fw_test.o $(MOBJ) $(FWSlvOBJ) $(MLIB) $(FWSlvLIB) \
	      $(SW)

$(DIR)/fw_test.o: ../fw_test.cpp ../fw_test_common.h $(MH) $(FWSlvH)
	$(CC) -c ../fw_test.cpp -o $@ $(MINC) $(FWSlvINC) $(SW)

.PHONY: tudps fw

clean::
	rm -f $(DIR)/TUDPS_test $(DIR)/UCBlock_FW_test

############################ End of makefile #################################
