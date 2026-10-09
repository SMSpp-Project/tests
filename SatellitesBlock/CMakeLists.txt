# --------------------------------------------------------------------------- #
#    CMake file for tests/SatellitesBlock                                     #
# --------------------------------------------------------------------------- #

# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SatellitesBlock)
    # Solvers
    find_package(MILPSolver)
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::SatellitesBlock)

    add_executable(SatellitesBlock_test test.cpp ../common_utils.cpp)
    target_include_directories(SatellitesBlock_test PRIVATE ..)
    target_link_libraries(SatellitesBlock_test
                          PRIVATE SMS++::SatellitesBlock
                                  SMS++::MILPSolver
                                  SMS++::LagrangianDualSolver
                                  SMS++::BundleSolver)

    smspp_install(SatellitesBlock_test)

    add_batch_test(SatellitesBlock_test batches/batch)

endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
