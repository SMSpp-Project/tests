##############################################################################
################################# makefile ###################################
##############################################################################
#                                                                            #
#   makefile of Graph                                                        #
#                                                                            #
#   Input:  $(CC)     = compiler command                                     #
#           $(SW)     = compiler options                                     #
#           $(MMGDIR) = the directory where the source is                    #
#                                                                            #
#   Output: $(MMGOBJ) = the final object(s) / library                        #
#           $(MMGLIB) = external libraries + -L< libdirs >                   #
#           $(MMGH)   = the .h files to include	(apart from the OPT* ones)   #
#           $(MMGINC) = the -I$(include directories)                         #
#                                                                            #
#                              Antonio Frangioni                             #
#                         Dipartimento di Informatica                        #
#                             Universita' di Pisa                            #
#                                                                            #
##############################################################################

# clean - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

clean::
	rm -f $(MMGDIR)/*.o $(MMGDIR)/*~

# macros to be exported - - - - - - - - - - - - - - - - - - - - - - - - - - -

MMGOBJ = $(MMGDIR)/Graph.o
MMGLIB =
MMGH   = $(MMGDIR)/Graph.h $(OPTUxH)
MMGINC = -I$(MMGDIR) $(OPTUxINC)

# dependencies: every .o from its .C + every recursively included .h- - - - -

$(MMGOBJ): $(MMGDIR)/Graph.C $(MMGH) 
	$(CC) -c $*.C -o $@ $(SW)

############################ End of makefile #################################
