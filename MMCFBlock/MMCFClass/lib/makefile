##############################################################################
################################# makefile ###################################
##############################################################################
#                                                                            #
#   makefile of libMMCF                                                      #
#                                                                            #
#   all modules are given in input                                           #
#                                                                            #
#   Input:  $(CC)        = compiler command                                  #
#           $(SW)        = compiler options                                  #
#           $(OPTUxH)    = the OPT****.h files to include                    #
#           $(OPTUxINC)  = the -I$( OPT****.h directory )                    #
#           $(libMMCFSDR) = the basic directory for the library              #
#                                                                            #
#   Output: $(libMMCFOBJ) = the libMMCF library (that must be built)	     #
#           $(libMMCFLIB) = the external libraries + -L< libdirs >           #
#           $(libMMCFH)   = all the .h files to include                      #
#           $(libMMCFINC) = all the -I$(include directories)	     	     #
#                                                                            #
#                              Antonio Frangioni                             #
#                         Dipartimento di Informatica                        #
#                             Universita' di Pisa                            #
#                                                                            #
##############################################################################

# define & include the necessary modules- - - - - - - - - - - - - - - - - - -
# if a module is not used in the current configuration, just comment out the
# corresponding include line
# each module outputs some macros to be used here:
# *OBJ is the final object/library
# *LIB external libraries + -L< libdirs >
# *H   is the list of all include files
# *INC is the -I< include directories >

# MMCFClass
MMFDIR = $(libMMCFSDR)/MMCFClas
include $(MMFDIR)/makefile

# MCFCplex
MMXDIR =  $(libMMCFSDR)/MMCFCplex
include $(MMXDIR)/makefile

# Graph
MMGDIR = $(libMMCFSDR)/Graph
include $(MMGDIR)/makefile


# macros to be exported - - - - - - - - - - - - - - - - - - - - - - - - - - -

# object files
libMMCFOBJ = $(MMGOBJ)\
             $(MMXOBJ)

# libraries
libMMCFLIB = $(MMGLIB)\
             $(MMXLIB)

# includes
libMMCFH  =  $(MMGH)\
             $(MMFH)\
             $(MMXH)\
             $(OPTUxH)


# include dirs
libMMCFINC = $(MMFINC)\
             $(MMXINC)\
             $(MMGINC)\
             $(OPTUxINC)


############################ End of makefile #################################
