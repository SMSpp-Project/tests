# ----- Test ---------------------------------------------------------------- #
if (TARGET SMS++::BundleSolver AND
    TARGET SMS++::LagrangianDualSolver)

    add_executable(LDS_Box_test test.cpp)
    target_link_libraries(LDS_Box_test PRIVATE
                          SMS++::LagrangianDualSolver
                          SMS++::BundleSolver)

    # This version replicates the batch file in CMake.
    # This method was preferred because it allows to keep track
    # of the single tests via the CTest application and also
    # avoids copying all the auxiliary files.
    set(what 15)
    foreach (nvar 2 10 50)
        foreach (nson 2 9 20)
            foreach (dens 0.05 0.1)
                foreach (seed RANGE 0 19)
                    add_test(NAME "LDS_Box_test/[${seed},${what},${nvar},${nson},${dens}]"
                             COMMAND LDS_Box_test ${seed} ${what} ${nvar} ${nson} ${dens}
                             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
                endforeach ()
            endforeach ()
        endforeach ()
    endforeach ()
endif ()

# --------------------------------------------------------------------------- #
