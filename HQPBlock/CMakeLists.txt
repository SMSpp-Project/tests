# --------------------------------------------------------------------------- #
#    CMake file for tests/HQPBlock                                            #
# --------------------------------------------------------------------------- #

# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(HQPBlock)
    # Solvers
    find_package(MILPSolver)
    find_package(BendersDecompositionSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
add_executable(HQPBlock_test test.cpp ../common_utils.cpp)
target_include_directories(HQPBlock_test PRIVATE ..)
target_link_libraries(HQPBlock_test PRIVATE SMS++::HQPBlock
                                            SMS++::MILPSolver)

# the inner Solver of the Benders decomposition of the HQPDistBlock [see
# HQPDistBSCfg-bds.txt], which the tester leaves out when the build does not
# have it
if (TARGET SMS++::BendersDecompositionSolver)
    target_link_libraries(HQPBlock_test
                          PRIVATE SMS++::BendersDecompositionSolver)
endif ()

smspp_install(HQPBlock_test)

# the hierarchies built by the tester
add_batch_test(HQPBlock_test batches/batch-generated)

# the instances of HQPBlock, downloaded and extracted when the tests that
# read them are run, not when they are built [see the CMakeLists.txt of the
# module]; the lock is shared by the tests that build a target of the data
# of the module, which under ctest -j would write the same files at once
add_test(NAME extract_hqp_nc4
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                 --target extract_hqp_nc4)
set_tests_properties(extract_hqp_nc4 PROPERTIES
                     FIXTURES_SETUP hqp_nc4_extracted
                     RESOURCE_LOCK hqp_data)

add_batch_test(HQPBlock_test batches/batch-radial)
set_tests_properties(HQPBlock_test/batches/batch-radial PROPERTIES
                     FIXTURES_REQUIRED hqp_nc4_extracted)

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
