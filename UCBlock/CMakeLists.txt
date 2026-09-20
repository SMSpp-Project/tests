# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(UCBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::UCBlock)

    add_executable(UCBlock_test test.cpp ../common_utils.cpp)
    target_include_directories(UCBlock_test PRIVATE ..)
    target_link_libraries(UCBlock_test PRIVATE SMS++::UCBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(UCBlock_test PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(UCBlock_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(UCBlock_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(UCBlock_test)

    # the pollutant budget constraints, on instances the tester writes itself
    if (TARGET SMS++::MILPSolver)
        add_test(NAME UCBlock_test/pollutant
                 COMMAND UCBlock_test --pollutant)
    endif ()

    # Create a test fixture to generate .nc4 files by running extract_uc_nc4.
    # This test will build and run the extract_uc_nc4 target
    add_test(NAME extract_uc_nc4
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target extract_uc_nc4)
    # Mark extract_uc_nc4 as a fixture that sets up "uc_data_extracted"
    set_tests_properties(extract_uc_nc4
                         PROPERTIES FIXTURES_SETUP uc_data_extracted)

    # Register every batch: each one covers a family of instances (academic,
    # plan4res, energy community, PyPSA, AC OPF) and cross-checks all the
    # Solver its BlockSolverConfig attaches, so they are independent and can be
    # run, selected and timed one by one.
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/*")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME UCBlock_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/UCBlock_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME UCBlock_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:UCBlock_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
        # Make each batch test require the uc_data_extracted fixture
        set_tests_properties(UCBlock_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED uc_data_extracted)
    endforeach ()

    # The specialised dynamic programming solver of ThermalUnitBlock against a
    # :MILPSolver on the same unit: it reads the instances of this suite and
    # compares two solvers of this module's Block, hence it is one of the
    # tests of UCBlock and not a suite of its own. Its batches live apart
    # because each of them is run with this tester and not with the one above.
    add_executable(TUDPS_test test_tudps.cpp ../common_utils.cpp)
    target_include_directories(TUDPS_test PRIVATE ..)
    target_link_libraries(TUDPS_test PRIVATE SMS++::UCBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(TUDPS_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(TUDPS_test)

    file(GLOB TUDPS_BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches-tudps/*")
    foreach (BATCH_FILE ${TUDPS_BATCH_FILES})
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME TUDPS_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/TUDPS_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME TUDPS_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:TUDPS_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
        set_tests_properties(TUDPS_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED uc_data_extracted)
    endforeach ()
endif ()

# The scenario generator of this Block and the runs that reduce the scenarios
# it writes: the reduction is asked of ScenarioReductionSolver, but what it is
# asked of is the unit commitment instances, hence it belongs to this suite.
if (TARGET SMS++::ScenarioReductionSolver)
    add_subdirectory(scenario-reduction)
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
