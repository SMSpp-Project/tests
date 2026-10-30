# ----- Requirements -------------------------------------------------------- #
# The C++20 modules of the SMS++ modules, built when SMSpp_BUILD_MODULE is ON
# [see SMS++]; if it's not being called by the umbrella, they are looked for
# among the system-installed libraries.
if (NOT hasParent)
    find_package(SMS++)
endif ()

# ----- Tests --------------------------------------------------------------- #
# One program imports the module of each SMS++ module that is in the build,
# and for each one of them names its main classes and gets a Block or a
# Solver from the factory: a name that a module fails to export stops the
# build, a class whose factory registration is lost stops the run.
# the scan of the module dependencies, which cmake_policy(VERSION 3.15) of
# the tests leaves off, without which the program finds no module to import
cmake_policy(SET CMP0155 NEW)
add_executable(import_modules_test test.cpp)
target_link_libraries(import_modules_test PRIVATE SMS++::SMS++_module)

foreach (module
         BendersDecompositionSolver BinaryKnapsackBlock BranchAndXSolver
         BundleSolver CapacitatedFacilityLocationBlock FrankWolfeSolver
         InvestmentBlock LagrangianDualSolver LukFiBlock MCFBlock
         MCFClassSolver MCFLemonSolver MILPSolver MMCFBlock
         MultiKnapsackAssignBlock MultiStageStochasticBlock SATBlock SDDPBlock
         SVMBlock SatellitesBlock ScenarioReductionSolver SingleFlowDCRBlock
         StochasticBlock SubGradSolver TwoStageStochasticBlock UCBlock)
    if (TARGET SMS++::${module}_module)
        target_link_libraries(import_modules_test PRIVATE
                SMS++::${module}_module)
        target_compile_definitions(import_modules_test PRIVATE
                HAVE_${module})
    endif ()
endforeach ()

add_test(NAME import_modules_test
         COMMAND import_modules_test)

# --------------------------------------------------------------------------- #
