# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SingleFlowDCRBlock)
    # Solvers
    find_package(MILPSolver)
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
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

    # the instances of SingleFlowDCRBlock, downloaded and extracted when the
    # tests that read them are run, not when they are built [see the
    # CMakeLists.txt of the module]; the lock is shared by the tests that build
    # a target of the data of the module, which under ctest -j would write the
    # same files at once
    add_test(NAME extract_dcr_nc4
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                     --target extract_dcr_nc4)
    set_tests_properties(extract_dcr_nc4 PROPERTIES
                         FIXTURES_SETUP dcr_nc4_extracted
                         RESOURCE_LOCK dcr_data)

    # the battery that solves the SOCP formulation with CPLEX where Gurobi is
    # not stable is only there when CPLEX is [see socp-unstable.txt]
    find_package(CPLEX QUIET)

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/*")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (BATCH_NAME STREQUAL "batches/batch-instances-cplex"
                AND NOT (CPLEX_FOUND AND TARGET SMS++::MILPSolver))
            continue ()
        endif ()
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
        if (BATCH_NAME MATCHES "^batches/batch-instances")
            set_tests_properties(SingleFlowDCRBlock_test/${BATCH_NAME}
                                 PROPERTIES FIXTURES_REQUIRED dcr_nc4_extracted)
        endif ()
    endforeach ()

    # MultiFlowDCRBlock_test, whose batteries compare a :MILPSolver with the
    # LagrangianDualSolver, hence they are only there when the build has it,
    # the BundleSolver inside it included; all of them read the instances of
    # the module
    if (TARGET SMS++::LagrangianDualSolver AND TARGET SMS++::BundleSolver)
        add_executable(MultiFlowDCRBlock_test test_multiflow.cpp
                       ../common_utils.cpp)
        target_include_directories(MultiFlowDCRBlock_test PRIVATE ..)
        target_link_libraries(MultiFlowDCRBlock_test
                              PRIVATE SMS++::SingleFlowDCRBlock
                                      SMS++::LagrangianDualSolver
                                      SMS++::BundleSolver)

        if (TARGET SMS++::MILPSolver)
            target_link_libraries(MultiFlowDCRBlock_test
                                  PRIVATE SMS++::MILPSolver)
        endif ()

        smspp_install(MultiFlowDCRBlock_test)

        file(GLOB MULTI_BATCH_FILES
             "${CMAKE_CURRENT_SOURCE_DIR}/batches-multiflow/*")
        foreach (BATCH_FILE ${MULTI_BATCH_FILES})
            string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME
                   ${BATCH_FILE})
            if (UNIX)
                add_test(NAME MultiFlowDCRBlock_test/${BATCH_NAME}
                         COMMAND bash ${BATCH_FILE}
                                 ${CMAKE_CURRENT_BINARY_DIR}/MultiFlowDCRBlock_test
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            elseif (WIN32)
                add_test(NAME MultiFlowDCRBlock_test/${BATCH_NAME}
                         # using Git for Windows that includes Git Bash
                         COMMAND "C:/Program Files/Git/bin/bash.exe"
                                 "${BATCH_FILE}"
                                 "$<TARGET_FILE:MultiFlowDCRBlock_test>"
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            endif ()
            set_tests_properties(MultiFlowDCRBlock_test/${BATCH_NAME}
                                 PROPERTIES FIXTURES_REQUIRED dcr_nc4_extracted)
        endforeach ()
    endif ()
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
