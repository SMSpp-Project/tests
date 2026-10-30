# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SVMBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
    find_package(BendersDecompositionSolver)
    find_package(FrankWolfeSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::SVMBlock)

    # the instances of SVMBlock, downloaded and extracted when the tests that
    # read them are run, not when they are built [see the CMakeLists.txt of the
    # module]; the lock is shared by the tests that build a target of the data
    # of the module, which under ctest -j would write the same files at once
    add_test(NAME extract_svm_libsvm
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                     --target extract_svm_libsvm)
    set_tests_properties(extract_svm_libsvm PROPERTIES
                         FIXTURES_SETUP svm_libsvm_extracted
                         RESOURCE_LOCK svm_data)

    add_executable(SVM_test test.cpp ../common_utils.cpp)
    target_include_directories(SVM_test PRIVATE ..)
    target_link_libraries(SVM_test PRIVATE SMS++::SVMBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(SVM_test PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(SVM_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::BendersDecompositionSolver)
        target_link_libraries(SVM_test PRIVATE
                              SMS++::BendersDecompositionSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(SVM_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(SVM_test)

    # the two dual decompositions of one training problem, the consensus one
    # under a Lagrangian Solver and the Benders one under the generic Benders
    # Solver, both cross-checked against the ad hoc solver of the module: what
    # is tested is that the two structures set_structure() deals out describe
    # the same instance, hence it belongs to the suite of the Block and not to
    # that of either Solver
    if (TARGET SMS++::BendersDecompositionSolver AND
        TARGET SMS++::LagrangianDualSolver)
        add_executable(SVM_bds_test test_bds.cpp)
        target_link_libraries(SVM_bds_test PRIVATE
                              SMS++::SVMBlock
                              SMS++::BendersDecompositionSolver
                              SMS++::LagrangianDualSolver
                              SMS++::BundleSolver
                              SMS++::MILPSolver)

        smspp_install(SVM_bds_test)

        add_test(NAME SVM_bds_test
                 COMMAND SVM_bds_test
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif ()

    # FrankWolfeSolver on two problems built out of the data sets of this
    # suite: the least-squares fit in Euclidean balls, a feasible region that
    # is not a polytope, whose oracle is the closed-form BallSolver of the
    # tester, and the Wolfe dual on a box, whose oracle is BoxSolver
    if (TARGET SMS++::FrankWolfeSolver)
        add_executable(SVM_FW_test test_fw.cpp ../common_utils.cpp)
        target_include_directories(SVM_FW_test PRIVATE ..)
        target_link_libraries(SVM_FW_test PRIVATE
                              SMS++::SVMBlock
                              SMS++::FrankWolfeSolver)
        if (TARGET SMS++::MILPSolver)
            target_link_libraries(SVM_FW_test PRIVATE SMS++::MILPSolver)
        endif ()

        smspp_install(SVM_FW_test)

        if (UNIX)
            add_test(NAME SVM_FW_test/batch-fw
                     COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-fw
                             ${CMAKE_CURRENT_BINARY_DIR}/SVM_FW_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            set_tests_properties(SVM_FW_test/batch-fw PROPERTIES
                                 FIXTURES_REQUIRED svm_libsvm_extracted)
        endif ()
    endif ()

    # Add a test for each batch file. batch-libsvm and batch-liblinear need
    # LIBSVMSolver and LIBLINEARSolver, which SVMBlock only builds when the
    # library each of them wraps is found: without it the Solver is not in the
    # factory and its BlockSolverConfig would throw, so the batch is left out
    # rather than failing.
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/*")
    # batch-fw is not run by SVM_test but by SVM_FW_test, see above
    list(REMOVE_ITEM BATCH_FILES
         "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-fw")
    if (NOT SVMBlock_HAS_LIBSVM)
        list(REMOVE_ITEM BATCH_FILES
             "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-libsvm")
    endif ()
    if (NOT SVMBlock_HAS_LIBLINEAR)
        list(REMOVE_ITEM BATCH_FILES
             "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-liblinear")
    endif ()
    # the same for the Benders form, which is the one the Benders Solver asks
    # for and which is therefore only checked where that Solver is built: it
    # is not among the labels of this suite, the rest of which runs without it
    if (NOT TARGET SMS++::BendersDecompositionSolver)
        list(REMOVE_ITEM BATCH_FILES
             "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-bds")
    endif ()
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME SVM_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/SVM_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            # the batch that runs on the LIBSVM data sets needs them there
            if (BATCH_NAME MATCHES "batch-real")
                set_tests_properties(SVM_test/${BATCH_NAME}
                                     PROPERTIES FIXTURES_REQUIRED svm_libsvm_extracted)
            endif ()
        elseif (WIN32)
            add_test(NAME SVM_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:SVM_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
    endforeach ()
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# The battery of FrankWolfeSolver runs one module more than the rest of the
# suite declares, and the suite is not skipped when that module is out of the
# build, so it is labelled with it here rather than in the map of the
# directory.
if (TARGET SMS++::SVMBlock AND TARGET SMS++::FrankWolfeSolver AND UNIX)
    set_tests_properties(SVM_FW_test/batch-fw PROPERTIES LABELS
                         "${SMSPP_TEST_LABELS_SVMBlock};FrankWolfeSolver")
endif ()

# --------------------------------------------------------------------------- #
