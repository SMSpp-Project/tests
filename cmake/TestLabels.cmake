# --------------------------------------------------------------------------- #
#    Per-module CI test labels                                                #
#                                                                             #
#    Single source of the map "test -> the modules that exercise it". Each    #
#    test is tagged with CTest LABELS naming every module it links or finds,  #
#    so a module's CI can run exactly the relevant tests, and only those,     #
#    with no hard-coded paths:                                                #
#                                                                             #
#        ctest -L <module>                                                    #
#                                                                             #
#    where <module> is the module (repository) name, e.g. on GitHub           #
#    `ctest -L "${GITHUB_REPOSITORY##*/}"` and on GitLab                      #
#    `ctest -L "$CI_PROJECT_NAME"`. A test thus runs in the CI of every       #
#    module it depends on, so a change to any of them re-checks it.           #
#                                                                             #
#    To extend it when adding a test: add one SMSPP_TEST_LABELS_<dir> entry   #
#    below (keyed by the test directory name) and call smspp_label_tests()    #
#    at the end of that directory's CMakeLists.txt. The labels are the        #
#    module names the test exercises; `SMS++` denotes the core library.       #
#                                                                             #
#                                Donato Meoli                                 #
#                         Dipartimento di Informatica                         #
#                             Universita' di Pisa                             #
# --------------------------------------------------------------------------- #

set(SMSPP_TEST_LABELS_BoxSolver                 "SMS++;MILPSolver")
set(SMSPP_TEST_LABELS_LagrangianDualSolver_Box  "SMS++;BundleSolver;LagrangianDualSolver;MILPSolver")
set(SMSPP_TEST_LABELS_LagBFunction              "SMS++;BundleSolver;MILPSolver")
set(SMSPP_TEST_LABELS_PolyhedralFunction        "SMS++;BundleSolver;MILPSolver")
set(SMSPP_TEST_LABELS_PolyhedralFunctionBlock   "SMS++;BundleSolver;MILPSolver")
set(SMSPP_TEST_LABELS_QuadraticTests            "SMS++;MILPSolver")
set(SMSPP_TEST_LABELS_Write-Read                "SMS++;MILPSolver")
set(SMSPP_TEST_LABELS_compare_formulations      "SMS++")
set(SMSPP_TEST_LABELS_BendersBFunction          "BundleSolver;MCFBlock;MCFClassSolver;MILPSolver")
set(SMSPP_TEST_LABELS_MCF_MILP                  "MCFBlock;MCFClassSolver;MCFLemonSolver;MILPSolver")
set(SMSPP_TEST_LABELS_BinaryKnapsackBlock       "BinaryKnapsackBlock;BranchAndXSolver;MILPSolver")
set(SMSPP_TEST_LABELS_CapacitatedFacilityLocation
                                                "BundleSolver;CapacitatedFacilityLocationBlock;LagrangianDualSolver;MCFClassSolver;MCFLemonSolver;MILPSolver")
set(SMSPP_TEST_LABELS_MMCFBlock                 "BundleSolver;MMCFBlock;MILPSolver")
set(SMSPP_TEST_LABELS_LagrangianDualSolver_MMCF "BundleSolver;LagrangianDualSolver;MCFLemonSolver;MMCFBlock;MILPSolver")
set(SMSPP_TEST_LABELS_LagrangianDualSolver_UC   "BundleSolver;LagrangianDualSolver;UCBlock;MILPSolver")
set(SMSPP_TEST_LABELS_ThermalUnitBlock_Solver   "UCBlock;MILPSolver")
set(SMSPP_TEST_LABELS_InvestmentBlock           "BundleSolver;InvestmentBlock;MILPSolver")
set(SMSPP_TEST_LABELS_TwoStageStochasticBlock   "BundleSolver;LagrangianDualSolver;TwoStageStochasticBlock;UCBlock;MILPSolver")
set(SMSPP_TEST_LABELS_MultiStageStochasticBlock "BundleSolver;LagrangianDualSolver;MultiStageStochasticBlock;TwoStageStochasticBlock;UCBlock;MILPSolver")
set(SMSPP_TEST_LABELS_LukFiBlock                "BundleSolver;LukFiBlock;MILPSolver")
set(SMSPP_TEST_LABELS_FrankWolfeSolver          "FrankWolfeSolver;MCFBlock;MCFClassSolver;UCBlock;MILPSolver")

# Attach the labels of the current directory (keyed by its name) to every test
# it registered, dynamic batch-file test names included.
function(smspp_label_tests)
    get_filename_component(_dir "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
    get_property(_tests DIRECTORY PROPERTY TESTS)
    if (_tests AND DEFINED SMSPP_TEST_LABELS_${_dir})
        set_tests_properties(${_tests} PROPERTIES
                             LABELS "${SMSPP_TEST_LABELS_${_dir}}")
    endif ()
endfunction()

# Skip the whole suite unless every module its labels declare is in the build:
# the labels are exactly the modules the tests exercise, and a test registered
# with some of them missing only dies at run time with "<module> not present
# in Solver factory". A macro so that return() leaves the calling directory.
macro(smspp_require_labelled_modules)
    get_filename_component(_dir "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
    if (DEFINED SMSPP_TEST_LABELS_${_dir})
        foreach (_mod IN LISTS SMSPP_TEST_LABELS_${_dir})
            if (NOT (_mod STREQUAL "SMS++") AND NOT TARGET SMS++::${_mod})
                message(STATUS "Skipping tests/${_dir}: ${_mod} not in build")
                return()
            endif ()
        endforeach ()
    endif ()
endmacro()

# --------------------------------------------------------------------------- #
