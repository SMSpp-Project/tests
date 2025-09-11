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

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::UCBlock)

    add_executable(LDS_UC_test test.cpp)
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

    # Create a test fixture to generate .nc4 files by running extract_nc4uc.
    # This test will build and run the extract_nc4uc target
    add_test(NAME extract_nc4uc
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target extract_nc4uc)
    # Mark extract_nc4uc as a fixture that sets up "data_extracted"
    set_tests_properties(extract_nc4uc PROPERTIES FIXTURES_SETUP data_extracted)

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-acad"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-acad-s"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-acadh"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-ec"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-p4r")
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
        # Make each batch test require the data_extracted fixture
        set_tests_properties(LDS_UC_test/${BATCH_NAME}
                PROPERTIES FIXTURES_REQUIRED data_extracted)
    endforeach ()
endif ()

# --------------------------------------------------------------------------- #
