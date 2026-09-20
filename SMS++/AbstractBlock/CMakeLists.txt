# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SMS++)
    find_package(BinaryKnapsackBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
# The three testers posed on an AbstractBlock: the box-structured one that the
# Lagrangian dual is computed of, the copy of the abstract representation, and
# the round trip of a linear program through a file. Each has its own batch in
# batches/, since each is run with its own tester.

# the box-structured AbstractBlock: k sub-Block with n variables each, only
# box constraints and a separable Objective, solved by a LagrangianDualSolver
# (whose LagBFunction are solved by a BoxSolver) against a :MILPSolver
if (TARGET SMS++::LagrangianDualSolver)

    add_executable(AbstractBlock_Box_test test_box.cpp ../../common_utils.cpp)
    target_include_directories(AbstractBlock_Box_test PRIVATE ../..)
    target_link_libraries(AbstractBlock_Box_test PRIVATE
                          SMS++::LagrangianDualSolver)

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(AbstractBlock_Box_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(AbstractBlock_Box_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(AbstractBlock_Box_test)

    add_batch_test(AbstractBlock_Box_test batches/batch-box)
endif ()

# AbstractBlock::mirror(), i.e., the copy of the abstract representation that
# any Block has without a line written for it: the copy is solved by the same
# :MILPSolver as the original, takes the solution back to it and follows it
# when it changes
if (TARGET SMS++::BinaryKnapsackBlock AND TARGET SMS++::MILPSolver)

    add_executable(AbstractBlock_mirror_test test_mirror.cpp)
    target_link_libraries(AbstractBlock_mirror_test PRIVATE
                          SMS++::SMS++
                          SMS++::BinaryKnapsackBlock
                          SMS++::MILPSolver)

    smspp_install(AbstractBlock_mirror_test)

    add_batch_test(AbstractBlock_mirror_test batches/batch-mirror)
endif ()

# AbstractBlock::read_lp() and AbstractBlock::read_mps(): a random linear
# program is written by the :MILPSolver attached to it, read back into a
# second AbstractBlock and solved again, the two optima having to agree
if (TARGET SMS++::MILPSolver)

    add_executable(AbstractBlock_readwrite_test test_readwrite.cpp
                                                ../../common_utils.cpp)
    target_include_directories(AbstractBlock_readwrite_test PRIVATE ../..)
    target_link_libraries(AbstractBlock_readwrite_test PRIVATE
                          SMS++::SMS++ SMS++::MILPSolver)

    smspp_install(AbstractBlock_readwrite_test)

    add_batch_test(AbstractBlock_readwrite_test batches/batch-readwrite)
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# each of the three asks for one module more than the other two, and the
# directory declares only what they all need, so the rest is set here
if (TEST AbstractBlock_Box_test/batches/batch-box)
    set_tests_properties(AbstractBlock_Box_test/batches/batch-box PROPERTIES
                         LABELS "${SMSPP_TEST_LABELS_AbstractBlock};BundleSolver;LagrangianDualSolver")
endif ()

if (TEST AbstractBlock_mirror_test/batches/batch-mirror)
    set_tests_properties(AbstractBlock_mirror_test/batches/batch-mirror PROPERTIES
                         LABELS "${SMSPP_TEST_LABELS_AbstractBlock};BinaryKnapsackBlock")
endif ()

# --------------------------------------------------------------------------- #
