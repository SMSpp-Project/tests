# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(TwoStageStochasticBlock)
    find_package(UCBlock)
    find_package(InvestmentBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
    find_package(BendersDecompositionSolver)
    find_package(MMCFBlock)
endif ()

# The reduction of whatever TwoStageStochasticBlock a file holds: the tester
# reads the instance, its scenario set and the AbstractPath of its first-stage
# Variable, reduces the scenarios to the K representatives a method of
# ScenarioReductionSolver (or CSSCScenarioReductionSolver) picks, solves the
# reduced problem and reports its gap against the whole one. It knows of no
# concrete Block, hence it registers no test of its own: it is run by the
# batches-scenred batteries of the suites whose generators write those files
# (tests/UCBlock and tests/CapacitatedFacilityLocationBlock), which is why
# this directory is added before theirs, and why it is built before the
# modules the rest of the suite needs are asked for, since it needs none of
# them; the Block of those instances and the :MILPSolver are linked when they
# are in the build, so that the factory can read and solve them.
if (TARGET SMS++::ScenarioReductionSolver)

    add_executable(TSSB_scenred_test test_scenred.cpp)
    target_link_libraries(TSSB_scenred_test PRIVATE
                          SMS++::TwoStageStochasticBlock
                          SMS++::ScenarioReductionSolver)

    foreach (_mod CapacitatedFacilityLocationBlock UCBlock MILPSolver)
        if (TARGET SMS++::${_mod})
            target_link_libraries(TSSB_scenred_test PRIVATE SMS++::${_mod})
        endif ()
    endforeach ()

    smspp_install(TSSB_scenred_test)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::TwoStageStochasticBlock)

    add_executable(TSSB_test test.cpp benders_form.cpp ../common_utils.cpp)
    target_include_directories(TSSB_test PRIVATE ..)
    target_link_libraries(TSSB_test PRIVATE SMS++::TwoStageStochasticBlock)

    # the Benders form of an instance is assembled by the tester itself [see
    # benders_form()], but the Solver that asks for it comes out of the
    # factory, hence has to be linked where it is built
    if (TARGET SMS++::BendersDecompositionSolver)
        target_link_libraries(TSSB_test PRIVATE
                SMS++::BendersDecompositionSolver)
    endif ()

    if (TARGET SMS++::UCBlock)
        target_link_libraries(TSSB_test PRIVATE SMS++::UCBlock)
    endif ()

    # a MultiStageStochasticBlock is a TwoStageStochasticBlock, whose leaves
    # benders_form() takes as the subproblems, hence the tester solves it too
    # when the factory knows it
    if (TARGET SMS++::MultiStageStochasticBlock)
        target_link_libraries(TSSB_test PRIVATE
                SMS++::MultiStageStochasticBlock)
    endif ()

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(TSSB_test PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(TSSB_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(TSSB_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(TSSB_test)

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-ec"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-pypsa")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME TSSB_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/TSSB_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME TSSB_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:TSSB_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
        # Re-use the same `uc_nc4_extracted` fixture set up by
        # tests/UCBlock/CMakeLists.txt: it builds the
        # extract_uc_nc4 target which produces the EC nc4 files used here.
        # CTest fixtures are global, so referencing it from a different
        # subdirectory just adds a dependency without redefining the test.
        set_tests_properties(TSSB_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED uc_nc4_extracted)

        # batch-pypsa runs each instance twice, the second time on the Benders
        # form, whose BlockSolverConfig names a Solver that may not be in the
        # build: there the second run is emptied out
        if (BATCH_NAME MATCHES "batch-pypsa" AND
            NOT TARGET SMS++::BendersDecompositionSolver)
            set_tests_properties(TSSB_test/${BATCH_NAME}
                                 PROPERTIES ENVIRONMENT "TSSB_BDS_PAR=")
        endif ()
    endforeach ()

    # The three ways of solving the same two-stage stochastic investment
    # problem: the extensive form, the generic Benders decomposition of
    # BendersDecompositionSolver and the ad hoc one an InvestmentBlock over
    # the whole TwoStageStochasticBlock carries. Neither of the two Solver it
    # takes is among the labels of this suite, since the rest of it runs
    # without them, so the tester is built only where they are.
    if (TARGET SMS++::BendersDecompositionSolver AND
        TARGET SMS++::InvestmentBlock)

        add_executable(TSSB_BDS_test test_bds.cpp)
        target_link_libraries(TSSB_BDS_test PRIVATE
                SMS++::TwoStageStochasticBlock
                SMS++::InvestmentBlock
                SMS++::BendersDecompositionSolver
                SMS++::BundleSolver
                SMS++::MILPSolver)
        smspp_install(TSSB_BDS_test)

        add_test(NAME TSSB_BDS_test
                 COMMAND TSSB_BDS_test
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

        # the same comparison on instances of growing size, which is where the
        # forms part ways: those instances are written by gen_investment.py,
        # hence the batch runs only where there is a Python that can write
        # netCDF
        find_package(Python3 COMPONENTS Interpreter)
        if (Python3_Interpreter_FOUND)
            execute_process(COMMAND ${Python3_EXECUTABLE} -c "import netCDF4"
                            RESULT_VARIABLE HAS_NETCDF4
                            OUTPUT_QUIET ERROR_QUIET)
            if (HAS_NETCDF4 EQUAL 0)
                add_test(NAME TSSB_BDS_test/batches/batch-bds
                         COMMAND bash
                                 ${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-bds
                                 $<TARGET_FILE:TSSB_BDS_test>
                                 ${Python3_EXECUTABLE}
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            else ()
                message(STATUS "tests/TwoStageStochasticBlock: netCDF4 is not "
                               "there, the BDS batch is out")
            endif ()
        endif ()
    endif ()
endif ()

# A stochastic multicommodity network design problem: a
# TwoStageStochasticBlock whose scenarios are MMCFBlock with one knapsack per
# arc, solved by the MILP, by the Lagrangian duals over the scenarios (with
# MILP scenarios, nested, recursive) and, on its Benders form, by
# BendersDecompositionSolver, all cross-checked [see batches/batch-mmcf]. The
# instances are written by mmcf_tssb_gen out of the Canad ones of the
# MMCFBlock suite. Neither the MMCFBlock nor the Solver are among the labels
# of this suite, so the tester links them and the batch runs only where they
# are all there.
if (TARGET SMS++::TwoStageStochasticBlock AND TARGET SMS++::MMCFBlock AND
    TARGET SMS++::LagrangianDualSolver AND TARGET SMS++::BundleSolver AND
    TARGET SMS++::MILPSolver AND TARGET SMS++::BendersDecompositionSolver)

    target_link_libraries(TSSB_test PRIVATE SMS++::MMCFBlock)

    add_executable(mmcf_tssb_gen mmcf_tssb_gen.cpp)
    target_link_libraries(mmcf_tssb_gen PRIVATE
                          SMS++::MMCFBlock
                          SMS++::TwoStageStochasticBlock)

    if (UNIX)
        add_test(NAME TSSB_test/batches/batch-mmcf
                 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-mmcf
                         $<TARGET_FILE:TSSB_test>
                         $<TARGET_FILE:mmcf_tssb_gen>
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        # the instances are written out of the Canad ones, which the
        # extract_mmcf_Canad test of the MMCFBlock suite downloads
        set_tests_properties(TSSB_test/batches/batch-mmcf
                             PROPERTIES FIXTURES_REQUIRED mmcf_Canad_extracted)
    endif ()
endif ()

# What a scenario reduction costs on the investment problem: the instance is
# solved on the whole scenario set, then on the K representatives each method
# of ScenarioReductionSolver picks, and the first-stage decision the reduced
# problem finds is put back into the whole set, so that what is reported is
# both the gap of the reduced problem and the implementation error of its
# decision. The Block is a TwoStageStochasticBlock and the second stage a
# UCBlock, hence it belongs to this suite and not to that of either of the
# two modules it also reads.
if (TARGET SMS++::ScenarioReductionSolver AND TARGET SMS++::UCBlock
    AND TARGET SMS++::StochasticBlock)

    add_executable(TSSB_reduction_test test_reduction.cpp)
    target_link_libraries(TSSB_reduction_test PRIVATE
                          SMS++::TwoStageStochasticBlock
                          SMS++::ScenarioReductionSolver
                          SMS++::StochasticBlock
                          SMS++::UCBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(TSSB_reduction_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(TSSB_reduction_test)

    add_batch_test(TSSB_reduction_test batches-scenred/batch)
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
