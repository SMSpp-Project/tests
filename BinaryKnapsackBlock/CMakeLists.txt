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

    smspp_install(BinaryKnapsackBlock_bench)

    # Add a test for each batch file (batch-par and batch-par-nc4 are manual
    # timing benchmarks, so they are intentionally not registered here)
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-greedy"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-bounds"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-core")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/batches/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME BinaryKnapsackBlock_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/BinaryKnapsackBlock_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME BinaryKnapsackBlock_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:BinaryKnapsackBlock_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
    endforeach ()

    # ----- Curated Pisinger knapsack .nc4 cross-check ---------------------- #
    # Solve the curated literature instances (downloaded as txt.tgz and
    # converted to .nc4 by the run_bk2nc4 fixture) with the exact DP solver
    # and a :MILPSolver, and cross-check the optima (see batch-nc4). Disabled
    # by default: enable it only once the curated txt.tgz has been published to
    # the Package Registry (see BinaryKnapsackBlock/data/nc4/readme.txt).
    option(BUILD_BKB_BENCHMARK_TESTS
           "Run the curated Pisinger knapsack .nc4 cross-check tests" OFF)
    if (BUILD_BKB_BENCHMARK_TESTS AND TARGET block_solver)
        # fixture: build bk2nc4 and (down)load + convert the instances
        add_test(NAME run_bk2nc4
                 COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_bk2nc4)
        set_tests_properties(run_bk2nc4
                             PROPERTIES FIXTURES_SETUP bk2nc4_generated)

        # batch-nc4      : exact DP vs :MILPSolver
        # batch-nc4-par  : multi-threaded ParallelDP vs exact serial DP
        # batch-nc4-core : core / non-dominated-state DP vs exact full-table DP
        foreach (NC4_BATCH batch-nc4 batch-nc4-par batch-nc4-core)
            if (UNIX)
                add_test(NAME BinaryKnapsackBlock_test/${NC4_BATCH}
                         COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/batches/${NC4_BATCH} $<TARGET_FILE:block_solver>
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            elseif (WIN32)
                add_test(NAME BinaryKnapsackBlock_test/${NC4_BATCH}
                         # using Git for Windows that includes Git Bash
                         COMMAND "C:/Program Files/Git/bin/bash.exe"
                                 "${CMAKE_CURRENT_SOURCE_DIR}/batches/${NC4_BATCH}" "$<TARGET_FILE:block_solver>"
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            endif ()
            # the cross-check requires the generated .nc4 instances
            set_tests_properties(BinaryKnapsackBlock_test/${NC4_BATCH}
                                 PROPERTIES FIXTURES_REQUIRED bk2nc4_generated)
        endforeach ()
    endif ()
endif ()

# --------------------------------------------------------------------------- #
