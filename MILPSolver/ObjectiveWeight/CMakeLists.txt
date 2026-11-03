# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SMS++)
    # Solvers
    find_package(MILPSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
add_executable(MILPSolver_ObjectiveWeight_test test.cpp ../../common_utils.cpp)
target_include_directories(MILPSolver_ObjectiveWeight_test PRIVATE ../..)
target_link_libraries(MILPSolver_ObjectiveWeight_test PRIVATE SMS++::SMS++)

if (TARGET SMS++::MILPSolver)
    target_link_libraries(MILPSolver_ObjectiveWeight_test
                          PRIVATE SMS++::MILPSolver)
endif ()

smspp_install(MILPSolver_ObjectiveWeight_test)

if (UNIX)
    add_test(NAME MILPSolver_ObjectiveWeight_test/batches/batch-weight
             COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-weight
                     ${CMAKE_CURRENT_BINARY_DIR}/MILPSolver_ObjectiveWeight_test
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
elseif (WIN32)
    add_test(NAME MILPSolver_ObjectiveWeight_test/batches/batch-weight
             # using Git for Windows that includes Git Bash
             COMMAND "C:/Program Files/Git/bin/bash.exe"
             "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-weight"
             "$<TARGET_FILE:MILPSolver_ObjectiveWeight_test>"
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
