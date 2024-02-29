##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for LagrangianDualSolver_UC)                           #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = LDS_UC_test

# basic directory
DIR = .

# debug switches
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -Wno-enum-compare
# debug switches with address sanitizer and extra pedantic warning
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer -Wpedantic -Wextra -Wno-unused-parameter -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare
# production switches with address sanitizer
#SW = -O3 -std=c++17 -DNDEBUG -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -Wno-enum-compare
# production switches
SW = -O3 -std=c++17 -DNDEBUG -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare

# linker switches
#LSW = -std=c++17 -ferror-limit=1

# compiler
CC = clang++

# default target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

default: $(DIR)/$(NAME)

# clean - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

clean::
	rm -f $(DIR)/*.o $(DIR)/*~ $(DIR)/$(NAME)

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

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BNDSLVOBJ) $(LgDSLVOBJ) $(UCBckOBJ) $(SMS++OBJ)

# libraries
MLIB =  $(SMS++LIB) $(BNDSLVLIB) $(LgDSLVLIB) $(UCBckLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .C + every recursively included .h- - - - -

# include directives
MINC =  $(SMS++INC) $(BNDSLVINC) $(LgDSLVINC) $(UCBckINC)

# includes
MH =    $(SMS++H) $(BNDSLVH) $(LgDSLVH) $(UCBckH)

# compile command

$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
