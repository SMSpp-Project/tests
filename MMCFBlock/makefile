##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for MMCFBlock)                                         #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = MMCF_test

# basic directory
DIR = .

# debug switches
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare
# debug switches with address sanitizer and extra pedantic warning
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer -Wpedantic -Wextra -Wno-unused-parameter -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare
# production switches with address sanitizer
#SW = -O3 -std=c++17 -DNDEBUG -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare
# production switches
SW = -O3 -std=c++17 -DNDEBUG -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare

# compiler
CC = clang++

# default target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

all default: $(DIR)/$(NAME)

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


# OPT*** stuff
#OPTUxH = $(libMMCFSDR)OPTUtils/OPTUtils.h $(libMMCFSDR)OPTUtils/OPTtypes.h \
#	 $(libMMCFSDR)OPTUtils/OPTvect.h
#OPTUxINC = -I$(libMMCFSDR)OPTUtils

# core SMS++
SMS++SDR = ../../SMS++
include $(SMS++SDR)/lib/makefile-inc

# MILPSolver
MILPSSDR = ../../MILPSolver
include $(MILPSSDR)/makefile-s

# MMCFBlock
MMCFSDR = ../../MMCFBlock
include $(MMCFSDR)/makefile-s

# MMCFClass
libMMCFSDR = $(DIR)/MMCFClass
include $(libMMCFSDR)/lib/makefile-c

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(MMCFOBJ) $(MILPSOBJ) $(libMMCFOBJ) $(SMS++OBJ)

# libraries
MLIB =  $(SMS++LIB) $(MILPSLIB) $(MMCFLIB) $(libMMCFLIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/main.o
	$(CC) -o $(DIR)/$(NAME) $(DIR)/main.o $(MOBJ) $(MLIB) $(SW)

# dependencies: every .o from its .C + every recursively included .h- - - - -

# include directives
MINC =  $(libMMCFINC) $(SMS++INC) $(MILPSINC) $(MMCFINC) $(OPTUxINC)

# includes
MH =    $(SMS++H) $(MILPSH) $(MMCFH) $(libMMCFH) $(OPTUxH)

# compile command

$(DIR)/main.o: $(DIR)/main.cpp $(MH)
	$(CC) -c $*.cpp -o $@ $(MINC) $(SW)

############################ End of makefile ################################

