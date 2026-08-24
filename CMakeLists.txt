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
add_subdirectory(BoxSolver)
add_subdirectory(LagrangianDualSolver_Box)

add_subdirectory(LagBFunction)

add_subdirectory(PolyhedralFunction)
add_subdirectory(PolyhedralFunctionBlock)

add_subdirectory(QuadraticTests)

add_subdirectory(Write-Read)

add_subdirectory(BendersBFunction)
add_subdirectory(MCFBlock)

add_subdirectory(BinaryKnapsackBlock)

add_subdirectory(CapacitatedFacilityLocation)

add_subdirectory(MMCFBlock)

add_subdirectory(UCBlock)
add_subdirectory(ThermalUnitBlock_Solver)
add_subdirectory(InvestmentBlock)
add_subdirectory(TwoStageStochasticBlock)
add_subdirectory(MultiStageStochasticBlock)
add_subdirectory(ScenarioReduction)

add_subdirectory(LukFiBlock)

add_subdirectory(FrankWolfeSolver)


add_subdirectory(compare_formulations)

# --------------------------------------------------------------------------- #

add_subdirectory(BundleSolverML)
