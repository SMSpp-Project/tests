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

    # same driver built with timing on (LOG_LEVEL=1), used by the scaling
    # benchmarks of ParallelDPBinaryKnapsackSolver (see batches/batch-par)
    add_executable(BinaryKnapsackBlock_bench test.cpp ../common_utils.cpp)
    target_include_directories(BinaryKnapsackBlock_bench PRIVATE ..)
    target_compile_definitions(BinaryKnapsackBlock_bench PRIVATE LOG_LEVEL=1)
    target_link_libraries(BinaryKnapsackBlock_bench PRIVATE
                          SMS++::BinaryKnapsackBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(BinaryKnapsackBlock_bench PRIVATE
                              SMS++::MILPSolver)
    endif ()

    if (TARGET SMS++::BranchAndXSolver)
        target_link_libraries(BinaryKnapsackBlock_bench PRIVATE
                              SMS++::BranchAndXSolver)
    endif ()

    smspp_install(BinaryKnapsackBlock_bench)

    # Formulation cross-checks on randomly generated instances: a single
    # "2S"-style batch attaches ALL the mathematically equivalent solvers at
    # once and checks they agree (see SolveAll() in test.cpp). batch covers the
    # pure-binary case (every exact solver + the BranchAndXSolver in each
    # exploration mode), batch-mixed the mixed-binary one (the DP/MILP solvers
    # that handle continuous variables). The data-preparation / Pisinger
    # cross-checks live in batches-data/, the parallel timing benchmarks in
    # batches-bench/ (the latter are NOT registered here).
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

    # ----- Curated Pisinger knapsack .nc4 cross-checks (batches-data/) ----- #
    # Solve the curated literature instances (downloaded as txt.tgz from the
    # Package Registry and converted to .nc4 by the run_bk2nc4 fixture, see
    # BinaryKnapsackBlock/data/nc4/readme.txt) and cross-check the optima:
    # batch-nc4*    are solver-vs-solver (our solvers must agree with one
    #               another), batch-nc4-ref is solver-vs-reference (our optimum
    #               must match the published Pisinger optimum, see
    #               batches-data/batch-nc4-ref).
    if (TARGET block_solver)
        # Create a test fixture to generate .nc4 files by running run_bk2nc4.
        # This test will build and run the run_bk2nc4 target
        add_test(NAME run_bk2nc4
                 COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_bk2nc4)
        # Mark run_bk2nc4 as a fixture that sets up "bk2nc4_generated"
        set_tests_properties(run_bk2nc4
                             PROPERTIES FIXTURES_SETUP bk2nc4_generated)

        # batch-nc4      : exact DP vs :MILPSolver           (solver-vs-solver)
        # batch-nc4-par  : ParallelDP vs exact serial DP     (solver-vs-solver)
        # batch-nc4-core : core DP vs exact full-table DP     (solver-vs-solver)
        # batch-nc4-ref  : our optimum vs the published z    (solver-vs-ref)
        foreach (NC4_BATCH batch-nc4 batch-nc4-par batch-nc4-core batch-nc4-ref)
            if (UNIX)
                add_test(NAME BinaryKnapsackBlock_test/${NC4_BATCH}
                         COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/batches-data/${NC4_BATCH} $<TARGET_FILE:block_solver>
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            elseif (WIN32)
                add_test(NAME BinaryKnapsackBlock_test/${NC4_BATCH}
                         # using Git for Windows that includes Git Bash
                         COMMAND "C:/Program Files/Git/bin/bash.exe"
                                 "${CMAKE_CURRENT_SOURCE_DIR}/batches-data/${NC4_BATCH}" "$<TARGET_FILE:block_solver>"
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            endif ()
            # the cross-check requires the generated .nc4 instances
            set_tests_properties(BinaryKnapsackBlock_test/${NC4_BATCH}
                                 PROPERTIES FIXTURES_REQUIRED bk2nc4_generated)
        endforeach ()
    endif ()
endif ()

# --------------------------------------------------------------------------- #
