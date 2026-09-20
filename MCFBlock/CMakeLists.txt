# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(MCFBlock)
    # Solvers
    find_package(MILPSolver)
    find_package(MCFClassSolver)
    find_package(MCFLemonSolver)
    find_package(FrankWolfeSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::MCFBlock)

    add_executable(MCFBlock_test test.cpp ../common_utils.cpp)
    target_include_directories(MCFBlock_test PRIVATE ..)
    target_link_libraries(MCFBlock_test PRIVATE SMS++::MCFBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(MCFBlock_test PRIVATE SMS++::MILPSolver)
    endif ()

    if (TARGET SMS++::MCFClassSolver)
        target_link_libraries(MCFBlock_test PRIVATE SMS++::MCFClassSolver)
    endif ()

    if (TARGET SMS++::MCFLemonSolver)
        target_link_libraries(MCFBlock_test PRIVATE SMS++::MCFLemonSolver)
    endif ()

    smspp_install(MCFBlock_test)

    # Create a test fixture to generate .nc4 files by running run_dmx2nc4.
    # This test will build and run the run_dmx2nc4 target
    add_test(NAME run_dmx2nc4
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_dmx2nc4)
    # Mark it as a fixture that sets up "dmx2nc4_generated"
    set_tests_properties(run_dmx2nc4
                         PROPERTIES FIXTURES_SETUP dmx2nc4_generated)

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/*")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME MCFBlock_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/MCFBlock_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME MCFBlock_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:MCFBlock_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
        # Make each batch test require the dmx2nc4_generated fixture
        set_tests_properties(MCFBlock_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED dmx2nc4_generated)
    endforeach ()

    # The Frank-Wolfe decomposition of a father Block whose leaves are the
    # MCFBlock of this suite. The tester is the generic one (../fw_test.cpp):
    # it reads a leaf Block K times, makes the K copies the sub-Block of a
    # father AbstractBlock with a random objective over their Variable, and
    # cross-checks the FrankWolfeSolver that decomposes it, using the :Solver
    # registered to each sub-Block as a Linear Minimization Oracle, against a
    # monolithic :MILPSolver. Which Block is read and which :Solver are
    # attached is the configurations' business, so the same source is built on
    # the ThermalUnitBlock of the UCBlock suite; here the LMO is a MCFSolver,
    # and the configurations and the batches live in FW/ beside it.
    if (TARGET SMS++::FrankWolfeSolver)

        add_executable(MCFBlock_FW_test ../fw_test.cpp ../common_utils.cpp)
        target_include_directories(MCFBlock_FW_test PRIVATE ..)
        target_link_libraries(MCFBlock_FW_test PRIVATE
                              SMS++::FrankWolfeSolver SMS++::MCFBlock)

        if (TARGET SMS++::MCFClassSolver)
            target_link_libraries(MCFBlock_FW_test PRIVATE
                                  SMS++::MCFClassSolver)
        endif ()

        if (TARGET SMS++::MILPSolver)
            target_link_libraries(MCFBlock_FW_test PRIVATE SMS++::MILPSolver)
        endif ()

        smspp_install(MCFBlock_FW_test)

        # the same decomposition while the feasible region of a sub-Block
        # changes (arc costs, capacities and fixing): what it changes is of
        # the MCFBlock, hence this tester is not the generic one
        add_executable(MCFBlock_FWmods_test test_fw_mods.cpp ../common_utils.cpp)
        target_include_directories(MCFBlock_FWmods_test PRIVATE ..)
        target_link_libraries(MCFBlock_FWmods_test PRIVATE
                              SMS++::FrankWolfeSolver SMS++::MCFBlock)

        if (TARGET SMS++::MCFClassSolver)
            target_link_libraries(MCFBlock_FWmods_test PRIVATE
                                  SMS++::MCFClassSolver)
        endif ()

        if (TARGET SMS++::MILPSolver)
            target_link_libraries(MCFBlock_FWmods_test PRIVATE
                                  SMS++::MILPSolver)
        endif ()

        smspp_install(MCFBlock_FWmods_test)

        # the fast regression suite: small instances, static and with dynamic
        # arcs, covering vanilla / Away-step / BPCG, the bounded active set,
        # the parallel LMO, the Polyhedral bracket and the rounds of
        # Modification of the feasible region. It reads the instances of
        # MCFClassSolver, hence it needs no fixture of this suite, and it
        # takes the two testers as its arguments
        if (UNIX)
            add_test(NAME MCFBlock_FW_test/regression
                     COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/FW/regression
                             ${CMAKE_CURRENT_BINARY_DIR}/MCFBlock_FW_test
                             ${CMAKE_CURRENT_BINARY_DIR}/MCFBlock_FWmods_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/FW)
        elseif (WIN32)
            add_test(NAME MCFBlock_FW_test/regression
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${CMAKE_CURRENT_SOURCE_DIR}/FW/regression"
                             "$<TARGET_FILE:MCFBlock_FW_test>"
                             "$<TARGET_FILE:MCFBlock_FWmods_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/FW)
        endif ()
    endif ()
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# The Frank-Wolfe test exercises one module more than the rest of the suite,
# and the suite is not skipped when that module is out of the build, so its
# labels are set apart from those of the directory.
if (TEST MCFBlock_FW_test/regression)
    set_tests_properties(MCFBlock_FW_test/regression PROPERTIES
                         LABELS "${SMSPP_TEST_LABELS_MCFBlock};FrankWolfeSolver")
endif ()

# --------------------------------------------------------------------------- #
