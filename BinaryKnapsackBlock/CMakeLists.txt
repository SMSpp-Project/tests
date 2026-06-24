# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(BinaryKnapsackBlock)
    # Solvers
    find_package(MILPSolver)
endif ()

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

    # Formulation cross-checks on randomly generated instances: a single
    # cross-check batch attaches ALL the mathematically equivalent solvers at
    # once and checks they agree (see SolveAll() in test.cpp). batch covers the
    # pure-binary case (every exact solver + the BranchAndXSolver in each
    # exploration mode), batch-mixed the mixed-binary one (the DP/MILP solvers
    # that handle continuous variables). These have no external optimum: they
    # cross-check the solvers against one another over a broad random sweep. The
    # validation against the PUBLISHED Pisinger optima is batch-pisinger.
    foreach (BATCH_NAME batch batch-mixed)
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

    if (UNIX)
        add_test(NAME BinaryKnapsackBlock_test/batch-pisinger
                 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-pisinger
                         ${CMAKE_CURRENT_BINARY_DIR}/BinaryKnapsackBlock_test
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    elseif (WIN32)
        add_test(NAME BinaryKnapsackBlock_test/batch-pisinger
                 # using Git for Windows that includes Git Bash
                 COMMAND "C:/Program Files/Git/bin/bash.exe"
                         "${CMAKE_CURRENT_SOURCE_DIR}/batches-data/batch-pisinger"
                         "$<TARGET_FILE:BinaryKnapsackBlock_test>"
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif ()
    # the cross-check requires the curated data (the raw .csv)
    set_tests_properties(BinaryKnapsackBlock_test/batch-pisinger
                         PROPERTIES FIXTURES_REQUIRED bk_data_ready)

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

# --------------------------------------------------------------------------- #
