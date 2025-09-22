##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for ScenarioReduction)                                 #
#                                                                            #
##############################################################################

# Tests are built in subdirectories

# scenario test subdirectories
CFL_DIR = CFL
UC_DIR = UCBlock

# basic directory
DIR = .

# compiler
CC = clang++

# common flags
COMMON_SW = -std=c++20 -ferror-limit=1 -Wno-enum-compare -DCLANG_1200_0_32_27_PATCH

# debug switches
SW_DEBUG = -g3 -glldb -fno-inline $(COMMON_SW)

# production switches
SW_RELEASE = -O3 $(COMMON_SW) -DNDEBUG

# default target - build all test executables in subdirectories (release mode)
default:
	$(MAKE) -C $(CFL_DIR)
	$(MAKE) -C $(UC_DIR)

# debug target - build all test executables in debug mode
debug:
	$(MAKE) -C $(CFL_DIR) debug
	$(MAKE) -C $(UC_DIR) debug

# No object files or linking in this directory - tests are built in subdirectories

# clean target
clean::
	rm -f $(DIR)/src/*.o $(DIR)/src/*~
	rm -f $(DIR)/include/*~
	rm -f $(DIR)/cache/*.nc4
	$(MAKE) -C $(CFL_DIR) clean
	$(MAKE) -C $(UC_DIR) clean

# phony targets
.PHONY: default debug clean

############################ End of makefile #################################