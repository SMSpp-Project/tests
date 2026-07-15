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

# --------------------------------------------------------------------------- #
#  cfl_impl_error_test : implementation error (fix reduced y~*, re-solve on N)  #
# --------------------------------------------------------------------------- #

add_executable( cfl_impl_error_test
    ${CMAKE_CURRENT_SOURCE_DIR}/CFLImplementationErrorTest.cpp
    ${SR_ROOT}/src/CSSCScenarioReductionSolver.cpp
    ${SR_ROOT}/src/ScenarioReductionSolver.cpp
)

target_link_libraries( cfl_impl_error_test PRIVATE
    SMS++::StochasticBlock
    SMS++::TwoStageStochasticBlock
    SMS++::CapacitatedFacilityLocationBlock
    SMS++::MILPSolver
)

target_include_directories( cfl_impl_error_test PRIVATE
    ${SR_ROOT}/include      # FIRST: our headers win over any same-named library ones
    ${CMAKE_SOURCE_DIR}/StochasticBlock/include
    ${CMAKE_SOURCE_DIR}/TwoStageStochasticBlock/include
    ${CMAKE_SOURCE_DIR}/CapacitatedFacilityLocationBlock/include
)

if( WIN32 AND BUILD_SHARED_LIBS AND NOT DEFINED ENV{CONDA_BUILD} )
    add_custom_command( TARGET cfl_impl_error_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:cfl_impl_error_test>
            $<TARGET_FILE_DIR:cfl_impl_error_test>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs"
    )
endif()

# --------------------------------------------------------------------------- #
#  cfl_impl_error_np_test : implementation error without penalty             #
# --------------------------------------------------------------------------- #

add_executable( cfl_impl_error_np_test
    ${CMAKE_CURRENT_SOURCE_DIR}/CFLImplementationErrorNoPenaltyTest.cpp
    ${SR_ROOT}/src/ScenarioReductionSolver.cpp
)

target_link_libraries( cfl_impl_error_np_test PRIVATE
    SMS++::StochasticBlock
    SMS++::TwoStageStochasticBlock
    SMS++::CapacitatedFacilityLocationBlock
    SMS++::MILPSolver
)

target_include_directories( cfl_impl_error_np_test PRIVATE
    ${SR_ROOT}/include      # FIRST: our headers win over any same-named library ones
    ${CMAKE_SOURCE_DIR}/StochasticBlock/include
    ${CMAKE_SOURCE_DIR}/TwoStageStochasticBlock/include
    ${CMAKE_SOURCE_DIR}/CapacitatedFacilityLocationBlock/include
)

if( WIN32 AND BUILD_SHARED_LIBS AND NOT DEFINED ENV{CONDA_BUILD} )
    add_custom_command( TARGET cfl_impl_error_np_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:cfl_impl_error_np_test>
            $<TARGET_FILE_DIR:cfl_impl_error_np_test>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs"
    )
endif()

# --------------------------------------------------------------------------- #
#  cfl_oos_test : out-of-sample test (kept for later reuse; fix y*, eval on an  #
#  optional independent reference set -T)                                       #
# --------------------------------------------------------------------------- #

add_executable( cfl_oos_test
    ${CMAKE_CURRENT_SOURCE_DIR}/CFLOutOfSampleTest.cpp
    ${SR_ROOT}/src/CSSCScenarioReductionSolver.cpp
    ${SR_ROOT}/src/ScenarioReductionSolver.cpp
)

target_link_libraries( cfl_oos_test PRIVATE
    SMS++::StochasticBlock
    SMS++::TwoStageStochasticBlock
    SMS++::CapacitatedFacilityLocationBlock
    SMS++::MILPSolver
)

target_include_directories( cfl_oos_test PRIVATE
    ${SR_ROOT}/include      # FIRST: our headers win over any same-named library ones
    ${CMAKE_SOURCE_DIR}/StochasticBlock/include
    ${CMAKE_SOURCE_DIR}/TwoStageStochasticBlock/include
    ${CMAKE_SOURCE_DIR}/CapacitatedFacilityLocationBlock/include
)

if( WIN32 AND BUILD_SHARED_LIBS AND NOT DEFINED ENV{CONDA_BUILD} )
    add_custom_command( TARGET cfl_oos_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:cfl_oos_test>
            $<TARGET_FILE_DIR:cfl_oos_test>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs"
    )
endif()

# --------------------------------------------------------------------------- #
#  CTest integration : moved to the generic scenario_reduction_solve binary   #
#  (built in UC/CMakeLists.txt); see run_cfl_tests.sh for the generate->solve #
#  flow. The old cfl_scenario_reduction_test-based smoke tests were removed   #
#  together with that binary (Option B Step 4).                              #
# --------------------------------------------------------------------------- #