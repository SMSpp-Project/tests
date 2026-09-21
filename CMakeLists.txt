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
cmake_minimum_required(VERSION 3.21)
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
    if (WIN32 AND BUILD_SHARED_LIBS AND NOT DEFINED ENV{CONDA_BUILD})
        # On Windows with shared libraries enabled copies all runtime DLL
        # dependencies next to the executable in the build directory
        # (POST_BUILD), using CMake's TARGET_RUNTIME_DLLS generator expression
        add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target_name}>
                $<TARGET_FILE_DIR:${target_name}>
                COMMAND_EXPAND_LISTS
                COMMENT "Copying runtime DLLs next to ${target_name}.exe")
    endif ()

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
    if (UNIX)
        add_test(NAME ${exe}/${batch}
                 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/${batch}
                         ${CMAKE_CURRENT_BINARY_DIR}/${exe} ${_others}
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    elseif (WIN32)
        add_test(NAME ${exe}/${batch}
                 # using Git for Windows that includes Git Bash
                 COMMAND "C:/Program Files/Git/bin/bash.exe"
                         "${CMAKE_CURRENT_SOURCE_DIR}/${batch}"
                         "$<TARGET_FILE:${exe}>" ${_others}
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif ()
endfunction()

# ----- Settings ------------------------------------------------------------ #
# This creates an ENABLE_TESTING option (default: ON) and enables the testing
# using the ctest executable.
# See: https://cmake.org/cmake/help/latest/manual/ctest.1.html
include(CTest)

# Since we are using the block factory, objects from linked libraries
# may wrongly appear unused, and by default the linker does not include them,
# so we have to force the linking.
if (BUILD_SHARED_LIBS)
    if (UNIX AND (NOT APPLE))
        add_link_options("-Wl,--no-as-needed")
    endif ()
else ()
    if (MSVC)
        add_link_options("/WHOLEARCHIVE" "/FORCE:MULTIPLE")
    else () # Unix
        if (APPLE)
            add_link_options("-Wl,-all_load")
        else ()
            add_link_options("-Wl,--whole-archive,--allow-multiple-definition")
        endif ()
    endif ()
endif ()

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

add_subdirectory(CapacitatedFacilityLocationBlock)

add_subdirectory(MMCFBlock)

add_subdirectory(UCBlock)
add_subdirectory(InvestmentBlock)
add_subdirectory(TwoStageStochasticBlock)
add_subdirectory(MultiStageStochasticBlock)

add_subdirectory(LukFiBlock)

add_subdirectory(SVMBlock)
add_subdirectory(SingleFlowDCRBlock)

# the one tester that is posed on no Block in particular: it reads whichever
# two the configurations name and compares the two formulations
add_subdirectory(compare_formulations)

# --------------------------------------------------------------------------- #
