# ----- Test ---------------------------------------------------------------- #
if (TARGET SMS++::MMCFBlock AND
    TARGET SMS++::UCBlock AND
    TARGET SMS++::BundleSolver AND
    TARGET SMS++::LagrangianDualSolver AND
    TARGET SMS++::MILPSolver)

    add_executable(LDS_MMCF_test test.cpp)
    target_link_libraries(LDS_MMCF_test PRIVATE
                          SMS++::BundleSolver
                          SMS++::MILPSolver
                          SMS++::LagrangianDualSolver
                          SMS++::MMCFBlock
                          SMS++::UCBlock)
endif ()

# --------------------------------------------------------------------------- #
