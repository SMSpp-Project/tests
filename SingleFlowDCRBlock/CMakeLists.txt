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

    # the batteries on the instances of the module need them extracted first,
    # which this test does, building the target that downloads them
    add_test(NAME extract_dcr_nc4
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                     --target extract_dcr_nc4)
    set_tests_properties(extract_dcr_nc4
                         PROPERTIES FIXTURES_SETUP dcr_data_extracted)

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/*")
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
        if (BATCH_NAME STREQUAL "batches/batch-instances")
            set_tests_properties(SingleFlowDCRBlock_test/${BATCH_NAME}
                                 PROPERTIES FIXTURES_REQUIRED dcr_data_extracted)
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
                                 PROPERTIES FIXTURES_REQUIRED dcr_data_extracted)
        endforeach ()
    endif ()
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
