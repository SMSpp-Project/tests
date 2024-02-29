# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::MCFBlock AND
    TARGET SMS++::MILPSolver)

    add_executable(MCF_MILP_test test.cpp)
    target_link_libraries(MCF_MILP_test PRIVATE
                          SMS++::MILPSolver
                          SMS++::MCFBlock)

    add_test(NAME MCF_MILP_test
             COMMAND MCF_MILP_test
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

# --------------------------------------------------------------------------- #
