# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SMS++)
    find_package(BinaryKnapsackBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BendersDecompositionSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
# The four testers posed on an AbstractBlock: the box-structured one that the
# Lagrangian dual is computed of, the copy of the abstract representation, the
# round trip of a linear program through a file, and the Benders decomposition
# of a two-stage linear program. The first three have their own batch in
# batches/, since each is run with its own tester; the fourth runs its cases
# in a single call.

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

    add_executable(AbstractBlock_mirror_test test_mirror.cpp
                                             ../../common_utils.cpp)
    target_include_directories(AbstractBlock_mirror_test PRIVATE ../..)
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

# BendersDecompositionSolver on a small two-stage linear program written as an
# AbstractBlock, in every way it has of writing a cut, against the optimum a
# :MILPSolver gives of the monolithic model; the master of the convex regime
# is solved by a BundleSolver, hence the tester is built only where both are
if (TARGET SMS++::BendersDecompositionSolver AND TARGET SMS++::BundleSolver
    AND TARGET SMS++::MILPSolver)

    add_executable(AbstractBlock_BDS_test test_bds.cpp)
    target_link_libraries(AbstractBlock_BDS_test PRIVATE
                          SMS++::SMS++
                          SMS++::BendersDecompositionSolver
                          SMS++::BundleSolver
                          SMS++::MILPSolver)

    smspp_install(AbstractBlock_BDS_test)

    add_test(NAME AbstractBlock_BDS_test
             COMMAND AbstractBlock_BDS_test
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# each of the four asks for modules the others do not, and the directory
# declares only what they all need, so the rest is set here
if (TEST AbstractBlock_Box_test/batches/batch-box)
    set_tests_properties(AbstractBlock_Box_test/batches/batch-box PROPERTIES
                         LABELS "${SMSPP_TEST_LABELS_AbstractBlock};BundleSolver;LagrangianDualSolver")
endif ()

if (TEST AbstractBlock_mirror_test/batches/batch-mirror)
    set_tests_properties(AbstractBlock_mirror_test/batches/batch-mirror PROPERTIES
                         LABELS "${SMSPP_TEST_LABELS_AbstractBlock};BinaryKnapsackBlock")
endif ()

if (TEST AbstractBlock_BDS_test)
    set_tests_properties(AbstractBlock_BDS_test PROPERTIES
                         LABELS "${SMSPP_TEST_LABELS_AbstractBlock};BundleSolver;BendersDecompositionSolver")
endif ()

# --------------------------------------------------------------------------- #
