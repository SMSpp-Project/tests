# ----- Test ---------------------------------------------------------------- #
if (TARGET SMS++::UCBlock AND
    TARGET SMS++::MILPSolver)

    add_executable(TUDPS_test test.cpp)
    target_link_libraries(TUDPS_test PRIVATE
                          SMS++::MILPSolver
                          SMS++::UCBlock)

    add_test(NAME TUDPS_test
             COMMAND TUDPS_test
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

# --------------------------------------------------------------------------- #
