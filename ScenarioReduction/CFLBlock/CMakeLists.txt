# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(CapacitatedFacilityLocationBlock)
    find_package(TwoStageStochasticBlock)
    # Solvers
    find_package(MILPSolver)
endif ()

# ----- Tests --------------------------------------------------------------- #
# CFLScenarioGenerator writes a TwoStageStochasticBlock file that the generic
# ScenarioReductionSolver_test program (ScenarioReductionSolver module) then reduces
# and solves; see run_cfl_tests for the generate->solve flow.
if (TARGET SMS++::CapacitatedFacilityLocationBlock
    AND TARGET SMS++::TwoStageStochasticBlock)

    add_executable(CFLScenarioGenerator
                   CFLScenarioGenerator.cpp ../../common_utils.cpp)
    target_include_directories(CFLScenarioGenerator PRIVATE ../..)
    target_link_libraries(CFLScenarioGenerator PRIVATE
                          SMS++::CapacitatedFacilityLocationBlock
                          SMS++::StochasticBlock
                          SMS++::TwoStageStochasticBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(CFLScenarioGenerator PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(CFLScenarioGenerator)

endif ()

# --------------------------------------------------------------------------- #
