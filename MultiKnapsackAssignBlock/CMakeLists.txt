# --------------------------------------------------------------------------- #
#    CMake file for tests/MultiKnapsackAssignBlock                            #
# --------------------------------------------------------------------------- #

# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(MultiKnapsackAssignBlock)
    # Solvers
    find_package(MILPSolver)
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::MultiKnapsackAssignBlock)

    add_executable(MultiKnapsackAssignBlock_test test.cpp ../common_utils.cpp)
    target_include_directories(MultiKnapsackAssignBlock_test PRIVATE ..)
    target_link_libraries(MultiKnapsackAssignBlock_test
                          PRIVATE SMS++::MultiKnapsackAssignBlock
                                  SMS++::MILPSolver
                                  SMS++::LagrangianDualSolver
                                  SMS++::BundleSolver)

    smspp_install(MultiKnapsackAssignBlock_test)

    # the instances of MultiKnapsackAssignBlock, downloaded and extracted when
    # the tests that read them are run, not when they are built [see the
    # CMakeLists.txt of the module]; the lock is shared by the tests that build
    # a target of the data of the module, which under ctest -j would write the
    # same files at once
    add_test(NAME extract_mkab_txt
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                     --target extract_mkab_txt)
    set_tests_properties(extract_mkab_txt PROPERTIES
                         FIXTURES_SETUP mkab_txt_extracted
                         RESOURCE_LOCK mkab_data)

    add_batch_test(MultiKnapsackAssignBlock_test batches/batch)
    set_tests_properties(MultiKnapsackAssignBlock_test/batches/batch
                         PROPERTIES FIXTURES_REQUIRED mkab_txt_extracted)

endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
