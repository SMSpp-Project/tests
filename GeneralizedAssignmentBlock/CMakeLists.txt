# --------------------------------------------------------------------------- #
#    CMake file for tests/GeneralizedAssignmentBlock                          #
# --------------------------------------------------------------------------- #

# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(GeneralizedAssignmentBlock)
    # Solvers
    find_package(MILPSolver)
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(SubGradSolver)
    find_package(BranchAndXSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::GeneralizedAssignmentBlock)

    add_executable(GeneralizedAssignmentBlock_test
                   test.cpp ../common_utils.cpp)
    target_include_directories(GeneralizedAssignmentBlock_test PRIVATE ..)
    target_link_libraries(GeneralizedAssignmentBlock_test
                          PRIVATE SMS++::GeneralizedAssignmentBlock
                                  SMS++::MILPSolver
                                  SMS++::LagrangianDualSolver
                                  SMS++::BundleSolver)

    # the third Solver of BSPar.txt, which the tester leaves out of the
    # cross-check when the build does not have it
    if (TARGET SMS++::SubGradSolver)
        target_link_libraries(GeneralizedAssignmentBlock_test
                              PRIVATE SMS++::SubGradSolver)
    endif ()
    if (TARGET SMS++::BranchAndXSolver)
        target_link_libraries(GeneralizedAssignmentBlock_test
                              PRIVATE SMS++::BranchAndXSolver)
    endif ()

    smspp_install(GeneralizedAssignmentBlock_test)

    # the instances of GeneralizedAssignmentBlock, downloaded and extracted
    # when the tests that read them are run, not when they are built [see the
    # CMakeLists.txt of the module]; the lock is shared by the tests that build
    # a target of the data of the module, which under ctest -j would write the
    # same files at once
    add_test(NAME extract_gab_txt
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                     --target extract_gab_txt)
    set_tests_properties(extract_gab_txt PROPERTIES
                         FIXTURES_SETUP gab_txt_extracted
                         RESOURCE_LOCK gab_data)

    add_batch_test(GeneralizedAssignmentBlock_test batches/batch)
    set_tests_properties(GeneralizedAssignmentBlock_test/batches/batch
                         PROPERTIES FIXTURES_REQUIRED gab_txt_extracted)

endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
