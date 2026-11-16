# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SMS++)
    # Solvers
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
add_executable(LagBFunction_test test.cpp ../../common_utils.cpp)
target_include_directories(LagBFunction_test PRIVATE ../..)
target_link_libraries(LagBFunction_test PRIVATE SMS++::SMS++)

if (TARGET SMS++::BundleSolver)
    target_link_libraries(LagBFunction_test PRIVATE SMS++::BundleSolver)
endif ()

if (TARGET SMS++::MILPSolver)
    target_link_libraries(LagBFunction_test PRIVATE SMS++::MILPSolver)
endif ()

smspp_install(LagBFunction_test)

# Add a test for each batch file
file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch"
                      "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-easy")
foreach (BATCH_FILE ${BATCH_FILES})
    # Remove the source directory prefix to get the batch name
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
    add_test(NAME LagBFunction_test/${BATCH_NAME}
             COMMAND ${SMSpp_BASH}
                     "${BATCH_FILE}" "$<TARGET_FILE:LagBFunction_test>"
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
