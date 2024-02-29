##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for BinaryKnapsackBlock and DPBinaryKnapsackSolver)    #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = BinaryKnapsackBlock_test

# basic directory
DIR = .

# debug switches
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -Wno-enum-compare
# debug switches with address sanitizer and extra pedantic warning
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer -Wpedantic -Wextra -Wno-unused-parameter -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare
# production switches with address sanitizer
#SW = -O3 -std=c++17 -DNDEBUG -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -Wno-enum-compare
# production switches
SW = -O3 -std=c++17 -ferror-limit=1 -DNDEBUG -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare

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

# BKBlock
BKBkSDR = ../../BinaryKnapsackBlock
include $(BKBkSDR)/makefile-c

# define input macros for MILPSolver complete makefile, then include it
# use the makefile-s version since the SMS++ stuff has been defined already
MILPSSDR = $(BKBkSDR)/../MILPSolver
include $(MILPSSDR)/makefile-s

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(BKBkOBJ) $(MILPSOBJ)

# libraries
MLIB =  $(BKBkLIB) $(MILPSLIB) $(libCPLEXLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o
	$(CC) -o $(DIR)/$(NAME) $(DIR)/test.o $(MOBJ) $(MLIB) $(SW)

# dependencies: every .o from its .C + every recursively included .h- - - - -

# include directives
MINC =  $(BKBkINC) $(MILPSINC)

# includes
MH =    $(BKBkH)

# compile command

$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $*.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
