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
#    Note: this file tries to respect the principles of Modern CMake,         #
#          keep it in mind when editing it.                                   #
#                                                                             #
#                              Niccolo' Iardella                              #
#                                Donato Meoli                                 #
#                         Dipartimento di Informatica                         #
#                             Universita' di Pisa                             #
# --------------------------------------------------------------------------- #
cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.12)
project(SMS++_tests
        VERSION 0.5.3
        DESCRIPTION "SMS++ tests"
        HOMEPAGE_URL https://gitlab.com/smspp/tests
        LANGUAGES C CXX)

# Find out if it's being called by the umbrella.
get_directory_property(hasParent PARENT_DIRECTORY)

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
        add_link_options("/WHOLEARCHIVE")
        add_link_options("/FORCE:MULTIPLE")
    else () # Unix
        if (APPLE)
            add_link_options("-Wl,-all_load")
        else ()
            add_link_options("-Wl,--whole-archive,--allow-multiple-definition")
        endif ()
    endif ()
endif ()

# ----- Policies ----------------------------------------------------------- #
# Introduced in 3.19 to handle arbitrary characters in test names
# See: https://cmake.org/cmake/help/latest/policy/CMP0110.html
if (POLICY CMP0110)
    cmake_policy(SET CMP0110 NEW)
endif ()

# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    find_package(MCFClass)
    find_package(MCFBlock)
    find_package(MMCFBlock)
    find_package(UCBlock)
    find_package(BundleSolver)
    find_package(LagrangianDualSolver)
    find_package(MILPSolver)
    find_package(BinaryKnapsackBlock)
endif ()

# ----- Subdirectories ------------------------------------------------------ #
add_subdirectory(BendersBFunction)
add_subdirectory(BinaryKnapsackBlock)
add_subdirectory(BoxSolver)
add_subdirectory(CapacitatedFacilityLocation)
add_subdirectory(LagBFunction)
add_subdirectory(LagrangianDualSolver_Box)
add_subdirectory(LagrangianDualSolver_MMCF)
add_subdirectory(LagrangianDualSolver_UC)
add_subdirectory(MCF_MILP)
add_subdirectory(MMCFBlock)
add_subdirectory(PolyhedralFunction)
add_subdirectory(PolyhedralFunctionBlock)
add_subdirectory(ThermalUnitBlock_Solver)
add_subdirectory(Write-Read)

# --------------------------------------------------------------------------- #
