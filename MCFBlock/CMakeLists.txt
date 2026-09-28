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

    # The Frank-Wolfe decomposition of a father Block whose leaves are the
    # MCFBlock of this suite. The tester is the generic one (../fw_test.cpp):
    # it reads a leaf Block K times, makes the K copies the sub-Block of a
    # father AbstractBlock with a random objective over their Variable, and
    # cross-checks the FrankWolfeSolver that decomposes it, using the :Solver
    # registered to each sub-Block as a Linear Minimization Oracle, against a
    # monolithic :MILPSolver. Which Block is read and which :Solver are
    # attached is the configurations' business, so the same source is built on
    # the ThermalUnitBlock of the UCBlock suite; here the Oracle is a
    # MCFSolver, and its configurations are the `Father*` ones of the suite.
    if (TARGET SMS++::FrankWolfeSolver)

        add_executable(MCFBlock_FW_test ../fw_test.cpp ../common_utils.cpp)
        target_include_directories(MCFBlock_FW_test PRIVATE ..)
        target_link_libraries(MCFBlock_FW_test PRIVATE
                              SMS++::FrankWolfeSolver SMS++::MCFBlock)

        if (TARGET SMS++::MCFClassSolver)
            target_link_libraries(MCFBlock_FW_test PRIVATE
                                  SMS++::MCFClassSolver)
        endif ()

        if (TARGET SMS++::MCFLemonSolver)
            target_link_libraries(MCFBlock_FW_test PRIVATE
                                  SMS++::MCFLemonSolver)
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

        if (TARGET SMS++::MCFLemonSolver)
            target_link_libraries(MCFBlock_FWmods_test PRIVATE
                                  SMS++::MCFLemonSolver)
        endif ()

        if (TARGET SMS++::MILPSolver)
            target_link_libraries(MCFBlock_FWmods_test PRIVATE
                                  SMS++::MILPSolver)
        endif ()

        smspp_install(MCFBlock_FWmods_test)

    endif ()

    # Every battery is posed on a family of instances and runs it with each
    # of the testers that applies to it, which is why it is given all three:
    # the Solver of the Block itself, the Frank-Wolfe decomposition of a
    # father of K copies of it, and what a Modification of the feasible
    # region of a sub-Block does under it.
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/*")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        add_batch_test(MCFBlock_test ${BATCH_NAME}
                       MCFBlock_FW_test MCFBlock_FWmods_test)
        # Make each batch test require the dmx2nc4_generated fixture
        set_tests_properties(MCFBlock_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED dmx2nc4_generated)
    endforeach ()
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# The batteries run one module more than the rest of the suite declares, and
# the suite is not skipped when that module is out of the build, so they are
# labelled with it here rather than in the map of the directory.
if (TARGET SMS++::FrankWolfeSolver)
    get_property(_tests DIRECTORY PROPERTY TESTS)
    foreach (_t IN LISTS _tests)
        if (_t MATCHES "^MCFBlock_test/batches/")
            set_tests_properties(${_t} PROPERTIES
                                 LABELS "${SMSPP_TEST_LABELS_MCFBlock};FrankWolfeSolver")
        endif ()
    endforeach ()
endif ()

# --------------------------------------------------------------------------- #
