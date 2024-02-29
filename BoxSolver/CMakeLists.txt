# ----- Test ---------------------------------------------------------------- #
if (TARGET SMS++::MILPSolver)

    add_executable(BoxSolver_test test.cpp)
    target_link_libraries(BoxSolver_test PRIVATE
                          SMS++::MILPSolver)

    # This version replicates the batch file in CMake.
    # This method was preferred because it allows to keep track
    # of the single tests via the CTest application and also
    # avoids copying all the auxiliary files.
    set(what 3)
    foreach (size 2 10 50 100 1000)
        foreach (seed RANGE 0 19)
            add_test(NAME "BoxSolver_test/[${seed},${what},${size}]"
                     COMMAND BoxSolver_test ${seed} ${what} ${size}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endforeach ()
    endforeach ()
endif ()

# --------------------------------------------------------------------------- #
