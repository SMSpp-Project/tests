# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SMS++)
    # Solvers
    find_package(MILPSolver)
endif ()

# ----- Tests --------------------------------------------------------------- #
add_executable(quad_test test.cpp ../common_utils.cpp)
target_include_directories(quad_test PRIVATE ..)
target_link_libraries(quad_test PRIVATE SMS++::SMS++)

if (TARGET SMS++::MILPSolver)
    target_link_libraries(quad_test PRIVATE SMS++::MILPSolver)
endif ()

smspp_install(quad_test)

# Add a test for each batch file
file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/batchCL_C"
                      "${CMAKE_CURRENT_SOURCE_DIR}/batches/batchCL_I"
                      "${CMAKE_CURRENT_SOURCE_DIR}/batches/batchLC_C"
                      "${CMAKE_CURRENT_SOURCE_DIR}/batches/batchLC_I"
                      "${CMAKE_CURRENT_SOURCE_DIR}/batches/batchQL_C"
                      "${CMAKE_CURRENT_SOURCE_DIR}/batches/batchQL_I")
foreach (BATCH_FILE ${BATCH_FILES})
    # Remove the source directory prefix to get the batch name
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
    if (UNIX)
        add_test(NAME quad_test/${BATCH_NAME}
                 COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/quad_test
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    elseif (WIN32)
        add_test(NAME quad_test/${BATCH_NAME}
                # using Git for Windows that includes Git Bash
                COMMAND "C:/Program Files/Git/bin/bash.exe"
                "${BATCH_FILE}" "$<TARGET_FILE:quad_test>"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif ()
endforeach ()

# --------------------------------------------------------------------------- #
