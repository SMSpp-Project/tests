# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(UCBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::UCBlock)

    add_executable(UCBlock_test test.cpp ../common_utils.cpp)
    target_include_directories(UCBlock_test PRIVATE ..)
    target_link_libraries(UCBlock_test PRIVATE SMS++::UCBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(UCBlock_test PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(UCBlock_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(UCBlock_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(UCBlock_test)

    # Create a test fixture to generate .nc4 files by running extract_uc_nc4.
    # This test will build and run the extract_uc_nc4 target
    add_test(NAME extract_uc_nc4
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target extract_uc_nc4)
    # Mark extract_uc_nc4 as a fixture that sets up "uc_data_extracted"
    set_tests_properties(extract_uc_nc4
                         PROPERTIES FIXTURES_SETUP uc_data_extracted)

    # Register every batch: each one covers a family of instances (academic,
    # plan4res, energy community, resilient, AC OPF) and cross-checks all the
    # Solver its BlockSolverConfig attaches, so they are independent and can be
    # run, selected and timed one by one.
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/*")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME UCBlock_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/UCBlock_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME UCBlock_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:UCBlock_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
        # Make each batch test require the uc_data_extracted fixture
        set_tests_properties(UCBlock_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED uc_data_extracted)
    endforeach ()
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
