# ----- Test ---------------------------------------------------------------- #
if (TARGET MMCFClass AND
    TARGET SMS++::MMCFBlock AND
    TARGET SMS++::BundleSolver AND
    TARGET SMS++::MILPSolver)

    add_subdirectory(MMCFClass)
    add_executable(MMCF_test)

    target_sources(MMCF_test PRIVATE main.cpp)

    target_link_libraries(MMCF_test PRIVATE
                          SMS++::BundleSolver
                          SMS++::MILPSolver
                          SMS++::MMCFBlock
                          MMCFClass)
endif ()

# --------------------------------------------------------------------------- #
