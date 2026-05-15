# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(TwoStageStochasticBlock)
    find_package(UCBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::TwoStageStochasticBlock)

    add_executable(TSSB_test test.cpp ../common_utils.cpp)
    target_include_directories(TSSB_test PRIVATE ..)
    target_link_libraries(TSSB_test PRIVATE SMS++::TwoStageStochasticBlock)

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
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-ec")
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
        # tests/LagrangianDualSolver_UC/CMakeLists.txt: it builds the
        # extract_uc_nc4 target which produces the EC nc4 files used here.
        # CTest fixtures are global, so referencing it from a different
        # subdirectory just adds a dependency without redefining the test.
        set_tests_properties(TSSB_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED uc_data_extracted)
    endforeach ()
endif ()

# --------------------------------------------------------------------------- #
