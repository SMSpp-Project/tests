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

    # Create a test fixture to fetch the LIBSVM data sets by running
    # extract_svm_libsvm, the target the SVMBlock module declares: the data
    # live with the module, as UCBlock's and CapacitatedFacilityLocationBlock's
    # do, and batch-real is the batch that runs on them
    add_test(NAME extract_svm_libsvm
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target extract_svm_libsvm)
    set_tests_properties(extract_svm_libsvm
                         PROPERTIES FIXTURES_SETUP svm_data_extracted)

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

    # Add a test for each batch file. batch-libsvm and batch-liblinear need
    # LIBSVMSolver and LIBLINEARSolver, which SVMBlock only builds when the
    # library each of them wraps is found: without it the Solver is not in the
    # factory and its BlockSolverConfig would throw, so the batch is left out
    # rather than failing.
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/*")
    if (NOT SVMBlock_HAS_LIBSVM)
        list(REMOVE_ITEM BATCH_FILES
             "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-libsvm")
    endif ()
    if (NOT SVMBlock_HAS_LIBLINEAR)
        list(REMOVE_ITEM BATCH_FILES
             "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-liblinear")
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
                                     PROPERTIES FIXTURES_REQUIRED svm_data_extracted)
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

# --------------------------------------------------------------------------- #
