# ----- Test ---------------------------------------------------------------- #
if (TARGET SMS++::BinaryKnapsackBlock AND
    TARGET SMS++::MILPSolver)

    add_executable(BinaryKnapsackBlock_test test.cpp)
    target_link_libraries(BinaryKnapsackBlock_test PRIVATE
                          SMS++::MILPSolver
                          SMS++::BinaryKnapsackBlock)
endif ()

# --------------------------------------------------------------------------- #
