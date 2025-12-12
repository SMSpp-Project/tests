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
cmake_minimum_required(VERSION 3.16)
cmake_policy(VERSION 3.15)
project(SMS++_tests
        VERSION 0.6.0
        DESCRIPTION "SMS++ tests"
        HOMEPAGE_URL https://gitlab.com/smspp/tests
        LANGUAGES C CXX)

# Find out if it's being called by the umbrella.
get_directory_property(hasParent PARENT_DIRECTORY)

# This adds the cmake directory to the module search paths,
# allowing us to use our modules.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# ----- Settings ------------------------------------------------------------ #
# This creates an ENABLE_TESTING option (default: ON) and enables the testing
# using the ctest executable.
# See: https://cmake.org/cmake/help/latest/manual/ctest.1.html
include(CTest)

# Since we are using the block factory, objects from linked libraries
# may wrongly appear unused, and by default the linker does not include them,
# so we have to force the linking.
if (BUILD_SHARED_LIBS)
    if (MSVC)
        add_link_options("/OPT:NOREF")
    elseif (UNIX AND (NOT APPLE))
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
add_subdirectory(MCF_MILP)

add_subdirectory(BinaryKnapsackBlock)

add_subdirectory(CapacitatedFacilityLocation)

add_subdirectory(MMCFBlock)
add_subdirectory(LagrangianDualSolver_MMCF)

add_subdirectory(LagrangianDualSolver_UC)
add_subdirectory(ThermalUnitBlock_Solver)

add_subdirectory(LukFiBlock)


add_subdirectory(compare_formulations)

# --------------------------------------------------------------------------- #
