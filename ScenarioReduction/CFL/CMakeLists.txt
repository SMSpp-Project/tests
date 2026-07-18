# --------------------------------------------------------------------------- #
#    CMake file for tests/ScenarioReduction/CFL                               #
# --------------------------------------------------------------------------- #
#
# Builds the CFL scenario reduction test executable.
# Replaces the old test that used the sr_cfl hack.
#
# --------------------------------------------------------------------------- #

cmake_minimum_required( VERSION 3.20 )

# Force linking for static libraries to avoid the linker discarding symbols
if( BUILD_SHARED_LIBS )
    if( UNIX AND NOT APPLE )
        add_link_options( "-Wl,--no-as-needed" )
    endif()
else()
    if( MSVC )
        add_link_options( "/WHOLEARCHIVE" "/FORCE:MULTIPLE" )
    elseif( APPLE )
        add_link_options( "-Wl,-all_load" )
    else()
        add_link_options( "-Wl,--whole-archive,--allow-multiple-definition" )
    endif()
endif()

set( SR_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/.. )

# --------------------------------------------------------------------------- #
#  CFLScenarioGenerator                                                        #
# --------------------------------------------------------------------------- #
add_executable( CFLScenarioGenerator
    ${CMAKE_CURRENT_SOURCE_DIR}/CFLScenarioGenerator.cpp
)
target_link_libraries( CFLScenarioGenerator PRIVATE
    SMS++::ScenarioReductionSolver
    SMS++::StochasticBlock
    SMS++::TwoStageStochasticBlock
    SMS++::CapacitatedFacilityLocationBlock
    SMS++::MILPSolver
)
target_include_directories( CFLScenarioGenerator PRIVATE
    ${CMAKE_SOURCE_DIR}/ScenarioReductionSolver/include
    ${CMAKE_SOURCE_DIR}/StochasticBlock/include
    ${CMAKE_SOURCE_DIR}/TwoStageStochasticBlock/include
    ${CMAKE_SOURCE_DIR}/CapacitatedFacilityLocationBlock/include
    ${SR_ROOT}/include
)