# --------------------------------------------------------------------------- #
#    Main CMake file for SMS++ system tests                                   #
#                                                                             #
#    This file allows one to build the tests using CMake.                     #
#    Tests that should be supported by ctest executable must be added         #
#    with the add_test() command.                                             #
#    To do so, you can use the following commands:                            #
#                                                                             #
#        $ cmake -S <source-path> -B <build-path>                             #
#        $ cmake --build <build-path>                                         #
#                                                                             #
#                                Donato Meoli                                 #
#                         Dipartimento di Informatica                         #
#                             Universita' di Pisa                             #
# --------------------------------------------------------------------------- #
cmake_minimum_required(VERSION 3.24)
cmake_policy(VERSION 3.15)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeriveVersion.cmake)
smspp_derive_version(SMSPP_MODULE_VERSION)

project(SMS++_tests
        VERSION ${SMSPP_MODULE_VERSION}
        DESCRIPTION "SMS++ tests"
        HOMEPAGE_URL https://gitlab.com/smspp/tests
        LANGUAGES C CXX)

# Find out if it's being called by the umbrella.
get_directory_property(hasParent PARENT_DIRECTORY)

# This adds the cmake directory to the module search paths,
# allowing us to use our modules.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# The per-module CI test-label map and the smspp_label_tests() helper that each
# test directory calls to tag its tests (see cmake/TestLabels.cmake).
include(TestLabels)

# Utility function to standardize installation and runtime dependency handling
# for SMS++ tests.
function(smspp_install target_name)
    smspp_copy_runtime(${target_name})

    #[[install(TARGETS ${target_name}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})]]
endfunction()

# Register a batch file as a ctest driving the given executable: every suite
# runs its batteries this way, each of them being a family of instances that
# the tester cross-checks all the Solver of its BlockSolverConfig on, so that
# they are independent and can be run, selected and timed one by one.
#
# A battery that the Solver of one tester do not cover alone runs every other
# tester that applies to its instances as well: those are named here after the
# first one and reach the battery as the arguments after it, in the order they
# are given and each of them only if it is in the build, an empty argument
# standing for the one that is not, which the battery then leaves out.
function(add_batch_test exe batch)
    set(_others "")
    foreach (_t IN LISTS ARGN)
        if (TARGET ${_t})
            # a generator expression, since a tester of another suite or of a
            # module is built in another directory of the tree
            list(APPEND _others "$<TARGET_FILE:${_t}>")
        else ()
            list(APPEND _others "")
        endif ()
    endforeach ()
    add_test(NAME ${exe}/${batch}
             COMMAND ${SMSpp_BASH}
                     "${CMAKE_CURRENT_SOURCE_DIR}/${batch}"
                     "$<TARGET_FILE:${exe}>" ${_others}
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

# ----- Settings ------------------------------------------------------------ #
# This creates an ENABLE_TESTING option (default: ON) and enables the testing
# using the ctest executable.
# See: https://cmake.org/cmake/help/latest/manual/ctest.1.html
include(CTest)

# ----- Tests --------------------------------------------------------------- #
# One directory per module the tests are posed on, named after it, since a
# suite here is what a module cannot test by itself: what it needs is either
# the cross-check machinery of common_utils or a module that is not one of its
# dependencies. The testers of the objects of the core library are therefore
# grouped under SMS++, those of a Block under the name of its module.
add_subdirectory(SMS++)

add_subdirectory(MILPSolver)

add_subdirectory(MCFBlock)

add_subdirectory(BinaryKnapsackBlock)

# before the suites whose batteries run its tester of the scenario reduction,
# which has to be a target when they are configured [see add_batch_test()]
add_subdirectory(TwoStageStochasticBlock)

add_subdirectory(CapacitatedFacilityLocationBlock)

add_subdirectory(MMCFBlock)

add_subdirectory(UCBlock)
add_subdirectory(InvestmentBlock)
add_subdirectory(MultiStageStochasticBlock)

add_subdirectory(LukFiBlock)

add_subdirectory(SVMBlock)
add_subdirectory(SingleFlowDCRBlock)
add_subdirectory(SATBlock)
add_subdirectory(SatellitesBlock)
add_subdirectory(MultiKnapsackAssignBlock)
add_subdirectory(GeneralizedAssignmentBlock)
add_subdirectory(HQPBlock)

# the program that imports the C++20 module of every SMS++ module in the
# build, which are there when SMSpp_BUILD_MODULE is ON [see SMS++]
if (SMSpp_BUILD_MODULE)
    add_subdirectory(import_modules)
endif ()

# the one tester that is posed on no Block in particular: it reads whichever
# two the configurations name and compares the two formulations
add_subdirectory(compare_formulations)

# The classes of the modules register themselves in the factories from
# static objects, which nothing here names: a shared library is kept by
# what its module asks of the linker, while an archive has to be taken
# whole [see SMSppModule.cmake of SMS++], for all the suites above, where
# the modules have been found.
smspp_keep_modules()

# --------------------------------------------------------------------------- #
