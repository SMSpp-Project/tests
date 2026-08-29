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
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::SVMBlock)

    add_executable(SVM_test test.cpp ../common_utils.cpp)
    target_include_directories(SVM_test PRIVATE ..)
    target_link_libraries(SVM_test PRIVATE SMS++::SVMBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(SVM_test PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(SVM_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(SVM_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(SVM_test)

    # Add a test for each batch file. batch-libsvm needs LIBSVMSolver, which
    # SVMBlock only builds when LIBSVM is found: without it the Solver is not
    # in the factory and its BlockSolverConfig would throw, so the batch is
    # left out rather than failing.
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/*")
    if (NOT SVMBlock_HAS_LIBSVM)
        list(REMOVE_ITEM BATCH_FILES
             "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-libsvm")
    endif ()
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME SVM_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/SVM_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
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

# --------------------------------------------------------------------------- #
