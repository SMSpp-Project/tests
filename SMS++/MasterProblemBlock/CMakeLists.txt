# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SMS++)
    # Solvers
    find_package(MILPSolver)
    find_package(BDMinQuadSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
add_executable(MasterProblemBlock_test test.cpp ../../common_utils.cpp)
target_include_directories(MasterProblemBlock_test PRIVATE ../..)
target_link_libraries(MasterProblemBlock_test PRIVATE SMS++::SMS++
                      SMS++::MILPSolver)

# the Solver of the third entry of MPBSolvers.txt, which the tester leaves
# out when the build does not have it
if (TARGET SMS++::BDMinQuadSolver)
    target_link_libraries(MasterProblemBlock_test PRIVATE
                          SMS++::BDMinQuadSolver)
endif ()

smspp_install(MasterProblemBlock_test)

# batch is the battery of the CI; batch-long, the same over more seeds,
# rounds and sizes, is run by hand
if (UNIX)
    add_test(NAME MasterProblemBlock_test/batch
             COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/batch
                 ${CMAKE_CURRENT_BINARY_DIR}/MasterProblemBlock_test
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
elseif (WIN32)
    add_test(NAME MasterProblemBlock_test/batch
             # using Git for Windows that includes Git Bash
             COMMAND "C:/Program Files/Git/bin/bash.exe"
                     "${CMAKE_CURRENT_SOURCE_DIR}/batch"
                     "$<TARGET_FILE:MasterProblemBlock_test>"
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# the battery runs BDMinQuadSolver too when the build has it, which the map
# of the directory does not declare since the suite does not need it
set(_batch MasterProblemBlock_test/batch)
if ((TARGET SMS++::BDMinQuadSolver) AND (TEST ${_batch}))
    set_tests_properties(${_batch} PROPERTIES LABELS
        "${SMSPP_TEST_LABELS_MasterProblemBlock};BDMinQuadSolver")
endif ()

# --------------------------------------------------------------------------- #
