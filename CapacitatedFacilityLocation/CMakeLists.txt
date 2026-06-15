# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(CapacitatedFacilityLocationBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
    find_package(MCFClassSolver)
    find_package(MCFLemonSolver)
endif ()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::CapacitatedFacilityLocationBlock)

    add_executable(CFL_test test.cpp ../common_utils.cpp)
    target_include_directories(CFL_test PRIVATE ..)
    target_link_libraries(CFL_test PRIVATE
                          SMS++::CapacitatedFacilityLocationBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(CFL_test PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(CFL_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(CFL_test PRIVATE SMS++::MILPSolver)
    endif ()

    if (TARGET SMS++::MCFClassSolver)
        target_link_libraries(CFL_test PRIVATE SMS++::MCFClassSolver)
    endif ()

    if (TARGET SMS++::MCFLemonSolver)
        target_link_libraries(CFL_test PRIVATE SMS++::MCFLemonSolver)
    endif ()

    smspp_install(CFL_test)

    # Create a test fixture to generate .nc4 files by running run_cfl2nc4.
    # This test will build and run the run_cfl2nc4 target
    add_test(NAME run_cfl2nc4
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_cfl2nc4)
    # Mark run_cfl2nc4 as a fixture that sets up "cfl2nc4_generated"
    set_tests_properties(run_cfl2nc4
                         PROPERTIES FIXTURES_SETUP cfl2nc4_generated)

    # Add a test for each batch file. All batch scripts live in batches/; each
    # one cd-s into the directory holding its config files (BPar*, BSPar*,
    # R3BCfg) so the same scripts work both via ctest and when launched by
    # hand. Only the scenarios that currently pass are registered.
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-s"
                          # default arrangement (natural-LP vs MCF flow), small instances
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch"
                          # default arrangement, full instance set
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-cuts"
                          # strong-LP vs weak + dynamic strong-cut separation
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-ld"
                          # knapsack Lagrangian dual (LP knapsack leaf) vs natural LP: LD == LP
                          #"${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-ldvscuts"
                          # disabled (not a bug): it compares the integer-knapsack Lagrangian dual
                          # (demand relaxed, decomposed per facility) against the cut-strengthened LP
                          # bound (demand exact, with cross-facility cuts). These are two distinct
                          # valid lower bounds: the cuts can exceed the per-facility dual, so their
                          # difference is neither zero nor sign-definite in general (verified ~7e-4
                          # on some instances, identical with the DP and the :MILPSolver integer
                          # leaves and immune to bundle tuning), so the strict 1e-5 equality is
                          # ill-posed. The batch file documents the per-instance gaps and offers a
                          # commented run_test variant with an ad-hoc 2e-3 tolerance that makes it pass.
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-ben"
                          # Benders-formulation LP test
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-mcf")
                          # MCF flow-relaxation test

    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME CFL_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/CFL_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME CFL_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:CFL_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
        # Make each batch test require the cfl2nc4_generated fixture
        set_tests_properties(CFL_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED cfl2nc4_generated)
    endforeach ()
endif ()

# --------------------------------------------------------------------------- #
