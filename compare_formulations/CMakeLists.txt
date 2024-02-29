# ----- Test ---------------------------------------------------------------- #
if (TARGET SMS++::UCBlock AND
    TARGET SMS++::MILPSolver)

    add_executable(compare_formulations test.cpp)
    target_link_libraries(compare_formulations PRIVATE
                          SMS++::MILPSolver
                          SMS++::UCBlock)

    add_test(NAME compare_formulations
             COMMAND compare_formulations
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

# --------------------------------------------------------------------------- #
