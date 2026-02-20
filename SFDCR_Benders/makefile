##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for DCRBlock / MCFSolver)                              #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = SFDCRBlock_test

# basic directory
DIR = .

# debug switches
#SW = -g -glldb -fno-inline -std=c++17 -ferror-limit=1
# debug switches with address sanitizer and extra pedantic warning
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer -Wpedantic -Wextra -Wno-unused-parameter
# production switches
SW = -O3 -std=c++17 -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION 
#-DNDEBUG -DCLANG_1200_0_32_27_PATCH -ferror-limit=1 

# compiler
CC = clang++ -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION

# basic CPLEX directory
libCPLEXBSCDIR = /Applications/CPLEX_Studio2211/cplex/

# lib Cplex directory
libCPLEXINCDIR = $(libCPLEXBSCDIR)lib/arm64_osx/static_pic/

libCPLEXLIB = -L$(libCPLEXINCDIR) -lilocplex -lcplex $(libCPLEXEXTLIB)
libCPLEXINC = -I$(libCPLEXBSCDIR)include/

libOSIINC = -I/usr/local/include/coin
libOSILIB = -rpath /usr/local/lib/ -lOsiClp -lClp -lOsi -lCoinUtils -lOsiCpx

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

# SFDCRBlock
SFDCRSDR = ../../SingleFlowDCRBlock
include $(SFDCRSDR)/makefile-c

LAGRANGIANDR = ../../LagrangianDualSolver
BUNDLEDR = ../../BundleSolver

#LgDSLVSDR = $(LAGRANGIANDR)
#LgDSLVODR = $(LAGRANGIANDR)/obj/
#include $(LAGRANGIANDR)/makefile-c

libNDOSDR = ../../BundleSolver/NdoFiOracle
include ../../BundleSolver/NdoFiOracle/lib/makefile

BNDSLVSDR = $(BUNDLEDR)
BNDSLVODR = $(BUNDLEDR)
include $(BUNDLEDR)/makefile

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SFDCROBJ) 

# libraries
MLIB =  $(SFDCRLIB) -L"/Library/gurobi1001/macos_universal2/lib" -lgurobi100

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o  
	$(CC) -o $(DIR)/$(NAME) ../../MILPSolver/obj/*.o ../../LagrangianDualSolver/obj/*.o ../../BundleSolver/obj/*.o $(libNDOOBJ) $^ $(MLIB) $(libNDOINC) $(BNDSLVINC) $(libOSIINC) $(libOSILIB) $(libCPLEXLIB) $(SW) -lpthread -lm 

# dependencies: every .o from its .C + every recursively included .h- - - - -

# include directives
MINC =  $(SFDCRINC) -I"/Library/gurobi1001/macos_universal2/include"

# includes
MH =    $(SFDCRH)

# compile command

$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $*.cpp -o $@ $(MINC) -I../../LagrangianDualSolver/include/ -I../../BundleSolver/include/ -I../../MILPSolver/include/ -I$(libNDOSDR)/OPTUtils -I../../BundleSolver/NdoFiOracle/NDOSlver -I$(libNDOINC) -I$(BNDSLVINC) $(libOSIINC) $(libOSILIB) $(libCPLEXINC) $(SW)

############################ End of makefile #################################
