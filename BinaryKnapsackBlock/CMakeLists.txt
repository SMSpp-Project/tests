# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(BinaryKnapsackBlock)
    # Solvers
    find_package(MILPSolver)
    find_package(FrankWolfeSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::BinaryKnapsackBlock)

    add_executable(BinaryKnapsackBlock_test test.cpp ../common_utils.cpp)
    target_include_directories(BinaryKnapsackBlock_test PRIVATE ..)
    target_link_libraries(BinaryKnapsackBlock_test PRIVATE
                          SMS++::BinaryKnapsackBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(BinaryKnapsackBlock_test PRIVATE
                              SMS++::MILPSolver)
    endif ()

    if (TARGET SMS++::BranchAndXSolver)
        target_link_libraries(BinaryKnapsackBlock_test PRIVATE
                              SMS++::BranchAndXSolver)
    endif ()

    smspp_install(BinaryKnapsackBlock_test)

    # The Frank-Wolfe decomposition of a father Block whose leaves are the
    # BinaryKnapsackBlock of this suite, each with its DPBinaryKnapsackSolver
    # as the Linear Minimization Oracle: the dynamic programme minimizes over
    # the knapsack whatever linear objective the decomposition writes into it,
    # which is exactly what an oracle is asked for. The tester is the generic
    # one (../fw_test.cpp), the same the suites of MCFBlock and of UCBlock
    # build on their own Block, and its configurations are the `Father*` ones
    # of the suite. The runs are in batches/batch-pisinger, on the netCDF
    # instances that BinaryKnapsackBlock/tools/batch writes out of the same
    # curated archive the rest of that battery reads.
    if (TARGET SMS++::FrankWolfeSolver)

        add_executable(BinaryKnapsackBlock_FW_test
                       ../fw_test.cpp ../common_utils.cpp)
        target_include_directories(BinaryKnapsackBlock_FW_test PRIVATE ..)
        target_link_libraries(BinaryKnapsackBlock_FW_test PRIVATE
                              SMS++::FrankWolfeSolver
                              SMS++::BinaryKnapsackBlock)

        if (TARGET SMS++::MILPSolver)
            target_link_libraries(BinaryKnapsackBlock_FW_test PRIVATE
                                  SMS++::MILPSolver)
        endif ()

        smspp_install(BinaryKnapsackBlock_FW_test)

        # fixture: write the netCDF instances out of the curated txt
        add_test(NAME run_bk2nc4
                 COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_bk2nc4)
        set_tests_properties(run_bk2nc4
                             PROPERTIES FIXTURES_SETUP bk_nc4_generated)

    endif ()

    # Formulation cross-checks on randomly generated instances: a single
    # cross-check batch attaches ALL the mathematically equivalent solvers at
    # once and checks they agree (see SolveAll() in test.cpp). batch covers the
    # pure-binary case (every exact solver + the BranchAndXSolver in each
    # exploration mode), batch-mixed the mixed-binary one (the DP/MILP solvers
    # that handle continuous variables). These have no external optimum: they
    # cross-check the solvers against one another over a broad random sweep. The
    # validation against the PUBLISHED Pisinger optima is batch-pisinger.
    # batch-negative is the same cross-check on the two regimes the sweep of
    # batch does not reach: mostly negative weights and profits, and small
    # instances re-solved many times.
    foreach (BATCH_NAME batch batch-mixed batch-negative)
        if (UNIX)
            add_test(NAME BinaryKnapsackBlock_test/${BATCH_NAME}
                     COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/batches/${BATCH_NAME}
                             ${CMAKE_CURRENT_BINARY_DIR}/BinaryKnapsackBlock_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME BinaryKnapsackBlock_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${CMAKE_CURRENT_SOURCE_DIR}/batches/${BATCH_NAME}"
                             "$<TARGET_FILE:BinaryKnapsackBlock_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
    endforeach ()

    # ----- Curated Pisinger knapsack validation (batches/batch-pisinger) --- #
    # batch-pisinger drives BinaryKnapsackBlock_test over the curated .csv
    # (which carry, per instance, the published optimum z): the test loads every
    # instance of a class and attaches ALL the equivalent solvers at once (the
    # exact MILP / DP / core DP / BranchAndXSolver, with the greedy relaxation
    # bracketing), so a single in-process pass checks them against one another
    # (solver-vs-solver) AND against z (solver-vs-reference). The curated data
    # is downloaded and extracted by the fetch_bk_data fixture.
    # fixture: download the curated txt.tgz from the Package Registry and
    # extract it (provides the raw .csv carrying the published optima)
    add_test(NAME fetch_bk_data
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target fetch_bk_data)
    set_tests_properties(fetch_bk_data
                         PROPERTIES FIXTURES_SETUP bk_data_ready)

    # the battery is given the Frank-Wolfe tester as well, when it is in the
    # build, as its fifth argument: the same knapsacks are then also solved by
    # the decomposition of a father of K copies of them
    set(_bk_fw "")
    if (TARGET BinaryKnapsackBlock_FW_test)
        set(_bk_fw "$<TARGET_FILE:BinaryKnapsackBlock_FW_test>")
    endif ()

    if (UNIX)
        add_test(NAME BinaryKnapsackBlock_test/batch-pisinger
                 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-pisinger
                         ${CMAKE_CURRENT_BINARY_DIR}/BinaryKnapsackBlock_test
                         "" "" "" ${_bk_fw}
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    elseif (WIN32)
        add_test(NAME BinaryKnapsackBlock_test/batch-pisinger
                 # using Git for Windows that includes Git Bash
                 COMMAND "C:/Program Files/Git/bin/bash.exe"
                         "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-pisinger"
                         "$<TARGET_FILE:BinaryKnapsackBlock_test>"
                         "" "" "" ${_bk_fw}
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif ()
    # the cross-check requires the curated data (the raw .csv), and the
    # decomposition the netCDF instances written out of it
    if (TARGET BinaryKnapsackBlock_FW_test)
        set_tests_properties(BinaryKnapsackBlock_test/batch-pisinger
                             PROPERTIES FIXTURES_REQUIRED "bk_data_ready;bk_nc4_generated")
    else ()
        set_tests_properties(BinaryKnapsackBlock_test/batch-pisinger
                             PROPERTIES FIXTURES_REQUIRED bk_data_ready)
    endif ()

    # ----- Jooken hard-instance validation (batches/batch-jooken) ---------- #
    # Same architecture as batch-pisinger, on the Jooken-Leyman-De Causmaecker
    # hard 0-1 knapsack instances (curated into data/jooken by the fetch_jooken_
    # data fixture, each .csv carrying the published optimum z). These instances
    # are designed to defeat branch-and-bound, so the toughest exact solvers we
    # have (core DP, CPLEX, the BranchAndXSolver) are run one per process under a
    # time / memory cap: the batch is GREEN unless a solver FINISHES with the
    # wrong optimum; a TIMEOUT or OOM on these hard instances is expected and
    # tolerated. The ctest run is bounded to the tractable (1e6-capacity) bucket
    # with a short per-run limit; larger buckets are for the offline benchmark.
    add_test(NAME fetch_jooken_data
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target fetch_jooken_data)
    set_tests_properties(fetch_jooken_data
                         PROPERTIES FIXTURES_SETUP jooken_data_ready)

    if (UNIX)
        add_test(NAME BinaryKnapsackBlock_test/batch-jooken
                 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-jooken
                         ${CMAKE_CURRENT_BINARY_DIR}/BinaryKnapsackBlock_test
                         20 8388608 1000000
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    elseif (WIN32)
        add_test(NAME BinaryKnapsackBlock_test/batch-jooken
                 COMMAND "C:/Program Files/Git/bin/bash.exe"
                         "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-jooken"
                         "$<TARGET_FILE:BinaryKnapsackBlock_test>"
                         20 8388608 1000000
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif ()
    # the cross-check requires the curated Jooken data
    set_tests_properties(BinaryKnapsackBlock_test/batch-jooken
                         PROPERTIES FIXTURES_REQUIRED jooken_data_ready)
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# One battery runs a module more than the rest of the suite declares, and the
# suite is not skipped when that module is out of the build, so it is labelled
# with it here rather than in the map of the directory.
if (TARGET SMS++::FrankWolfeSolver)
    set_tests_properties(BinaryKnapsackBlock_test/batch-pisinger PROPERTIES
                         LABELS "${SMSPP_TEST_LABELS_BinaryKnapsackBlock};FrankWolfeSolver")
endif ()

# --------------------------------------------------------------------------- #
