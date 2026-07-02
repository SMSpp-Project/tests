# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(MMCFBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
    find_package(MCFLemonSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::MMCFBlock)

    add_executable(LDS_MMCF_test test.cpp ../common_utils.cpp)
    target_include_directories(LDS_MMCF_test PRIVATE ..)
    target_link_libraries(LDS_MMCF_test PRIVATE SMS++::MMCFBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(LDS_MMCF_test PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(LDS_MMCF_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(LDS_MMCF_test PRIVATE SMS++::MILPSolver)
    endif ()

    if (TARGET SMS++::MCFLemonSolver)
        target_link_libraries(LDS_MMCF_test PRIVATE SMS++::MCFLemonSolver)
    endif ()

    smspp_install(LDS_MMCF_test)

    # The ML batch (batchML) drives the inner BundleSolverML, which is only
    # registered in the Solver factory when BundleSolver is built with Torch;
    # register it as a test only in that case.
    find_package(Torch QUIET HINTS ${Torch_ROOT})
    set(ML_BATCH "")
    if (Torch_FOUND)
        set(ML_BATCH "${CMAKE_CURRENT_SOURCE_DIR}/batchML")
    endif ()

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batch"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batch-c"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batch-m"
                          ${ML_BATCH})
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME LDS_MMCF_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/LDS_MMCF_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME LDS_MMCF_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:LDS_MMCF_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
    endforeach ()
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
