# --------------------------------------------------------------------------- #
#    CMake file for tests/SATBlock                                            #
# --------------------------------------------------------------------------- #

# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SATBlock)
    # Solvers
    find_package(MILPSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::SATBlock)

    add_executable(SATBlock_test test.cpp ../common_utils.cpp)
    target_include_directories(SATBlock_test PRIVATE ..)
    target_link_libraries(SATBlock_test PRIVATE SMS++::SATBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(SATBlock_test PRIVATE SMS++::MILPSolver)
    endif ()
    if (TARGET SMS++::BranchAndXSolver)
        target_link_libraries(SATBlock_test PRIVATE SMS++::BranchAndXSolver)
    endif ()
    # the heuristic of the barrier of the slacks [see IBCfg.txt]
    if (TARGET SMS++::FrankWolfeSolver)
        target_link_libraries(SATBlock_test PRIVATE SMS++::FrankWolfeSolver)
    endif ()
    # the Lagrangian dual of a SATBlock with a structure [see batch-structure]
    if (TARGET SMS++::LagrangianDualSolver AND TARGET SMS++::BundleSolver)
        target_link_libraries(SATBlock_test PRIVATE
                              SMS++::LagrangianDualSolver SMS++::BundleSolver)
    endif ()

    smspp_install(SATBlock_test)

    # the instances of SATBlock, downloaded and extracted when the tests that
    # read them are run, not when they are built [see the CMakeLists.txt of the
    # module]; the lock is shared by the tests that build a target of the data
    # of the module, which under ctest -j would write the same files at once
    foreach (fmt cnf wcnf)
        add_test(NAME extract_sat_${fmt}
                 COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                         --target extract_sat_${fmt})
        set_tests_properties(extract_sat_${fmt} PROPERTIES
                             FIXTURES_SETUP sat_${fmt}_extracted
                             RESOURCE_LOCK sat_data)
    endforeach ()

    # the generator of the instances made of groups of clauses, when it is a
    # target of this build, otherwise the one installed [see batch-groups]
    set(_satgen smspp_satgen)
    if (TARGET satgen)
        set(_satgen "$<TARGET_FILE:satgen>")
    endif ()

    # each battery with its BlockSolverConfig
    foreach (BATCH batch-mse:BSPar.txt batch-groups:BSPar.txt
                   batch-structure:BSPar-LD.txt)
        string(REPLACE ":" ";" BATCH "${BATCH}")
        list(GET BATCH 0 BATCH_NAME)
        list(GET BATCH 1 BATCH_PAR)
        if (UNIX)
            add_test(NAME SATBlock_test/batches/${BATCH_NAME}
                     COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/batches/${BATCH_NAME}
                             ${CMAKE_CURRENT_BINARY_DIR}/SATBlock_test
                             ${BATCH_PAR} "" ${_satgen}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME SATBlock_test/batches/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${CMAKE_CURRENT_SOURCE_DIR}/batches/${BATCH_NAME}"
                             "$<TARGET_FILE:SATBlock_test>"
                             ${BATCH_PAR} "" ${_satgen}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
    endforeach ()
    set_tests_properties(SATBlock_test/batches/batch-mse
                         PROPERTIES FIXTURES_REQUIRED sat_wcnf_extracted)

endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
