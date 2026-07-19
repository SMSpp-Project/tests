# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(UCBlock)
    find_package(TwoStageStochasticBlock)
    # Solvers
    find_package(ScenarioReductionSolver)
    find_package(MILPSolver)
endif ()

# ----- Tests --------------------------------------------------------------- #
# UCScenarioGenerator writes a scenario file that the generic
# ScenarioReductionSolver_test program (ScenarioReductionSolver module) then reduces
# and solves; see run_uc_test.sh for the generate->solve flow.
# It links SMS++::StochasticBlock, so require it too: a build that enables
# UCBlock but not StochasticBlock (e.g. the ucblock module CI) must skip it.
if (TARGET SMS++::UCBlock AND TARGET SMS++::StochasticBlock)

    add_executable(UCScenarioGenerator
                   UCScenarioGenerator.cpp ../../common_utils.cpp)
    target_include_directories(UCScenarioGenerator PRIVATE ../..)
    target_link_libraries(UCScenarioGenerator PRIVATE
                          SMS++::UCBlock
                          SMS++::StochasticBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(UCScenarioGenerator PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(UCScenarioGenerator)

    # uc_investment_test additionally measures the implementation error (it
    # applies the decision chosen on the reduced set to the full set) and so
    # links the reduction solvers directly.
    if (TARGET SMS++::ScenarioReductionSolver
        AND TARGET SMS++::TwoStageStochasticBlock)

        add_executable(uc_investment_test
                       UCInvestmentTest/UCInvestmentReductionTest.cpp)
        target_link_libraries(uc_investment_test PRIVATE
                              SMS++::ScenarioReductionSolver
                              SMS++::UCBlock
                              SMS++::StochasticBlock
                              SMS++::TwoStageStochasticBlock)

        if (TARGET SMS++::MILPSolver)
            target_link_libraries(uc_investment_test PRIVATE SMS++::MILPSolver)
        endif ()

        smspp_install(uc_investment_test)

    endif ()

endif ()

# --------------------------------------------------------------------------- #
