##############################################################################
#                                                                            #
#   makefile of mnetgen                                                      #
#                                                                            #
##############################################################################

# basic directory
DIR = .

# module
NAME = mnetgen

# libraries
LIB = -lm

# switches
SW = -O -w

# compiler
CC = clang++

# default target- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

default: $(NAME)

# clean - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

clean::
	rm -f $(DIR)/*.o $(DIR)/*~ $(NAME)

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

SOURCE = mnetgen.C

# main module

$(NAME): $(SOURCE)
	$(CC) $(SOURCE) -o $(NAME) $(LIB) $(SW)

############################ End of makefile #################################
