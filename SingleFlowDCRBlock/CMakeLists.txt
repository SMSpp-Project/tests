# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SingleFlowDCRBlock)
    # Solvers
    find_package(MILPSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::SingleFlowDCRBlock)

    add_executable(SingleFlowDCRBlock_test test.cpp ../common_utils.cpp)
    target_include_directories(SingleFlowDCRBlock_test PRIVATE ..)
    target_link_libraries(SingleFlowDCRBlock_test
                          PRIVATE SMS++::SingleFlowDCRBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(SingleFlowDCRBlock_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(SingleFlowDCRBlock_test)

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batch*")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME SingleFlowDCRBlock_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE}
                             ${CMAKE_CURRENT_BINARY_DIR}/SingleFlowDCRBlock_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME SingleFlowDCRBlock_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}"
                             "$<TARGET_FILE:SingleFlowDCRBlock_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
    endforeach ()
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
