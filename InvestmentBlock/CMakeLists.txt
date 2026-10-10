# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(InvestmentBlock)
    find_package(TwoStageStochasticBlock)
    find_package(MultiStageStochasticBlock)
    # Solvers
    find_package(BundleSolver)
    find_package(MILPSolver)
    find_package(LagrangianDualSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::InvestmentBlock)

    add_executable(IB_test test.cpp ../common_utils.cpp)
    target_include_directories(IB_test PRIVATE ..)
    target_link_libraries(IB_test PRIVATE SMS++::InvestmentBlock)

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(IB_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(IB_test PRIVATE SMS++::MILPSolver)
    endif ()

    # the inner Block of batch-stochastic is also solved by the recursive
    # Lagrangian dual, which the Solver factory only knows if it is linked in
    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(IB_test PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    # the InvestmentBlock accepts a whole stochastic Block as its inner one;
    # the Block factory only knows the types that are linked in, so without
    # these the instances of batch-stochastic cannot be deserialized
    if (TARGET SMS++::TwoStageStochasticBlock)
        target_link_libraries(IB_test
                              PRIVATE SMS++::TwoStageStochasticBlock)
    endif ()

    if (TARGET SMS++::MultiStageStochasticBlock)
        target_link_libraries(IB_test
                              PRIVATE SMS++::MultiStageStochasticBlock)
    endif ()

    smspp_install(IB_test)

    # the instances of InvestmentBlock, downloaded and extracted when the tests
    # that read them are run, not when they are built [see the CMakeLists.txt
    # of the module]; the lock is shared by the tests that build a target of
    # the data of the module, which under ctest -j would write the same files
    # at once
    add_test(NAME extract_investment_nc4
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                     --target extract_investment_nc4)
    set_tests_properties(extract_investment_nc4 PROPERTIES
                         FIXTURES_SETUP investment_nc4_extracted
                         RESOURCE_LOCK investment_data)

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-*")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME IB_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/IB_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME IB_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                     "${BATCH_FILE}" "$<TARGET_FILE:IB_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
        # Make each batch test require the investment_nc4_extracted fixture
        set_tests_properties(IB_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED investment_nc4_extracted)

        # batch-stochastic and batch-pypsa run the modular instances a second
        # time with the inner Block solved by the LagrangianDualSolver, which
        # may not be in the build: there the second run is emptied out
        if (BATCH_NAME MATCHES "batch-(stochastic|pypsa)" AND
            NOT TARGET SMS++::LagrangianDualSolver)
            set_tests_properties(IB_test/${BATCH_NAME}
                                 PROPERTIES ENVIRONMENT "IB_LD_BCFG=")
        endif ()
    endforeach ()
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
