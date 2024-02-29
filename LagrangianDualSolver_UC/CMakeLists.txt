# ----- Test ---------------------------------------------------------------- #
if (TARGET SMS++::UCBlock AND
    TARGET SMS++::BundleSolver AND
    TARGET SMS++::LagrangianDualSolver)

    add_executable(LDS_UC_test test.cpp)
    target_link_libraries(LDS_UC_test PRIVATE
                          SMS++::UCBlock
                          SMS++::LagrangianDualSolver
                          SMS++::BundleSolver)

    # This version replicates the batch file in CMake.
    # This method was preferred because it allows to keep track
    # of the single tests via the CTest application and also
    # avoids copying all the auxiliary files.
    foreach (size 10 20 50 75 100 200)
        foreach (seed RANGE 1 5)
            add_test(NAME "LDS_UC_test/[${size},${seed}]"
                     COMMAND LDS_UC_test ../../UCBlock/netCDF_files/UC_Data/T-Ramp/${size}_0_${seed}_w.nc4
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endforeach ()
    endforeach ()
endif ()

# --------------------------------------------------------------------------- #
