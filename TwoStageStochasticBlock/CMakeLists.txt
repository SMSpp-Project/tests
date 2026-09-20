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
        # Re-use the same `uc_data_extracted` fixture set up by
        # tests/UCBlock/CMakeLists.txt: it builds the
        # extract_uc_nc4 target which produces the EC nc4 files used here.
        # CTest fixtures are global, so referencing it from a different
        # subdirectory just adds a dependency without redefining the test.
        set_tests_properties(TSSB_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED uc_data_extracted)

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

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
