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

    add_executable(LDS_UC_test test.cpp ../common_utils.cpp)
    target_include_directories(LDS_UC_test PRIVATE ..)
    target_link_libraries(LDS_UC_test PRIVATE SMS++::UCBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(LDS_UC_test PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(LDS_UC_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(LDS_UC_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(LDS_UC_test)

    # Create a test fixture to generate .nc4 files by running extract_uc_nc4.
    # This test will build and run the extract_uc_nc4 target
    add_test(NAME extract_uc_nc4
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target extract_uc_nc4)
    # Mark extract_uc_nc4 as a fixture that sets up "uc_data_extracted"
    set_tests_properties(extract_uc_nc4
                         PROPERTIES FIXTURES_SETUP uc_data_extracted)

    # Register the two meta-batches only. metabatch (slv=0, LagrangianDualSolver)
    # and metabatch-PPH (slv=1, PrimalProximalHeur) each invoke every individual
    # batch-* script, so together they exercise all instances under both solver
    # modes and cover the metabatch orchestration itself. Registering the single
    # batch-* here as well would double the runtime and would still leave plain
    # metabatch untested; per-batch granularity is available locally by running
    # the batch-* scripts directly.
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/metabatch"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/metabatch-PPH")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME LDS_UC_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/LDS_UC_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME LDS_UC_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:LDS_UC_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
        # Make each batch test require the uc_data_extracted fixture
        set_tests_properties(LDS_UC_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED uc_data_extracted)
    endforeach ()
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
