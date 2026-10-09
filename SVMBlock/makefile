##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for SVMBlock)                                          #
#                                                                            #
#                                Donato Meoli                                #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = SVM_test

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

# SVMBlock
SVMBkSDR = ../../SVMBlock
include $(SVMBkSDR)/makefile-s

# BundleSolver (which includes MILPSolver)
BNDSLVSDR = ../../BundleSolver
include $(BNDSLVSDR)/makefile-s

# LagrangianDualSolver
LgDSLVSDR = ../../LagrangianDualSolver
include $(LgDSLVSDR)/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SVMBkOBJ) $(BNDSLVOBJ) $(LgDSLVOBJ) $(SMS++OBJ) $(CMMNUOBJ)

# libraries
MLIB =  $(SVMBkLIB) $(BNDSLVLIB) $(LgDSLVLIB) $(SMS++LIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(SVMBkINC) $(BNDSLVINC) $(LgDSLVINC) $(CMMNUINC)

# includes
MH =    $(SMS++H) $(SVMBkH) $(BNDSLVH) $(LgDSLVH) $(CMMNUH)

# compile command

$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
