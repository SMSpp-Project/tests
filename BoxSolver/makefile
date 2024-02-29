##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for BoxSolver)                                         #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# module name
NAME = BoxSolver_test

# basic directory
DIR = .

# debug switches
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare
# debug switches with address sanitizer and extra pedantic warning
#SW = -g3 -glldb -fno-inline -std=c++17 -ferror-limit=1 -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer -Wpedantic -Wextra -Wno-unused-parameter -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare
# production switches with address sanitizer
#SW = -O3 -std=c++17 -DNDEBUG -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -DCLANG_1200_0_32_27_PATCH -Wno-enum-compare
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

# define input macros for SMS++ complete makefile, then include it
SMS++SDR = ../../SMS++
include $(SMS++SDR)/lib/makefile-inc

# MILPSolver, without core SMS++
MILPSSDR = ../../MILPSolver
include $(MILPSSDR)/makefile-s

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SMS++OBJ) $(MILPSOBJ)

# libraries
# note the --force_link, that is used to ensure that BoxSolver (which is
# never explicitly mentioned in test.cpp if SMSpp_ensure_load() is not
# called) is properly linked with the final executable and therefore
# appears in the Solver factory. The ugly C++ mangled name may not be
# portable: if this does not work on your system run
#
#    nm libSMS++.a -j | grep BoxSolver
#
# in wherever folder libSMS++.a lives (typically SMS++/lib in the root
# umbrella folder) and use one of the many names that will come out of
# it. Be aware, though, that not all names may work and you may get a
#
#    what(): BoxSolver not present in Solver factory
#
# error, so a bit of experiment may be required. If this is too complicated
# or does not work, uncomment SMSpp_ensure_load() and be sure that
# BoxSolver.h is #include-d in test.cpp
MLIB =  $(SMS++LIB) $(MILPSLIB) \
	--force-link=__ZTVN17SMSpp_di_unipi_it9BoxSolverE

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o
	$(CC) -o $(DIR)/$(NAME) $^ $(MLIB) $(SW)

# dependencies: every .o from its .C + every recursively included .h- - - - -

# include directives
MINC =  $(SMS++INC) $(MILPSINC)

# includes
MH =    $(SMS++H) $(MILPSH)

# compile command

$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(SW)

############################ End of makefile #################################
