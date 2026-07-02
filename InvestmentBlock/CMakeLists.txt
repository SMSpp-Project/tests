# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(InvestmentBlock)
    # Solvers
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::InvestmentBlock)

    add_executable(InvestmentBlock_test test.cpp ../common_utils.cpp)
    target_include_directories(InvestmentBlock_test PRIVATE ..)
    target_link_libraries(InvestmentBlock_test PRIVATE SMS++::InvestmentBlock)

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(InvestmentBlock_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(InvestmentBlock_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(InvestmentBlock_test)

    # Create a test fixture to generate .nc4 files by running extract_investment_nc4.
    # This test will build and run the extract_investment_nc4 target
    add_test(NAME extract_investment_nc4
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target extract_investment_nc4)
    # Mark extract_investment_nc4 as a fixture that sets up "investment_data_extracted"
    set_tests_properties(extract_investment_nc4
                         PROPERTIES FIXTURES_SETUP investment_data_extracted)

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-resilient")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME InvestmentBlock_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/InvestmentBlock_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME InvestmentBlock_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                     "${BATCH_FILE}" "$<TARGET_FILE:InvestmentBlock_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
        # Make each batch test require the investment_data_extracted fixture
        set_tests_properties(InvestmentBlock_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED investment_data_extracted)
    endforeach ()
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
