# ----- Requirements -------------------------------------------------------- #
# BundleSolverML is only built into BundleSolver if libTorch is available,
# hence this test is only added if libTorch is found.
find_package(Torch QUIET HINTS ${Torch_ROOT})

if (Torch_FOUND)
    # If it's not being called by the umbrella, we need to
    # look for the system-installed libraries.
    if (NOT hasParent)
        # Blocks
        find_package(SMS++)
        # Solvers
        find_package(BundleSolver)
        find_package(MILPSolver)
    endif ()

    # ----- Tests ------------------------------------------------------------ #
    add_executable(BundleSolverML_test test.cpp ../common_utils.cpp)
    target_include_directories(BundleSolverML_test PRIVATE ..)
    target_link_libraries(BundleSolverML_test PRIVATE SMS++::SMS++)

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(BundleSolverML_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(BundleSolverML_test PRIVATE SMS++::MILPSolver)
    endif ()

    target_link_libraries(BundleSolverML_test PRIVATE ${TORCH_LIBRARIES})

    smspp_install(BundleSolverML_test)

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batch")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME BundleSolverML_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/BundleSolverML_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME BundleSolverML_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:BundleSolverML_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
    endforeach ()
endif ()

# --------------------------------------------------------------------------- #
