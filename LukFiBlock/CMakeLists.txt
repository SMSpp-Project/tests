# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(LukFiBlock)
    # Solvers
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::LukFiBlock)

    add_executable(LukFi_test main.cpp)
    target_link_libraries(LukFi_test PRIVATE SMS++::LukFiBlock)

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(LukFi_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(LukFi_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(LukFi_test)
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
