# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(MCFBlock)
    # Solvers
    find_package(MILPSolver)
    find_package(MCFClassSolver)
    find_package(MCFLemonSolver)
endif ()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::MCFBlock)

    add_executable(MCF_MILP_test test.cpp ../common_utils.cpp)
    target_include_directories(MCF_MILP_test PRIVATE ..)
    target_link_libraries(MCF_MILP_test PRIVATE SMS++::MCFBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(MCF_MILP_test PRIVATE SMS++::MILPSolver)
    endif ()

    if (TARGET SMS++::MCFClassSolver)
        target_link_libraries(MCF_MILP_test PRIVATE SMS++::MCFClassSolver)
    endif ()

    if (TARGET SMS++::MCFLemonSolver)
        target_link_libraries(MCF_MILP_test PRIVATE SMS++::MCFLemonSolver)
    endif ()

    smspp_install(MCF_MILP_test)

    # Create a test fixture to generate .nc4 files by running run_dmx2nc4.
    # This test will build and run the run_dmx2nc4 target
    add_test(NAME MCF_MILP_run_dmx2nc4
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_dmx2nc4)
    # Mark it as a fixture that sets up "dmx2nc4_generated"
    set_tests_properties(MCF_MILP_run_dmx2nc4
                         PROPERTIES FIXTURES_SETUP dmx2nc4_generated)

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batch")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME MCF_MILP_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/MCF_MILP_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME MCF_MILP_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:MCF_MILP_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
        # Make each batch test require the dmx2nc4_generated fixture
        set_tests_properties(MCF_MILP_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED dmx2nc4_generated)
    endforeach ()
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
