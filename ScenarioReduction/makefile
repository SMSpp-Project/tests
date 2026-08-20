##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for ScenarioReduction)                                 #
#                                                                            #
#   The scenario reduction programs are built in the subdirectories: this    #
#   makefile just recurses into them.                                        #
#                                                                            #
#                              Antonio Frangioni                             #
#                          Dipartimento di Informatica                       #
#                              Universita' di Pisa                           #
#                                                                            #
##############################################################################

# subdirectories with a makefile
SUBDIRS = CFLBlock UCBlock UCBlock/UCInvestmentTest

# default target - build all subdirectories in release mode
default:
	for d in $(SUBDIRS) ; do $(MAKE) -C $$d || exit 1 ; done

# debug target - build all subdirectories in debug mode
debug:
	for d in $(SUBDIRS) ; do $(MAKE) -C $$d debug || exit 1 ; done

# clean target
clean::
	for d in $(SUBDIRS) ; do $(MAKE) -C $$d clean ; done

# phony targets
.PHONY: default debug clean

############################ End of makefile #################################
