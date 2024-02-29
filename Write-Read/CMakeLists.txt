# ----- Tests --------------------------------------------------------------- #

if (TARGET SMS++::MILPSolver)
    add_executable(writeandread_test test.cpp)

    target_link_libraries(writeandread_test PRIVATE
                          SMS++::MILPSolver)
endif ()

# --------------------------------------------------------------------------- #
