# ----- Subdirectories ------------------------------------------------------ #
# Each subdirectory builds the scenario generator (and, for UC, the investment
# test) for one problem. A generator writes a problem file that the generic
# scenario_reduction_solve program (ScenarioReductionSolver module) then reduces
# and solves; see CFLBlock/run_cfl_tests.sh and UCBlock/run_uc_test.sh.
add_subdirectory(CFLBlock)
add_subdirectory(UCBlock)

# --------------------------------------------------------------------------- #
