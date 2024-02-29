# ----- Test ---------------------------------------------------------------- #
if (TARGET SMS++::MCFBlock AND
    TARGET SMS++::BinaryKnapsackBlock AND
    TARGET SMS++::BundleSolver AND
    TARGET SMS++::LagrangianDualSolver AND
    TARGET SMS++::CapacitatedFacilityLocationBlock AND
    TARGET SMS++::MILPSolver)

    add_executable(CFL_test test.cpp)
    target_link_libraries(CFL_test PRIVATE
                          SMS++::BundleSolver
                          SMS++::LagrangianDualSolver
                          SMS++::MCFBlock
                          SMS++::MILPSolver
                          SMS++::BinaryKnapsackBlock
                          SMS++::CapacitatedFacilityLocationBlock)
endif ()

# --------------------------------------------------------------------------- #
