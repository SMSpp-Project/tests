# ----- Subdirectories ------------------------------------------------------ #
# Each subdirectory builds the scenario generator (and, for UC, the investment
# test) for one problem. A generator writes a problem file that the generic
# ScenarioReductionSolver_test program (ScenarioReductionSolver module) then reduces
# and solves; see CFLBlock/run_cfl_tests and UCBlock/run_uc_test.
add_subdirectory(CFLBlock)
add_subdirectory(UCBlock)

# --------------------------------------------------------------------------- #
