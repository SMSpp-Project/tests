# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SMS++)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::LagrangianDualSolver)

    add_executable(LDS_Box_test test.cpp)
    target_link_libraries(LDS_Box_test PRIVATE SMS++::LagrangianDualSolver)

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(LDS_Box_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(LDS_Box_test PRIVATE SMS++::MILPSolver)
    endif ()

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batch")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME LDS_Box_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/LDS_Box_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME LDS_Box_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:LDS_Box_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
    endforeach ()
endif ()

# --------------------------------------------------------------------------- #
